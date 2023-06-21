// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_FORMAT_H_
#define OPENCBDC_TX_SRC_PARSEC_RUNTIME_LOCKING_SHARD_FORMAT_H_

#include "messages.hpp"
#include "util/serialization/serializer.hpp"

namespace cbdc {
    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::try_lock_request& req)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::try_lock_request& req)
        -> serializer&;

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::commit_request& req)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::commit_request& req)
        -> serializer&;

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::prepare_request& req)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::prepare_request& req)
        -> serializer&;

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::rollback_request& req)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::rollback_request& req)
        -> serializer&;

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::finish_request& req)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::finish_request& req)
        -> serializer&;

    auto operator<<(
        serializer& ser,
        const parsec::runtime_locking_shard::rpc::get_tickets_request& req)
        -> serializer&;
    auto
    operator>>(serializer& deser,
               parsec::runtime_locking_shard::rpc::get_tickets_request& req)
        -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::shard_error& err)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::shard_error& err)
        -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::wounded_details& det)
        -> serializer&;
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::wounded_details& det)
        -> serializer&;

    auto operator<<(
        serializer& ser,
        const parsec::runtime_locking_shard::rpc::replicated_prepare_request&
            req) -> serializer&;
    auto operator>>(
        serializer& deser,
        parsec::runtime_locking_shard::rpc::replicated_prepare_request& req)
        -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::rpc::
                        replicated_get_tickets_request& req) -> serializer&;
    auto operator>>(
        serializer& deser,
        parsec::runtime_locking_shard::rpc::replicated_get_tickets_request&
            req) -> serializer&;

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::
                        replicated_shard_interface::ticket_type& t)
        -> serializer&;
    auto operator>>(
        serializer& deser,
        parsec::runtime_locking_shard::replicated_shard_interface::ticket_type&
            t) -> serializer&;
}

#endif
