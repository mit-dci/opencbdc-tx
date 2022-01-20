// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "messages.hpp"

namespace cbdc {
    auto operator<<(serializer& ser, const nuraft::ptr<nuraft::buffer>& buf)
        -> serializer& {
        ser.write(buf->data_begin(), buf->size());
        return ser;
    }
}
