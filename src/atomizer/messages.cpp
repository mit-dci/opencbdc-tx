// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

#include "atomizer_raft.hpp"
#include "serialization/format.hpp"
#include "serialization/util.hpp"
#include "transaction/messages.hpp"

namespace cbdc {
    auto operator<<(serializer& packet, const cbdc::atomizer::block& blk)
        -> serializer& {
        return packet << blk.m_height << blk.m_transactions;
    }

    auto operator>>(serializer& packet, cbdc::atomizer::block& blk)
        -> serializer& {
        return packet >> blk.m_height >> blk.m_transactions;
    }

    auto operator<<(serializer& ser,
                    const atomizer::state_machine::snapshot& snp)
        -> serializer& {
        auto atomizer_buf = snp.m_atomizer->serialize();
        auto snp_buf = snp.m_snp->serialize();
        ser << static_cast<uint64_t>(snp_buf->size());
        ser.write(snp_buf->data_begin(), snp_buf->size());
        ser.write(atomizer_buf.data(), atomizer_buf.size());
        ser << *snp.m_blocks;
        return ser;
    }

    auto operator>>(serializer& deser, atomizer::state_machine::snapshot& snp)
        -> serializer& {
        uint64_t snp_sz{};
        deser >> snp_sz;
        auto snp_buf = nuraft::buffer::alloc(snp_sz);
        deser.read(snp_buf->data_begin(), snp_buf->size());
        auto nuraft_snp = nuraft::snapshot::deserialize(*snp_buf);
        snp.m_snp = std::move(nuraft_snp);
        snp.m_atomizer->deserialize(deser);
        snp.m_blocks->clear();
        deser >> *snp.m_blocks;
        return deser;
    }

    auto operator<<(serializer& packet,
                    const cbdc::atomizer::tx_notify_message& msg)
        -> serializer& {
        packet << atomizer::state_machine::command::tx_notify
               << msg.m_block_height << msg.m_tx << msg.m_attestations;
        return packet;
    }

    auto operator>>(serializer& packet, cbdc::atomizer::tx_notify_message& msg)
        -> serializer& {
        atomizer::state_machine::command command_byte{};
        packet >> command_byte >> msg.m_block_height >> msg.m_tx
            >> msg.m_attestations;
        return packet;
    }

    auto operator<<(serializer& packet,
                    const cbdc::atomizer::aggregate_tx_notify& msg)
        -> serializer& {
        packet << msg.m_oldest_attestation << msg.m_tx;
        return packet;
    }

    auto operator>>(serializer& packet,
                    cbdc::atomizer::aggregate_tx_notify& msg) -> serializer& {
        packet >> msg.m_oldest_attestation >> msg.m_tx;
        return packet;
    }

    auto operator<<(serializer& packet,
                    const cbdc::atomizer::aggregate_tx_notify_set& msg)
        -> serializer& {
        return packet << msg.m_cmd << msg.m_agg_txs;
    }

    auto operator>>(serializer& packet,
                    cbdc::atomizer::aggregate_tx_notify_set& msg)
        -> serializer& {
        return packet >> msg.m_cmd >> msg.m_agg_txs;
    }
}
