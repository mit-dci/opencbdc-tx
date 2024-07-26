// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ARCHIVER_CLIENT_H_
#define OPENCBDC_TX_SRC_ARCHIVER_CLIENT_H_

#include "uhs/atomizer/atomizer/block.hpp"
#include "util/common/logging.hpp"
#include "util/network/tcp_socket.hpp"

namespace cbdc::archiver {
    /// Height of the block to fetch from the archiver.
    using request = uint64_t;

    /// The requested block, or std::nullopt if not found.
    using response = std::optional<cbdc::atomizer::block>;

    /// \brief Retrieves blocks from a remote archiver via the network.
    ///
    /// \warning Not thread-safe. Only one thread can use the client without
    /// synchronization.
    class client {
      public:
        /// Constructor.
        /// \param endpoint archiver endpoint with which to connect.
        /// \param logger pointer to shared logger.
        client(network::endpoint_t endpoint,
               std::shared_ptr<logging::log> logger);
        client() = delete;

        /// Attempts to connect to the archiver.
        /// \return true if the connection was successful.
        auto init() -> bool;

        /// Retrieves the block at the given height from the archiver.
        /// \param height height of the block to retrieve.
        /// \return block at the given height or std::nullopt if not found.
        auto
        get_block(uint64_t height) -> std::optional<cbdc::atomizer::block>;

      private:
        network::tcp_socket m_sock;
        network::endpoint_t m_endpoint;
        std::shared_ptr<logging::log> m_logger;
    };
}

#endif // OPENCBDC_TX_SRC_ARCHIVER_CLIENT_H_
