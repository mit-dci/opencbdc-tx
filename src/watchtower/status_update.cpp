// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "status_update.hpp"

namespace cbdc::watchtower {
    status_update_request::status_update_request(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto status_update_request::uhs_ids() const -> const tx_id_uhs_ids& {
        return m_uhs_ids;
    }

    auto
    status_update_request::operator==(const status_update_request& rhs) const
        -> bool {
        return rhs.m_uhs_ids == m_uhs_ids;
    }

    cbdc::watchtower::status_update_request::status_update_request(
        cbdc::watchtower::tx_id_uhs_ids uhs_ids)
        : m_uhs_ids(std::move(uhs_ids)) {}

    cbdc::watchtower::status_update_state::status_update_state(
        cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto status_update_state::status() const -> search_status {
        return m_status;
    }

    auto status_update_state::block_height() const -> uint64_t {
        return m_block_height;
    }

    auto status_update_state::uhs_id() const -> hash_t {
        return m_uhs_id;
    }

    auto status_update_state::operator==(const status_update_state& rhs) const
        -> bool {
        return (rhs.m_status == m_status)
            && (rhs.m_block_height == m_block_height)
            && (rhs.m_uhs_id == m_uhs_id);
    }

    status_update_state::status_update_state(search_status status,
                                             uint64_t block_height,
                                             hash_t uhs_id)
        : m_status(status),
          m_block_height(block_height),
          m_uhs_id(uhs_id) {}

    status_request_check_success::status_request_check_success(
        cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto status_request_check_success::states() const -> const tx_id_states& {
        return m_states;
    }

    auto status_request_check_success::operator==(
        const status_request_check_success& rhs) const -> bool {
        return (rhs.m_states == m_states);
    }

    status_request_check_success::status_request_check_success(
        tx_id_states states)
        : m_states(std::move(states)) {}
}
