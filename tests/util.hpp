// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_TESTS_UTIL_H_
#define OPENCBDC_TX_TESTS_UTIL_H_

#include "atomizer/block.hpp"
#include "common/config.hpp"
#include "common/hash.hpp"
#include "network/connection_manager.hpp"
#include "serialization/format.hpp"
#include "transaction/transaction.hpp"
#include "transaction/validation.hpp"

#include <future>
#include <gtest/gtest.h>

namespace cbdc::test {
    /// Specialization of CCompactTransaction to allow for full struct
    /// comparison, as opposed to the abbreviated comparison done in the
    /// CCompactTransaction.
    struct compact_transaction : cbdc::transaction::compact_tx {
        compact_transaction() = default;
        explicit compact_transaction(const compact_tx& transaction);
        auto operator==(const compact_tx& tx) const noexcept -> bool;
    };

    /// Allows hashing \ref compact_transaction s (e.g., for storing them
    /// in a std::unordered_set
    struct compact_transaction_hasher {
        auto operator()(const compact_transaction& tx) const noexcept
            -> size_t;
    };

    /// Specialization of Block to allow transactions to be compared as
    /// cbdc::test::compact_transactions.
    struct block : cbdc::atomizer::block {
        auto operator==(const cbdc::atomizer::block& tx) const noexcept
            -> bool;
    };

    /// Maintains a connection to the specified set of endpoints and provides a
    /// get method for transmitting data to and awaiting and reading responses
    /// from those endpoints.
    /// \tparam T type of data to expect in response to transmissions. Initialize with std::nullopt_t to disable response handling.
    template<typename T>
    class simple_client {
      public:
        simple_client() = default;

        ~simple_client() {
            m_client_net.close();
            if(m_client_thread.joinable()) {
                m_client_thread.join();
            }
        };

        /// Connect to the specified endpoints.
        /// \param endpoints to which to connect.
        /// \return false if any connections failed.
        auto connect(const std::vector<network::endpoint_t>& endpoints)
            -> bool {
            if constexpr(std::is_same_v<std::decay_t<T>, std::nullopt_t>) {
                return m_client_net.cluster_connect(endpoints, true);
            } else {
                auto ct = m_client_net.start_cluster_handler(
                    endpoints,
                    [&](cbdc::network::message_t&& pkt)
                        -> std::optional<cbdc::buffer> {
                        if(!m_expect_message) {
                            ADD_FAILURE() << "Unexpected response.";
                        }
                        T res;
                        auto deser = cbdc::buffer_serializer(*pkt.m_pkt);
                        deser >> res;
                        m_promise->set_value(res);
                        return std::nullopt;
                    });
                if(!ct.has_value()) {
                    return false;
                }
                m_client_thread = std::move(ct.value());
            }
            return true;
        }

        /// Transmit the provided data to the target endpoints and wait for the
        /// response.
        /// \param data data to broadcast.
        /// \param timeout time to wait for a response.
        /// \return value received in response to the transmission, or null on timeout.
        template<typename Ta>
        auto get(const Ta& data,
                 const std::chrono::milliseconds& timeout
                 = std::chrono::milliseconds(1000)) -> std::optional<T> {
            m_promise = std::make_unique<std::promise<T>>();
            auto fut = m_promise->get_future();
            m_expect_message = true;
            broadcast(data);
            if(fut.wait_for(timeout) != std::future_status::ready) {
                return std::nullopt;
            };
            m_expect_message = false;
            return fut.get();
        }

        /// Transmit the provided data to the target endpoints. Non-blocking.
        /// \param data data to broadcast.
        template<typename Ta>
        void broadcast(const Ta& data) {
            m_client_net.broadcast(data);
        }

      private:
        cbdc::network::connection_manager m_client_net;
        std::thread m_client_thread;
        std::unique_ptr<std::promise<T>> m_promise;
        bool m_expect_message{false};
    };

    auto simple_tx(const hash_t& id,
                   const std::vector<hash_t>& ins,
                   const std::vector<hash_t>& outs) -> compact_transaction;

    void print_sentinel_error(
        const std::optional<transaction::validation::tx_error>& err);

    /// Loads the given config file into an options struct and asserts there
    /// was not an error.
    /// \param config_file path to config file to load and parse.
    /// \param opts reference to an options struct in which to place the result.
    void load_config(const std::string& config_file,
                     cbdc::config::options& opts);
}

#endif // OPENCBDC_TX_TESTS_UTIL_H_
