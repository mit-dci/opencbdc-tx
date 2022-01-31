// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_RAFT_MESSAGES_H_
#define OPENCBDC_TX_SRC_RAFT_MESSAGES_H_

#include "util/serialization/serializer.hpp"

#include <libnuraft/buffer.hxx>

namespace cbdc {
    auto operator<<(serializer& ser, const nuraft::ptr<nuraft::buffer>& buf)
        -> serializer&;
}

#endif // OPENCBDC_TX_SRC_RAFT_MESSAGES_H_
