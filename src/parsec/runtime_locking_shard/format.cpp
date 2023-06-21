// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "util/serialization/format.hpp"

namespace cbdc {
    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::try_lock_request& req)
        -> serializer& {
        return ser << req.m_ticket_number << req.m_broker_id << req.m_key
                   << req.m_locktype << req.m_first_lock;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::try_lock_request& req)
        -> serializer& {
        return deser >> req.m_ticket_number >> req.m_broker_id >> req.m_key
            >> req.m_locktype >> req.m_first_lock;
    }

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::commit_request& req)
        -> serializer& {
        return ser << req.m_ticket_number;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::commit_request& req)
        -> serializer& {
        return deser >> req.m_ticket_number;
    }

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::prepare_request& req)
        -> serializer& {
        return ser << req.m_ticket_number << req.m_state_updates
                   << req.m_broker_id;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::prepare_request& req)
        -> serializer& {
        return deser >> req.m_ticket_number >> req.m_state_updates
            >> req.m_broker_id;
    }

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::rollback_request& req)
        -> serializer& {
        return ser << req.m_ticket_number;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::rollback_request& req)
        -> serializer& {
        return deser >> req.m_ticket_number;
    }

    auto
    operator<<(serializer& ser,
               const parsec::runtime_locking_shard::rpc::finish_request& req)
        -> serializer& {
        return ser << req.m_ticket_number;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::finish_request& req)
        -> serializer& {
        return deser >> req.m_ticket_number;
    }

    auto operator<<(
        serializer& ser,
        const parsec::runtime_locking_shard::rpc::get_tickets_request& req)
        -> serializer& {
        return ser << req.m_broker_id;
    }
    auto
    operator>>(serializer& deser,
               parsec::runtime_locking_shard::rpc::get_tickets_request& req)
        -> serializer& {
        return deser >> req.m_broker_id;
    }

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::shard_error& err)
        -> serializer& {
        return ser << err.m_error_code << err.m_wounded_details;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::shard_error& err)
        -> serializer& {
        return deser >> err.m_error_code >> err.m_wounded_details;
    }

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::wounded_details& det)
        -> serializer& {
        return ser << det.m_wounding_ticket << det.m_wounding_key;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::wounded_details& det)
        -> serializer& {
        return deser >> det.m_wounding_ticket >> det.m_wounding_key;
    }

    auto operator<<(
        serializer& ser,
        const parsec::runtime_locking_shard::rpc::replicated_prepare_request&
            req) -> serializer& {
        return ser << req.m_ticket_number << req.m_broker_id
                   << req.m_state_update;
    }
    auto operator>>(
        serializer& deser,
        parsec::runtime_locking_shard::rpc::replicated_prepare_request& req)
        -> serializer& {
        return deser >> req.m_ticket_number >> req.m_broker_id
            >> req.m_state_update;
    }

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::rpc::
                        replicated_get_tickets_request& /* req */)
        -> serializer& {
        return ser;
    }
    auto operator>>(serializer& deser,
                    parsec::runtime_locking_shard::rpc::
                        replicated_get_tickets_request& /* req */)
        -> serializer& {
        return deser;
    }

    auto operator<<(serializer& ser,
                    const parsec::runtime_locking_shard::
                        replicated_shard_interface::ticket_type& t)
        -> serializer& {
        return ser << t.m_state << t.m_broker_id << t.m_state_update;
    }
    auto operator>>(
        serializer& deser,
        parsec::runtime_locking_shard::replicated_shard_interface::ticket_type&
            t) -> serializer& {
        return deser >> t.m_state >> t.m_broker_id >> t.m_state_update;
    }
}
