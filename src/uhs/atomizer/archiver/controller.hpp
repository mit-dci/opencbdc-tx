// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_ARCHIVER_CONTROLLER_H_
#define OPENCBDC_TX_SRC_ARCHIVER_CONTROLLER_H_

#include "client.hpp"
#include "uhs/atomizer/atomizer/block.hpp"
#include "util/common/config.hpp"
#include "util/network/connection_manager.hpp"

#include <leveldb/db.h>

namespace cbdc::archiver {

    /// @brief  Wrapper for leveldb::WriteOptions to provide a constructor to
    /// set base class member "sync". The base class default constructor is
    /// built with "= default;", and as a result - in c++20 - the single
    /// parameter constructor is not available.
    struct leveldbWriteOptions : public leveldb::WriteOptions {
        explicit leveldbWriteOptions(bool do_sync);
    };

    /// \brief Wrapper for the archiver executable implementation.
    ///
    /// Connects to the atomizer cluster to receive new blocks and listens for
    /// historical block requests from clients.
    class controller {
      public:
        controller() = delete;
        controller(const controller&) = delete;
        auto operator=(const controller&) -> controller& = delete;
        controller(controller&&) = delete;
        auto operator=(controller&&) -> controller& = delete;

        /// Constructor.
        /// \param archiver_id the running ID of this archiver.
        /// \param opts configuration options.
        /// \param log pointer to shared logger.
        /// \param max_samples maximum number of blocks to digest, for testing. 0=unlimited.
        controller(uint32_t archiver_id,
                   config::options opts,
                   std::shared_ptr<logging::log> log,
                   size_t max_samples);

        ~controller();

        /// Initializes the controller with all its dependencies.
        /// \return true if initialization succeeded.
        auto init() -> bool;

        /// Initializes the LevelDB database.
        /// \return true if initialization succeeded.
        auto init_leveldb() -> bool;

        /// Initializes the best block value.
        /// \return true if initialization succeeded.
        auto init_best_block() -> bool;

        /// Initializes the sample collection.
        /// \return true if initialization succeeded.
        auto init_sample_collection() -> bool;

        /// Initializes the connection to the atomizer.
        /// \return true if initialization succeeded.
        auto init_atomizer_connection() -> bool;

        /// Initializes the archiver server.
        /// \return true if initialization succeeded.
        auto init_archiver_server() -> bool;

        /// Returns the archiver's best block height.
        /// \return best block height.
        [[nodiscard]] auto best_block_height() const -> uint64_t;

        /// Receives a request for an archived block and returns the block.
        /// \param pkt packet containing the request.
        /// \return block or std::nullopt from \ref get_block.
        /// \see \ref network::packet_handler_t
        auto server_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;

        /// Receives a serialized block from the atomizer and digests it.
        /// \param pkt packet containing the serialized block.
        /// \return std::nullopt; no reply to atomizer necessary.
        /// \see \ref digest_block
        /// \see \ref network::packet_handler_t
        auto atomizer_handler(cbdc::network::message_t&& pkt)
            -> std::optional<cbdc::buffer>;

        /// \brief Adds a block to the archiver database.
        ///
        /// Initializes the best block height field on its first call. If the
        /// controller's known best block height is not contiguous with the
        /// height of the provided block, recursively requests preceding blocks
        /// from the atomizer. Stores each received block in a deferred
        /// processing cache until receiving the next contiguous block, then
        /// digests each block in order.
        ///
        /// Instructs connected atomizers to prune digested blocks.
        /// \param blk block to digest.
        void digest_block(const cbdc::atomizer::block& blk);

        /// Queries the archiver database for the block at the specified
        /// height.
        /// \param height the height of the block to retrieve.
        /// \return the block at the specified height, or std::nullopt if the
        ///         database does not contain a block at that height.
        auto
        get_block(uint64_t height) -> std::optional<cbdc::atomizer::block>;

        /// \brief Returns true if this archiver is receiving blocks
        /// from the atomizer.
        ///
        /// Will be true while the total number of digested blocks
        /// is less than max_samples.
        /// \return false if the controller is finished digesting blocks.
        [[nodiscard]] auto running() const -> bool;

      private:
        uint32_t m_archiver_id;
        cbdc::config::options m_opts;
        std::shared_ptr<logging::log> m_logger;

        std::unique_ptr<leveldb::DB> m_db;
        uint64_t m_best_height{0};
        /// Blocks pending digestion, waiting for the archiver to digest
        /// preceding blocks from the atomizer, keyed by height.
        /// \see \ref digest_block
        std::map<uint64_t, cbdc::atomizer::block> m_deferred;
        std::ofstream m_tp_sample_file;
        std::chrono::high_resolution_clock::time_point m_last_block_time;
        size_t m_max_samples{};
        size_t m_samples{};

        cbdc::network::connection_manager m_atomizer_network;
        cbdc::network::connection_manager m_archiver_network;

        bool m_sample_collection_active{false};

        std::thread m_atomizer_handler_thread;
        std::thread m_archiver_server;

        std::atomic_bool m_running{true};

        const std::string m_bestblock_key = "bestblock";
        static constexpr const leveldb::ReadOptions m_read_options{};
        static const leveldbWriteOptions m_write_options;

        void request_block(uint64_t height);
        void request_prune(uint64_t height);
    };
}

#endif // OPENCBDC_TX_SRC_ARCHIVER_CONTROLLER_H_
