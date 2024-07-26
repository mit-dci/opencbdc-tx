// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "format.hpp"

#include "atomizer_raft.hpp"
#include "uhs/transaction/messages.hpp"
#include "util/serialization/format.hpp"
#include "util/serialization/util.hpp"

namespace cbdc {
    auto operator<<(serializer& packet,
                    const cbdc::atomizer::block& blk) -> serializer& {
        return packet << blk.m_height << blk.m_transactions;
    }

    auto operator>>(serializer& packet,
                    cbdc::atomizer::block& blk) -> serializer& {
        return packet >> blk.m_height >> blk.m_transactions;
    }

    auto
    operator<<(serializer& ser,
               const atomizer::state_machine::snapshot& snp) -> serializer& {
        auto atomizer_buf = snp.m_atomizer->serialize();
        auto snp_buf = snp.m_snp->serialize();
        ser << static_cast<uint64_t>(snp_buf->size());
        ser.write(snp_buf->data_begin(), snp_buf->size());
        ser.write(atomizer_buf.data(), atomizer_buf.size());
        ser << *snp.m_blocks;
        return ser;
    }

    auto operator>>(serializer& deser,
                    atomizer::state_machine::snapshot& snp) -> serializer& {
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

    auto
    operator<<(serializer& packet,
               const cbdc::atomizer::tx_notify_request& msg) -> serializer& {
        packet << msg.m_block_height << msg.m_tx << msg.m_attestations;
        return packet;
    }

    auto operator>>(serializer& packet,
                    cbdc::atomizer::tx_notify_request& msg) -> serializer& {
        packet >> msg.m_block_height >> msg.m_tx >> msg.m_attestations;
        return packet;
    }

    auto operator<<(serializer& packet,
                    const cbdc::atomizer::aggregate_tx_notification& msg)
        -> serializer& {
        packet << msg.m_oldest_attestation << msg.m_tx;
        return packet;
    }

    auto
    operator>>(serializer& packet,
               cbdc::atomizer::aggregate_tx_notification& msg) -> serializer& {
        packet >> msg.m_oldest_attestation >> msg.m_tx;
        return packet;
    }

    auto operator<<(serializer& packet,
                    const cbdc::atomizer::aggregate_tx_notify_request& msg)
        -> serializer& {
        return packet << msg.m_agg_txs;
    }

    auto operator>>(serializer& packet,
                    cbdc::atomizer::aggregate_tx_notify_request& msg)
        -> serializer& {
        return packet >> msg.m_agg_txs;
    }

    auto operator<<(serializer& ser,
                    const atomizer::prune_request& r) -> serializer& {
        return ser << r.m_block_height;
    }
    auto operator>>(serializer& deser,
                    atomizer::prune_request& r) -> serializer& {
        return deser >> r.m_block_height;
    }

    auto
    operator<<(serializer& ser,
               const atomizer::make_block_request& /* r */) -> serializer& {
        return ser;
    }
    auto operator>>(serializer& deser,
                    atomizer::make_block_request& /* r */) -> serializer& {
        return deser;
    }

    auto operator<<(serializer& ser,
                    const atomizer::get_block_request& r) -> serializer& {
        return ser << r.m_block_height;
    }
    auto operator>>(serializer& deser,
                    atomizer::get_block_request& r) -> serializer& {
        return deser >> r.m_block_height;
    }

    auto operator<<(serializer& ser,
                    const atomizer::make_block_response& r) -> serializer& {
        return ser << r.m_blk << r.m_errs;
    }
    auto operator>>(serializer& deser,
                    atomizer::make_block_response& r) -> serializer& {
        return deser >> r.m_blk >> r.m_errs;
    }

    auto operator<<(serializer& ser,
                    const atomizer::get_block_response& r) -> serializer& {
        return ser << r.m_blk;
    }
    auto operator>>(serializer& deser,
                    atomizer::get_block_response& r) -> serializer& {
        return deser >> r.m_blk;
    }
}
