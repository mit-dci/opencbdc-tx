// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "format.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util2.hpp"

namespace cbdc::locking_shard::rpc {
    client::client(std::vector<network::endpoint_t> endpoints,
                   const std::pair<uint8_t, uint8_t>& output_range,
                   logging::log& logger)
        : interface(output_range),
          m_log(logger) {
        m_client = std::make_unique<decltype(m_client)::element_type>(
            std::move(endpoints));
    }

    client::~client() {
        stop();
    }

    auto client::init() -> bool {
        return m_client->init();
    }

    auto client::lock_outputs(std::vector<tx>&& txs, const hash_t& dtx_id)
        -> std::optional<std::vector<bool>> {
        auto req = request{dtx_id, std::move(txs)};
        auto resp = send_request(req);
        if(!resp.has_value()) {
            return std::nullopt;
        }
        return std::get<lock_response>(resp.value());
    }

    auto client::apply_outputs(std::vector<bool>&& complete_txs,
                               const hash_t& dtx_id) -> bool {
        auto req = request{dtx_id, std::move(complete_txs)};
        auto res = send_request(req);
        return res.has_value();
    }

    auto client::discard_dtx(const hash_t& dtx_id) -> bool {
        auto req = request{dtx_id, discard_params()};
        auto res = send_request(req);
        return res.has_value();
    }

    auto client::send_request(const request& req) -> std::optional<response> {
        auto result_timeout = std::chrono::seconds(3);
        constexpr auto max_result_timeout = std::chrono::seconds(10);
        constexpr auto retry_delay = std::chrono::seconds(1);
        auto res = std::optional<response>();
        while(m_running && !res.has_value()) {
            res = m_client->call(req, result_timeout);
            if(!res.has_value()) {
                m_log.warn("Shard request failed");
                if(m_running) {
                    std::this_thread::sleep_for(retry_delay);
                    result_timeout
                        = std::min(max_result_timeout, result_timeout * 2);
                }
            }
        }
        return res;
    }

    void client::stop() {
        m_running = false;
        m_client.reset();
    }
}
