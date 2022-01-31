// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "state_machine.hpp"

#include "atomizer.hpp"
#include "atomizer_raft.hpp"
#include "format.hpp"
#include "util/raft/serialization.hpp"
#include "util/raft/util.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/istream_serializer.hpp"
#include "util/serialization/ostream_serializer.hpp"
#include "util/serialization/util.hpp"

#include <filesystem>
#include <libnuraft/nuraft.hxx>
#include <utility>

namespace cbdc::atomizer {
    state_machine::state_machine(size_t stxo_cache_depth,
                                 std::string snapshot_dir)
        : m_snapshot_dir(std::move(snapshot_dir)),
          m_stxo_cache_depth(stxo_cache_depth) {
        m_atomizer = std::make_shared<atomizer>(0, m_stxo_cache_depth);
        m_blocks = std::make_shared<decltype(m_blocks)::element_type>();
        auto err = std::error_code();
        std::filesystem::create_directory(m_snapshot_dir, err);
        if(err) {
            std::exit(EXIT_FAILURE);
        }
        auto snp = state_machine::last_snapshot();
        if(snp) {
            if(!state_machine::apply_snapshot(*snp)) {
                std::exit(EXIT_FAILURE);
            }
        }
    }

    auto state_machine::commit(nuraft::ulong log_idx, nuraft::buffer& data)
        -> nuraft::ptr<nuraft::buffer> {
        m_last_committed_idx = log_idx;
        auto req = from_buffer<request>(data);
        assert(req.has_value());

        auto resp = std::visit(
            overloaded{
                [&](aggregate_tx_notify_request& r)
                    -> std::optional<response> {
                    auto errs = errors();
                    for(auto&& msg : r.m_agg_txs) {
                        auto err = m_atomizer->insert_complete(
                            msg.m_oldest_attestation,
                            std::move(msg.m_tx));

                        if(err.has_value()) {
                            errs.push_back(*err);
                        }
                        m_tx_notify_count++;
                    }

                    if(!errs.empty()) {
                        return errs;
                    }

                    return std::nullopt;
                },
                [&](const make_block_request& /* r */)
                    -> std::optional<response> {
                    auto [blk, errs] = m_atomizer->make_block();
                    m_blocks->emplace(blk.m_height, blk);
                    return make_block_response{blk, errs};
                },
                [&](const get_block_request& r) -> std::optional<response> {
                    auto it = m_blocks->find(r.m_block_height);
                    if(it != m_blocks->end()) {
                        return get_block_response{it->second};
                    }
                    return std::nullopt;
                },
                [&](const prune_request& r) -> std::optional<response> {
                    for(auto it = m_blocks->begin(); it != m_blocks->end();) {
                        if(it->second.m_height < r.m_block_height) {
                            it = m_blocks->erase(it);
                        } else {
                            it++;
                        }
                    }
                    return std::nullopt;
                },
            },
            req.value());
        if(!resp.has_value()) {
            return nullptr;
        }
        return make_buffer<response, nuraft::ptr<nuraft::buffer>>(
            resp.value());
    }

    auto
    state_machine::read_logical_snp_obj(nuraft::snapshot& s,
                                        void*& /* user_snp_ctx */,
                                        nuraft::ulong /* obj_id */,
                                        nuraft::ptr<nuraft::buffer>& data_out,
                                        bool& is_last_obj) -> int {
        auto path = get_snapshot_path(s.get_last_log_idx());
        {
            std::shared_lock<std::shared_mutex> l(m_snp_mut);
            auto ss = std::ifstream(path, std::ios::in | std::ios::binary);
            if(!ss.good()) {
                // Requested snapshot doesn't exit anymore, not fatal
                return -1;
            }
            auto err = std::error_code();
            auto sz = std::filesystem::file_size(path, err);
            if(err) {
                // If we got this far, this should work unless our system is
                // broken
                std::exit(EXIT_FAILURE);
            }
            auto buf = nuraft::buffer::alloc(sz);
            auto read_vec = std::vector<char>(sz);
            ss.read(read_vec.data(),
                    static_cast<std::streamsize>(buf->size()));
            if(!ss.good()) {
                // If we got this far, this should work unless our system is
                // broken
                std::exit(EXIT_FAILURE);
            }
            std::memcpy(buf->data_begin(), read_vec.data(), sz);
            data_out = std::move(buf);
        }

        // TODO: send and receive in chunks
        is_last_obj = true;

        return 0;
    }

    void state_machine::save_logical_snp_obj(nuraft::snapshot& s,
                                             nuraft::ulong& obj_id,
                                             nuraft::buffer& data,
                                             bool /* is_first_obj */,
                                             bool /* is_last_obj */) {
        assert(obj_id == 0);
        auto tmp_path = get_tmp_path();
        {
            std::unique_lock<std::shared_mutex> l(m_snp_mut);
            auto ss = std::ofstream(tmp_path,
                                    std::ios::out | std::ios::trunc
                                        | std::ios::binary);
            if(!ss.good()) {
                // Since we're the exclusive writer, this should work
                std::exit(EXIT_FAILURE);
            }

            auto write_vec = std::vector<char>(data.size());
            std::memcpy(write_vec.data(), data.data_begin(), data.size());
            ss.write(write_vec.data(),
                     static_cast<std::streamsize>(data.size()));
            if(!ss.good()) {
                std::exit(EXIT_FAILURE);
            }

            ss.flush();
            ss.close();

            auto path = get_snapshot_path(s.get_last_log_idx());
            auto err = std::error_code();
            std::filesystem::rename(tmp_path, path, err);
            if(err) {
                std::exit(EXIT_FAILURE);
            }
        }

        obj_id++;
    }

    auto state_machine::apply_snapshot(nuraft::snapshot& s) -> bool {
        auto snp = read_snapshot(s.get_last_log_idx());
        if(snp) {
            m_blocks = snp->m_blocks;
            m_atomizer = snp->m_atomizer;
            m_last_committed_idx = s.get_last_log_idx();
        }
        return snp.has_value();
    }

    auto state_machine::last_snapshot() -> nuraft::ptr<nuraft::snapshot> {
        auto snp = read_snapshot(0);
        if(!snp) {
            return nullptr;
        }
        return snp->m_snp;
    }

    auto state_machine::last_commit_index() -> nuraft::ulong {
        return m_last_committed_idx;
    }

    void state_machine::create_snapshot(
        nuraft::snapshot& s,
        nuraft::async_result<bool>::handler_type& when_done) {
        assert(s.get_last_log_idx() == last_commit_index());
        nuraft::ptr<std::exception> except(nullptr);
        bool ret = true;

        auto snp_ser = s.serialize();
        auto snp = snapshot{m_atomizer,
                            nuraft::snapshot::deserialize(*snp_ser),
                            m_blocks};

        auto tmp_path = get_tmp_path();
        auto path = get_snapshot_path(s.get_last_log_idx());
        {
            std::unique_lock<std::shared_mutex> l(m_snp_mut);
            auto ss = std::ofstream(tmp_path,
                                    std::ios::out | std::ios::trunc
                                        | std::ios::binary);
            if(!ss.good()) {
                // We're the exclusive writer so these file operations should
                // work
                std::exit(EXIT_FAILURE);
            }

            auto ser = cbdc::ostream_serializer(ss);
            if(!(ser << snp)) {
                std::exit(EXIT_FAILURE);
            }

            ss.flush();
            ss.close();

            auto err = std::error_code();
            std::filesystem::rename(tmp_path, path, err);
            if(err) {
                std::exit(EXIT_FAILURE);
            }

            for(const auto& p :
                std::filesystem::directory_iterator(m_snapshot_dir)) {
                auto name = p.path().filename().generic_string();
                if(name == m_tmp_file
                   || std::stoull(name) < s.get_last_log_idx()) {
                    std::filesystem::remove(p, err);
                    if(err) {
                        std::exit(EXIT_FAILURE);
                    }
                }
            }
        }

        when_done(ret, except);
    }

    auto state_machine::tx_notify_count() -> uint64_t {
        return m_tx_notify_count;
    }

    auto state_machine::get_snapshot_path(uint64_t idx) const -> std::string {
        return m_snapshot_dir + "/" + std::to_string(idx);
    }

    auto state_machine::get_tmp_path() const -> std::string {
        return m_snapshot_dir + "/" + m_tmp_file;
    }

    auto state_machine::read_snapshot(uint64_t idx)
        -> std::optional<snapshot> {
        std::shared_lock<std::shared_mutex> l(m_snp_mut);
        auto open_fail_fatal = false;
        if(idx == 0) {
            uint64_t max_idx{0};
            auto err = std::error_code();
            for(const auto& p :
                std::filesystem::directory_iterator(m_snapshot_dir, err)) {
                if(err) {
                    std::exit(EXIT_FAILURE);
                }
                auto name = p.path().filename().generic_string();
                if(name == m_tmp_file) {
                    continue;
                }
                auto f_idx = std::stoull(name);
                if(f_idx > max_idx) {
                    max_idx = f_idx;
                }
            }

            if(max_idx == 0) {
                return std::nullopt;
            }

            idx = max_idx;
            open_fail_fatal = true;
        }

        auto path = get_snapshot_path(idx);

        auto ss = std::ifstream(path, std::ios::in | std::ios::binary);
        if(!ss.good()) {
            if(open_fail_fatal) {
                std::exit(EXIT_FAILURE);
            }
            return std::nullopt;
        }
        auto err = std::error_code();
        auto sz = std::filesystem::file_size(path, err);
        if(err) {
            std::exit(EXIT_FAILURE);
        }
        auto deser = cbdc::istream_serializer(ss);
        auto new_atm = std::make_shared<atomizer>(0, m_stxo_cache_depth);
        auto new_blocks = std::make_shared<decltype(m_blocks)::element_type>();
        auto snp
            = snapshot{std::move(new_atm), nullptr, std::move(new_blocks)};
        if(!(deser >> snp)) {
            std::exit(EXIT_FAILURE);
        }
        snp.m_snp->set_size(sz);
        return snp;
    }
}
