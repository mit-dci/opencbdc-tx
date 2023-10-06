// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "uhs/atomizer/atomizer/format.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

#include <leveldb/write_batch.h>
#include <utility>

namespace cbdc::archiver {

    leveldbWriteOptions::leveldbWriteOptions(bool do_sync) {
        // Set base class member:
        sync = do_sync;
    }

    const leveldbWriteOptions controller::m_write_options{true};

    controller::controller(uint32_t archiver_id,
                           cbdc::config::options opts,
                           std::shared_ptr<logging::log> log,
                           size_t max_samples)
        : m_archiver_id(archiver_id),
          m_opts(std::move(opts)),
          m_logger(std::move(log)),
          m_max_samples(max_samples) {}

    controller::~controller() {
        m_atomizer_network.close();
        m_archiver_network.close();

        m_running = false;

        if(m_atomizer_handler_thread.joinable()) {
            m_atomizer_handler_thread.join();
        }

        if(m_archiver_server.joinable()) {
            m_archiver_server.join();
        }
    }

    auto controller::init() -> bool {
        if(!init_leveldb()) {
            return false;
        }
        if(!init_best_block()) {
            return false;
        }
        if(!init_sample_collection()) {
            return false;
        }
        if(!init_atomizer_connection()) {
            return false;
        }
        if(!init_archiver_server()) {
            return false;
        }
        return true;
    }

    auto controller::init_leveldb() -> bool {
        leveldb::Options opt;
        opt.create_if_missing = true;
        opt.paranoid_checks = true;
        opt.compression = leveldb::kNoCompression;

        leveldb::DB* db_ptr{};
        const auto res
            = leveldb::DB::Open(opt,
                                m_opts.m_archiver_db_dirs[m_archiver_id],
                                &db_ptr);
        if(!res.ok()) {
            m_logger->error(res.ToString());
            return false;
        }
        m_db.reset(db_ptr);
        return true;
    }

    auto controller::init_best_block() -> bool {
        std::string bestblock_val;
        const auto blk_res
            = m_db->Get(m_read_options, m_bestblock_key, &bestblock_val);
        if(blk_res.IsNotFound()) {
            bestblock_val = std::to_string(0);
            const auto wr_res
                = m_db->Put(m_write_options, m_bestblock_key, bestblock_val);
            if(!wr_res.ok()) {
                return false;
            }
        }
        m_best_height = static_cast<uint64_t>(std::stoul(bestblock_val));
        return true;
    }

    auto controller::init_sample_collection() -> bool {
        m_tp_sample_file.open("tp_samples.txt");
        if(!m_tp_sample_file.good()) {
            return false;
        }
        m_last_block_time = std::chrono::high_resolution_clock::now();
        m_sample_collection_active = true;
        return true;
    }

    auto controller::init_atomizer_connection() -> bool {
        m_atomizer_network.cluster_connect(m_opts.m_atomizer_endpoints, false);
        if(!m_atomizer_network.connected_to_one()) {
            m_logger->warn("Failed to connect to any atomizers.");
        }

        m_atomizer_handler_thread
            = m_atomizer_network.start_handler([&](auto&& pkt) {
                  return atomizer_handler(std::forward<decltype(pkt)>(pkt));
              });

        return true;
    }

    auto controller::init_archiver_server() -> bool {
        auto as = m_archiver_network.start_server(
            m_opts.m_archiver_endpoints[m_archiver_id],
            [&](auto&& pkt) {
                return server_handler(std::forward<decltype(pkt)>(pkt));
            });

        if(!as.has_value()) {
            m_logger->error("Failed to establish shard server.");
            return false;
        }

        m_archiver_server = std::move(as.value());

        return true;
    }

    auto controller::running() const -> bool {
        return m_running;
    }

    auto controller::server_handler(cbdc::network::message_t&& pkt)
        -> std::optional<cbdc::buffer> {
        auto req = from_buffer<request>(*pkt.m_pkt);
        if(!req.has_value()) {
            m_logger->error("Invalid request packet");
            return std::nullopt;
        }
        auto blk = get_block(req.value());
        return cbdc::make_buffer(blk);
    }

    auto controller::atomizer_handler(cbdc::network::message_t&& pkt)
        -> std::optional<cbdc::buffer> {
        auto blk = from_buffer<atomizer::block>(*pkt.m_pkt);
        if(!blk.has_value()) {
            m_logger->error("Invalid request packet");
            return std::nullopt;
        }
        if((m_max_samples != 0) && (m_samples >= m_max_samples)) {
            m_running = false;
            return std::nullopt;
        }
        digest_block(blk.value());
        return std::nullopt;
    }

    auto controller::best_block_height() const -> uint64_t {
        return m_best_height;
    }

    void controller::digest_block(const cbdc::atomizer::block& blk) {
        if(m_best_height == 0) {
            // This is the first call to digest_block. Check if there is
            // already a best height value in the database and set it if so.
            std::string bestblock_val;
            const auto blk_res
                = m_db->Get(m_read_options, m_bestblock_key, &bestblock_val);
            if(blk_res.ok()) {
                m_best_height
                    = static_cast<uint64_t>(std::stoul(bestblock_val));
            }
        }

        cbdc::atomizer::block next_blk;
        bool digest_next{false};
        {
            if(blk.m_height <= m_best_height) {
                m_logger->warn("Not processing duplicate block h:",
                               blk.m_height);
                return;
            }

            if(blk.m_height != m_best_height + 1) {
                // Not contiguous, check prev block isn't deferred already
                auto it = m_deferred.find(blk.m_height - 1);
                if(it == m_deferred.end()) {
                    // Request previous block from atomizer cluster
                    request_block(blk.m_height - 1);
                }
                m_deferred.emplace(blk.m_height, blk);
                return;
            }

            leveldb::WriteBatch batch;

            m_logger->trace("Digesting block ", blk.m_height, "... ");

            auto blk_bytes = make_buffer(blk);
            leveldb::Slice blk_slice(blk_bytes.c_str(), blk_bytes.size());

            const auto height_str = std::to_string(blk.m_height);

            batch.Put(height_str, blk_slice);
            batch.Put(m_bestblock_key, height_str);
            m_best_height++;

            const auto res = m_db->Write(m_write_options, &batch);
            assert(res.ok());

            m_logger->trace("Digested block ", blk.m_height);
            if(m_sample_collection_active) {
                const auto old_block_time = m_last_block_time;
                m_last_block_time = std::chrono::high_resolution_clock::now();
                const auto s_since_last_block = std::chrono::duration<double>(
                    m_last_block_time - old_block_time);
                const auto tx_throughput
                    = static_cast<double>(blk.m_transactions.size())
                    / s_since_last_block.count();

                m_tp_sample_file << tx_throughput << std::endl;
                m_samples++;
            }

            // Tell the atomizer cluster to prune all blocks <
            // m_best_height
            request_prune(m_best_height);

            auto it = m_deferred.find(blk.m_height + 1);
            if(it != m_deferred.end()) {
                next_blk = it->second;
                digest_next = true;
            }

            m_deferred.erase(blk.m_height);
        }

        if(digest_next) {
            // TODO: this can recurse back to genesis. In a long-running system
            // we'll want an alternative method of building a new archiver node
            // and limit the depth of recursion here.
            digest_block(next_blk);
        }
    }

    auto controller::get_block(uint64_t height)
        -> std::optional<cbdc::atomizer::block> {
        m_logger->trace(__func__, "(", height, ")");
        std::string height_str = std::to_string(height);
        std::string blk_str;
        // Assume blocks with 100k 256-byte transactions.
        static constexpr const auto expected_entry_sz = 256 * 100000;
        blk_str.reserve(expected_entry_sz);
        auto s = m_db->Get(m_read_options, height_str, &blk_str);

        if(!s.ok()) {
            m_logger->warn("block", height, "not found");
            m_logger->trace("end", __func__);

            return std::nullopt;
        }

        auto buf = cbdc::buffer();
        buf.append(blk_str.data(), blk_str.size());
        auto blk = from_buffer<atomizer::block>(buf);
        assert(blk.has_value());
        m_logger->trace("found block", height, "-", blk.value().m_height);
        return blk.value();
    }

    void controller::request_block(uint64_t height) {
        m_logger->trace("Requesting block", height);
        auto req = atomizer::get_block_request{height};
        auto pkt = make_shared_buffer(atomizer::request{req});
        if(!m_atomizer_network.send_to_one(pkt)) {
            m_logger->error("Failed to request block", height);
        }
    }

    void controller::request_prune(uint64_t height) {
        m_logger->trace("Requesting prune h <", height);
        auto req = atomizer::prune_request{height};
        auto pkt = make_shared_buffer(atomizer::request{req});
        if(!m_atomizer_network.send_to_one(pkt)) {
            m_logger->error("Failed to request prune", height);
        }
    }
}
