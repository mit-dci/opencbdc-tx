// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "controller.hpp"

#include "atomizer/messages.hpp"
#include "serialization/buffer_serializer.hpp"
#include "status_update.hpp"

#include <utility>

cbdc::watchtower::controller::controller(
    uint32_t watchtower_id,
    cbdc::config::options opts,
    const std::shared_ptr<logging::log>& log)
    : m_watchtower_id(watchtower_id),
      m_opts(std::move(opts)),
      m_logger(log),
      m_watchtower(m_opts.m_watchtower_block_cache_size,
                   m_opts.m_watchtower_error_cache_size),
      m_archiver_client(m_opts.m_archiver_endpoints[0], log) {}

cbdc::watchtower::controller::~controller() {
    m_internal_network.close();
    m_external_network.close();
    m_atomizer_network.close();

    if(m_internal_server.joinable()) {
        m_internal_server.join();
    }

    if(m_external_server.joinable()) {
        m_external_server.join();
    }

    if(m_atomizer_thread.joinable()) {
        m_atomizer_thread.join();
    }
}

auto cbdc::watchtower::controller::init() -> bool {
    auto internal = m_internal_network.start_server(
        m_opts.m_watchtower_internal_endpoints[m_watchtower_id],
        [&](auto&& pkt) {
            return internal_server_handler(std::forward<decltype(pkt)>(pkt));
        });

    if(!internal.has_value()) {
        m_logger->error("Failed to establish watchtower internal server.");
        return false;
    }

    m_internal_server = std::move(internal.value());

    auto external = m_external_network.start_server(
        m_opts.m_watchtower_client_endpoints[m_watchtower_id],
        [&](auto&& pkt) {
            return external_server_handler(std::forward<decltype(pkt)>(pkt));
        });

    if(!external.has_value()) {
        m_logger->error("Failed to establish watchtower external server.");
        return false;
    }

    m_external_server = std::move(external.value());

    static constexpr auto retry_delay = std::chrono::seconds(1);
    m_atomizer_network.cluster_connect(m_opts.m_atomizer_endpoints, false);
    while(!m_atomizer_network.connected_to_one()) {
        // Since atomizers require a watchtower and the archiver requires an
        // atomizer, this has to be allowed to fail. The network will reconnect
        // when an atomizer comes online.
        m_logger->warn("Failed to connect to any atomizers, waiting...");
        std::this_thread::sleep_for(retry_delay);
    }

    while(!m_archiver_client.init()) {
        m_logger->warn("Failed to connect to archiver, retrying...");
        std::this_thread::sleep_for(retry_delay);
    }

    m_atomizer_thread = m_atomizer_network.start_handler([&](auto&& pkt) {
        return atomizer_handler(std::forward<decltype(pkt)>(pkt));
    });

    m_logger->info("Connected to atomizers.");

    return true;
}

auto cbdc::watchtower::controller::atomizer_handler(
    cbdc::network::message_t&& pkt) -> std::optional<cbdc::buffer> {
    auto maybe_blk = from_buffer<atomizer::block>(*pkt.m_pkt);
    if(!maybe_blk.has_value()) {
        m_logger->error("Invalid block packet");
        return std::nullopt;
    }
    auto& blk = maybe_blk.value();
    m_logger->debug("Received block",
                    blk.m_height,
                    "with",
                    blk.m_transactions.size(),
                    "transactions.");
    if(blk.m_height != (m_last_blk_height + 1)) {
        m_logger->warn("Block not contiguous. Last block:", m_last_blk_height);
        while(blk.m_height != (m_last_blk_height + 1)) {
            auto missed_blk
                = m_archiver_client.get_block(m_last_blk_height + 1);
            if(!missed_blk) {
                m_logger->warn("Waiting for archiver sync");
                static constexpr auto archiver_wait_time
                    = std::chrono::milliseconds(100);
                std::this_thread::sleep_for(archiver_wait_time);
                continue;
            }

            m_last_blk_height = (*missed_blk).m_height;
            m_watchtower.add_block(std::move(*missed_blk));
        }
    }
    m_last_blk_height = blk.m_height;
    m_watchtower.add_block(std::move(blk));
    return std::nullopt;
}

auto cbdc::watchtower::controller::internal_server_handler(
    cbdc::network::message_t&& pkt) -> std::optional<cbdc::buffer> {
    auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
    std::vector<cbdc::watchtower::tx_error> errs;
    deser >> errs;
    m_watchtower.add_errors(std::move(errs));
    return std::nullopt;
}

auto cbdc::watchtower::controller::external_server_handler(
    cbdc::network::message_t&& pkt) -> std::optional<cbdc::buffer> {
    auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
    auto req = request(deser);
    cbdc::buffer msg;
    auto ser = cbdc::buffer_serializer(msg);
    auto res_handler = overloaded{
        [&](const cbdc::watchtower::status_update_request& su_req) {
            auto res = m_watchtower.handle_status_update_request(su_req);
            ser << *res;
            m_logger->info("Received status_update_request with",
                           su_req.uhs_ids().size(),
                           "UHS IDs");
        },
        [&](const cbdc::watchtower::best_block_height_request& bbh_req) {
            m_logger->info("Received request_best_block_height from peer",
                           pkt.m_peer_id);
            auto res = m_watchtower.handle_best_block_height_request(bbh_req);
            ser << *res;
        }};
    std::visit(res_handler, req.payload());
    return msg;
}
