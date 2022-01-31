// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tx_error_messages.hpp"

#include "util/common/variant_overloaded.hpp"
#include "util/serialization/format.hpp"

namespace cbdc::watchtower {
    tx_error_sync::tx_error_sync(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto tx_error_sync::operator==(const tx_error_sync& /* rhs */) const
        -> bool {
        return true;
    }

    tx_error_inputs_dne::tx_error_inputs_dne(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto tx_error_inputs_dne::input_uhs_ids() const -> std::vector<hash_t> {
        return m_input_uhs_ids;
    }

    tx_error_inputs_dne::tx_error_inputs_dne(std::vector<hash_t> input_uhs_ids)
        : m_input_uhs_ids(std::move(input_uhs_ids)) {}

    auto tx_error_inputs_dne::operator==(const tx_error_inputs_dne& rhs) const
        -> bool {
        return m_input_uhs_ids == rhs.m_input_uhs_ids;
    }

    tx_error_stxo_range::tx_error_stxo_range(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto
    tx_error_stxo_range::operator==(const tx_error_stxo_range& /* rhs */) const
        -> bool {
        return true;
    }

    tx_error_incomplete::tx_error_incomplete(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto
    tx_error_incomplete::operator==(const tx_error_incomplete& /* rhs */) const
        -> bool {
        return true;
    }

    tx_error_inputs_spent::tx_error_inputs_spent(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto tx_error_inputs_spent::input_uhs_ids() const
        -> std::unordered_set<hash_t, hashing::null> {
        return m_input_uhs_ids;
    }

    tx_error_inputs_spent::tx_error_inputs_spent(
        std::unordered_set<hash_t, hashing::null> input_uhs_ids)
        : m_input_uhs_ids(std::move(input_uhs_ids)) {}

    auto
    tx_error_inputs_spent::operator==(const tx_error_inputs_spent& rhs) const
        -> bool {
        return rhs.m_input_uhs_ids == m_input_uhs_ids;
    }

    tx_error::tx_error(cbdc::serializer& pkt) {
        pkt >> *this;
    }

    auto tx_error::tx_id() const -> hash_t {
        return m_tx_id;
    }

    auto tx_error::info() const -> tx_error_info {
        return *m_info;
    }

    auto tx_error::to_string() const -> std::string {
        const auto* ret = "Unknown error";
        std::visit(
            overloaded{
                [&](const tx_error_sync& /* v */) {
                    ret = "Shard is not synchronized with atomizer";
                },
                [&](const tx_error_inputs_dne& /* v */) {
                    ret = "Input(s) do not exist";
                },
                [&](const tx_error_stxo_range& /* v */) {
                    ret = "Transaction not in STXO Cache range";
                },
                [&](const tx_error_inputs_spent& /* v */) {
                    ret = "Input(s) are already spent";
                },
                [&](const tx_error_incomplete& /* v */) {
                    ret = "Did not receive attestations to all inputs before "
                          "STXO cache expired";
                }},
            *m_info);
        return ret;
    }

    auto tx_error::operator==(const tx_error& rhs) const -> bool {
        return rhs.m_tx_id == m_tx_id && *rhs.m_info == *m_info;
    }

    tx_error::tx_error(const hash_t& tx_id, const tx_error_sync& err)
        : m_tx_id(tx_id),
          m_info(std::make_shared<decltype(m_info)::element_type>(err)) {}

    tx_error::tx_error(const hash_t& tx_id, const tx_error_inputs_dne& err)
        : m_tx_id(tx_id),
          m_info(std::make_shared<decltype(m_info)::element_type>(err)) {}

    tx_error::tx_error(const hash_t& tx_id, const tx_error_stxo_range& err)
        : m_tx_id(tx_id),
          m_info(std::make_shared<decltype(m_info)::element_type>(err)) {}

    tx_error::tx_error(const hash_t& tx_id, const tx_error_incomplete& err)
        : m_tx_id(tx_id),
          m_info(std::make_shared<decltype(m_info)::element_type>(err)) {}

    tx_error::tx_error(const hash_t& tx_id,
                       const cbdc::watchtower::tx_error_inputs_spent& err)
        : m_tx_id(tx_id),
          m_info(std::make_shared<decltype(m_info)::element_type>(err)) {}
}

namespace cbdc {
    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::tx_error_inputs_dne& err)
        -> cbdc::serializer& {
        packet << err.m_input_uhs_ids;
        return packet;
    }

    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::tx_error_inputs_dne& err)
        -> cbdc::serializer& {
        packet >> err.m_input_uhs_ids;
        return packet;
    }

    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::tx_error_inputs_spent& err)
        -> cbdc::serializer& {
        packet << err.m_input_uhs_ids;
        return packet;
    }

    auto operator>>(cbdc::serializer& packet,
                    cbdc::watchtower::tx_error_inputs_spent& err)
        -> cbdc::serializer& {
        packet >> err.m_input_uhs_ids;
        return packet;
    }

    auto operator<<(cbdc::serializer& packet,
                    const cbdc::watchtower::tx_error& err)
        -> cbdc::serializer& {
        return packet << err.m_tx_id << *err.m_info;
    }

    auto operator>>(cbdc::serializer& packet, cbdc::watchtower::tx_error& err)
        -> cbdc::serializer& {
        packet >> err.m_tx_id;
        err.m_info = std::make_shared<decltype(err.m_info)::element_type>(
            get_variant<watchtower::tx_error_sync,
                        watchtower::tx_error_inputs_dne,
                        watchtower::tx_error_stxo_range,
                        watchtower::tx_error_incomplete,
                        watchtower::tx_error_inputs_spent>(packet));
        return packet;
    }
}
