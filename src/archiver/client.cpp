// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "client.hpp"

#include "atomizer/messages.hpp"
#include "serialization/format.hpp"

#include <utility>

namespace cbdc::archiver {
    client::client(network::endpoint_t endpoint,
                   std::shared_ptr<logging::log> logger)
        : m_endpoint(std::move(endpoint)),
          m_logger(std::move(logger)) {}

    auto client::init() -> bool {
        return m_sock.connect(m_endpoint);
    }

    auto client::get_block(uint64_t height)
        -> std::optional<cbdc::atomizer::block> {
        m_logger->info("Requesting block", height, "from archiver...");
        if(!m_sock.send(height)) {
            m_logger->error("Error requesting block from archiver.");
            return std::nullopt;
        }

        m_logger->info("Waiting for archiver response...");
        cbdc::buffer resp_pkt;
        if(!m_sock.receive(resp_pkt)) {
            m_logger->error("Error receiving block from archiver.");
            return std::nullopt;
        }

        auto resp = cbdc::from_buffer<response>(resp_pkt);
        if(!resp.has_value()) {
            m_logger->error("Invalid response packet");
            return std::nullopt;
        }

        return resp.value();
    }
}
