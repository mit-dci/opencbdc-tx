// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_DIRECTORY_INTERFACE_H_
#define OPENCBDC_TX_SRC_PARSEC_DIRECTORY_INTERFACE_H_

#include "parsec/runtime_locking_shard/interface.hpp"

namespace cbdc::parsec::directory {
    /// Interface for a directory. Maps keys to shard IDs.
    class interface {
      public:
        virtual ~interface() = default;

        interface() = default;
        interface(const interface&) = delete;
        auto operator=(const interface&) -> interface& = delete;
        interface(interface&&) = delete;
        auto operator=(interface&&) -> interface& = delete;

        /// Key location return type. Shard ID where key is located.
        using key_location_return_type = uint64_t;
        /// Callback function type for key location result.
        using key_location_callback_type
            = std::function<void(key_location_return_type)>;

        /// Returns the shard ID responsible for the given key.
        /// \param key key to locate.
        /// \param result_callback function to call with key location.
        /// \return true if the operation was initiated successfully.
        virtual auto
        key_location(runtime_locking_shard::key_type key,
                     key_location_callback_type result_callback) -> bool = 0;
    };
}

#endif
