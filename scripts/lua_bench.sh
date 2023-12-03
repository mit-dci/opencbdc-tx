#!/bin/bash

# A sample script for running the Lua bench test

IP="localhost"
PORT="8889"
N_WALLETS=2

./build/tools/bench/parsec/lua/lua_bench --component_id=0 \
    --ticket_machine0_endpoint=$IP:7777 --ticket_machine_count=1 \
    --shard_count=1 --shard0_count=1 --shard00_endpoint=$IP:5556 \
    --agent_count=1 --agent0_endpoint=$IP:$PORT \
    --loglevel=TRACE scripts/gen_bytecode.lua $N_WALLETS
echo done
