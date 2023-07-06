// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

#include "transaction.hpp"
#include "util/common/variant_overloaded.hpp"
#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const transaction::out_point& op)
        -> serializer& {
        return packet << op.m_tx_id << op.m_index;
    }

    auto operator>>(serializer& packet, transaction::out_point& op)
        -> serializer& {
        return packet >> op.m_tx_id >> op.m_index;
    }

    auto operator<<(serializer& packet, const transaction::output& out)
        -> serializer& {
        return packet << out.m_witness_program_commitment << out.m_value;
    }

    auto operator>>(serializer& packet, transaction::output& out)
        -> serializer& {
        return packet >> out.m_witness_program_commitment >> out.m_value;
    }

    auto operator<<(serializer& packet, const transaction::input& inp)
        -> serializer& {
        return packet << inp.m_prevout << inp.m_prevout_data;
    }

    auto operator>>(serializer& packet, transaction::input& inp)
        -> serializer& {
        return packet >> inp.m_prevout >> inp.m_prevout_data;
    }

    auto operator<<(serializer& packet, const transaction::full_tx& tx)
        -> serializer& {
        return packet << tx.m_inputs << tx.m_outputs << tx.m_witness;
    }

    auto operator>>(serializer& packet, transaction::full_tx& tx)
        -> serializer& {
        return packet >> tx.m_inputs >> tx.m_outputs >> tx.m_witness;
    }

    auto operator<<(serializer& packet, const transaction::compact_tx& tx)
        -> serializer& {
        return packet << tx.m_id << tx.m_inputs << tx.m_uhs_outputs
                      << tx.m_attestations;
    }

    auto operator>>(serializer& packet, transaction::compact_tx& tx)
        -> serializer& {
        return packet >> tx.m_id >> tx.m_inputs >> tx.m_uhs_outputs
            >> tx.m_attestations;
    }

    auto operator>>(serializer& packet,
                    transaction::validation::input_error& e) -> serializer& {
        return packet >> e.m_code >> e.m_data_err >> e.m_idx;
    }

    auto operator<<(serializer& packet,
                    const transaction::validation::input_error& e)
        -> serializer& {
        return packet << e.m_code << e.m_data_err << e.m_idx;
    }

    auto operator>>(serializer& packet,
                    transaction::validation::output_error& e) -> serializer& {
        return packet >> e.m_code >> e.m_idx;
    }

    auto operator<<(serializer& packet,
                    const transaction::validation::output_error& e)
        -> serializer& {
        return packet << e.m_code << e.m_idx;
    }

    auto operator>>(serializer& packet,
                    transaction::validation::witness_error& e) -> serializer& {
        return packet >> e.m_code >> e.m_idx;
    }

    auto operator<<(serializer& packet,
                    const transaction::validation::witness_error& e)
        -> serializer& {
        return packet << e.m_code << e.m_idx;
    }

    auto operator<<(serializer& ser, const transaction::uhs_element& e)
        -> serializer& {
        return ser << e.m_id << e.m_data << e.m_value;
    }

    auto operator>>(serializer& deser, transaction::uhs_element& e)
        -> serializer& {
        return deser >> e.m_id >> e.m_data >> e.m_value;
    }
}
