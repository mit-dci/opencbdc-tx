// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "parsec/agent/client.hpp"
#include "parsec/broker/impl.hpp"
#include "parsec/directory/impl.hpp"
#include "parsec/runtime_locking_shard/client.hpp"
#include "parsec/ticket_machine/client.hpp"
#include "parsec/util.hpp"
#include "wallet.hpp"

#include <future>
#include <lua.hpp>
#include <random>
#include <thread>

auto main(int argc, char** argv) -> int {
    auto log = std::make_shared<cbdc::logging::log>(
        cbdc::logging::log_level::trace);

    if(argc < 2) {
        log->error("Not enough arguments");
        return 1;
    }
    auto shard_cfg = cbdc::parsec::read_shard_info(argc - 3, argv);
    if(!shard_cfg.has_value()) {
        log->error("Error parsing shard options");
        return 1;
    }

    auto ticket_machine_cfg
        = cbdc::parsec::read_ticket_machine_info(argc - 3, argv);
    if(!ticket_machine_cfg.has_value()) {
        log->error("Error parsing ticket machine options");
        return 1;
    }

    log->trace("Connecting to shards");

    auto shards = std::vector<
        std::shared_ptr<cbdc::parsec::runtime_locking_shard::interface>>();

    for(const auto& shard_ep : shard_cfg->m_shard_endpoints) {
        auto client = std::make_shared<
            cbdc::parsec::runtime_locking_shard::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{shard_ep});
        if(!client->init()) {
            log->error("Error connecting to shard");
            return 1;
        }
        shards.emplace_back(client);
    }

    log->trace("Connected to shards");

    log->trace("Connecting to ticket machine");
    auto ticketer
        = std::make_shared<cbdc::parsec::ticket_machine::rpc::client>(
            std::vector<cbdc::network::endpoint_t>{
                ticket_machine_cfg->m_ticket_machine_endpoints});
    if(!ticketer->init()) {
        log->error("Error connecting to ticket machine");
        return 1;
    }
    log->trace("Connected to ticket machine");

    auto directory
        = std::make_shared<cbdc::parsec::directory::impl>(shards.size());
    auto broker = std::make_shared<cbdc::parsec::broker::impl>(
        std::numeric_limits<size_t>::max(),
        shards,
        ticketer,
        directory,
        log);

    auto args = cbdc::config::get_args(argc, argv);
    auto compile_file = args[args.size() - 3];
    auto contract_file = args[args.size() - 2];
    auto func_name = args[args.size() - 1];

    auto contract = cbdc::buffer();
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, compile_file.c_str());
    luaL_dofile(L, contract_file.c_str());

    lua_getglobal(L, "gen_bytecode");
    lua_getglobal(L, func_name.c_str());

    if(lua_pcall(L, 1, 1, 0) != 0) {
        log->error("Contract bytecode generation failed, with error:",
                   lua_tostring(L, -1));
        return 1;
    }

    contract = cbdc::buffer::from_hex(lua_tostring(L, -1)).value();
    log->trace(contract.to_hex());
    auto pay_contract_key = cbdc::buffer();
    pay_contract_key.append("con", 3);

    auto prom = std::promise<void>();
    auto fut = prom.get_future();
    log->info("Inserting pay contract");
    auto ret = cbdc::parsec::put_row(
        broker,
        pay_contract_key,
        contract,
        [&](bool res) {
            if(!res) {
                log->error("failed to instert pay contract");
            } else {
                log->info("Inserted pay contract");
            }
            prom.set_value();
        });
    fut.get();
    log->trace(ret);
    return 0;
}
