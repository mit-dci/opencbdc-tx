// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../util.hpp"
#include "3pc/agent/impl.hpp"
#include "3pc/broker/impl.hpp"
#include "3pc/directory/impl.hpp"
#include "3pc/runtime_locking_shard/impl.hpp"
#include "3pc/ticket_machine/impl.hpp"

#include <gtest/gtest.h>

TEST(broker_test, deploy_test) {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);
    auto shard = std::make_shared<cbdc::threepc::runtime_locking_shard::impl>(
        log,
        nullptr);
    auto ticketer
        = std::make_shared<cbdc::threepc::ticket_machine::impl>(log, 1);
    auto directory = std::make_shared<cbdc::threepc::directory::impl>(1);
    auto broker = std::make_shared<cbdc::threepc::broker::impl>(
        0,
        std::vector<
            std::shared_ptr<cbdc::threepc::runtime_locking_shard::interface>>(
            {shard}),
        ticketer,
        directory,
        log);

    auto deploy_contract_key = cbdc::buffer();
    deploy_contract_key.append("deploy", 6);

    auto deploy_contract
        = cbdc::buffer::from_hex(
              "1b4c7561540019930d0a1a0a040808785600000000000000000000002877400"
              "1808187010004968b0000028e0001030301020080010000c40003030f000102"
              "0f0000018b0000068e0001070b010000c40002020f000501930000005200000"
              "00f0008018b0000080b0100008b010001900002038b000008c8000200c70001"
              "008904846b6579048566756e630487737472696e670487756e7061636b04837"
              "373048276048a636f726f7574696e6504867969656c64048274810000008080"
              "808080")
              .value();

    cbdc::test::add_to_shard(broker, deploy_contract_key, deploy_contract);
}
