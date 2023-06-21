// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TEST_UNIT_PARSEC_UTIL_INC_
#define TEST_UNIT_PARSEC_UTIL_INC_

#include "parsec/broker/interface.hpp"

#include <memory>

namespace cbdc::test {
    void add_to_shard(std::shared_ptr<cbdc::parsec::broker::interface> broker,
                      cbdc::buffer key,
                      cbdc::buffer value);
}

#endif
