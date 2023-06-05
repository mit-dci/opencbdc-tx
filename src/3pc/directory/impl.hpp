// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_3PC_DIRECTORY_IMPL_H_
#define OPENCBDC_TX_SRC_3PC_DIRECTORY_IMPL_H_

#include "interface.hpp"

namespace cbdc::threepc::directory {
    /// Implementation of a directory which map keys to shard IDs. Thread-safe.
    class impl : public interface {
      public:
        /// Constructor.
        /// \param n_shards number of shards available to the directory.
        explicit impl(size_t n_shards);

        /// Returns the shard ID responsible for the given key. Calls the
        /// callback before returning.
        /// \param key key to locate.
        /// \param result_callback function to call with key location.
        /// \return true.
        auto key_location(runtime_locking_shard::key_type key,
                          key_location_callback_type result_callback)
            -> bool override;

      private:
        size_t m_n_shards{};
        hashing::const_sip_hash<runtime_locking_shard::key_type> m_siphash{};
    };
}

#endif
