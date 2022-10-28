// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "util/serialization/format.hpp"

namespace cbdc {
    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_account& acc)
        -> serializer& {
        return ser << acc.m_balance << acc.m_nonce;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_account& acc) -> serializer& {
        return deser >> acc.m_balance >> acc.m_nonce;
    }

    auto operator<<(serializer& ser, const evmc::address& addr)
        -> serializer& {
        ser.write(addr.bytes, sizeof(addr.bytes));
        return ser;
    }

    auto operator>>(serializer& deser, evmc::address& addr) -> serializer& {
        deser.read(addr.bytes, sizeof(addr.bytes));
        return deser;
    }

    auto operator<<(serializer& ser, const evmc::bytes32& b) -> serializer& {
        ser.write(b.bytes, sizeof(b.bytes));
        return ser;
    }

    auto operator>>(serializer& deser, evmc::bytes32& b) -> serializer& {
        deser.read(b.bytes, sizeof(b.bytes));
        return deser;
    }

    auto operator<<(serializer& ser, const threepc::agent::runner::evm_sig& s)
        -> serializer& {
        return ser << s.m_v << s.m_r << s.m_s;
    }
    auto operator>>(serializer& deser, threepc::agent::runner::evm_sig& s)
        -> serializer& {
        return deser >> s.m_v >> s.m_r >> s.m_s;
    }

    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_access_tuple& at)
        -> serializer& {
        return ser << at.m_address << at.m_storage_keys;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_access_tuple& at)
        -> serializer& {
        return deser >> at.m_address >> at.m_storage_keys;
    }

    auto operator<<(serializer& ser, const threepc::agent::runner::evm_tx& tx)
        -> serializer& {
        return ser << tx.m_type << tx.m_to << tx.m_value << tx.m_nonce
                   << tx.m_gas_price << tx.m_gas_limit << tx.m_gas_tip_cap
                   << tx.m_gas_fee_cap << tx.m_input << tx.m_access_list
                   << tx.m_sig;
    }

    auto operator>>(serializer& deser, threepc::agent::runner::evm_tx& tx)
        -> serializer& {
        return deser >> tx.m_type >> tx.m_to >> tx.m_value >> tx.m_nonce
            >> tx.m_gas_price >> tx.m_gas_limit >> tx.m_gas_tip_cap
            >> tx.m_gas_fee_cap >> tx.m_input >> tx.m_access_list >> tx.m_sig;
    }

    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_dryrun_tx& tx)
        -> serializer& {
        return ser << tx.m_from << tx.m_tx;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_dryrun_tx& tx) -> serializer& {
        return deser >> tx.m_from >> tx.m_tx;
    }

    auto operator<<(serializer& ser, const threepc::agent::runner::evm_log& l)
        -> serializer& {
        return ser << l.m_addr << l.m_data << l.m_topics;
    }

    auto operator>>(serializer& deser, threepc::agent::runner::evm_log& l)
        -> serializer& {
        return deser >> l.m_addr >> l.m_data >> l.m_topics;
    }

    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_tx_receipt& r)
        -> serializer& {
        return ser << r.m_tx << r.m_create_address << r.m_gas_used << r.m_logs
                   << r.m_output_data << r.m_ticket_number << r.m_timestamp;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_tx_receipt& r) -> serializer& {
        return deser >> r.m_tx >> r.m_create_address >> r.m_gas_used
            >> r.m_logs >> r.m_output_data >> r.m_ticket_number
            >> r.m_timestamp;
    }

    auto operator<<(serializer& ser, const threepc::agent::runner::code_key& k)
        -> serializer& {
        return ser << k.m_addr << uint8_t{};
    }

    auto operator>>(serializer& deser, threepc::agent::runner::code_key& k)
        -> serializer& {
        uint8_t b{};
        return deser >> k.m_addr >> b;
    }

    auto operator<<(serializer& ser,
                    const threepc::agent::runner::storage_key& k)
        -> serializer& {
        return ser << k.m_addr << k.m_key;
    }
    auto operator>>(serializer& deser, threepc::agent::runner::storage_key& k)
        -> serializer& {
        return deser >> k.m_addr >> k.m_key;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_pretend_block& b)
        -> serializer& {
        return deser >> b.m_ticket_number >> b.m_transactions;
    }
    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_pretend_block& b)
        -> serializer& {
        return ser << b.m_ticket_number << b.m_transactions;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_log_query& lq) -> serializer& {
        return deser >> lq.m_addresses >> lq.m_from_block >> lq.m_to_block
            >> lq.m_topics;
    }
    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_log_query& lq)
        -> serializer& {
        return ser << lq.m_addresses << lq.m_from_block << lq.m_to_block
                   << lq.m_topics;
    }

    auto operator>>(serializer& deser,
                    threepc::agent::runner::evm_log_index& idx)
        -> serializer& {
        return deser >> idx.m_ticket_number >> idx.m_txid >> idx.m_logs;
    }
    auto operator<<(serializer& ser,
                    const threepc::agent::runner::evm_log_index& idx)
        -> serializer& {
        return ser << idx.m_ticket_number << idx.m_txid << idx.m_logs;
    }
}
