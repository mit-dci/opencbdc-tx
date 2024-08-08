#!/bin/bash

IP="localhost"
PORT="8889"
N_WALLETS=5
LOGLEVEL="WARN"

function print_help() {
    echo "Usage: lua_bench.sh [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  --ip           The IP address to use. Default is localhost."
    echo "  --port         The port number to use. Default is 8888."
    echo "  --loglevel     The log level to use. Default is WARN."
    echo "  -h, --help     Show this help message and exit."
    echo ""
}

for arg in "$@"; do
    if [[ "$arg" == "-h" || "$arg" == "--help" ]]; then
        print_help
        exit 0
    elif [[ "$arg" == "--ip"* ]]; then
        IP="${arg#--ip=}"
    elif [[ "$arg" == "--port"* ]]; then
        PORT="${arg#--port=}"
    elif [[ "$arg" == "--loglevel"* ]]; then
        LOGLEVEL="${arg#--loglevel=}"
    fi
done
./build/tools/bench/parsec/lua/lua_bench --component_id=0 \
    --ticket_machine0_endpoint="$IP":7777 --ticket_machine_count=1 \
    --shard_count=1 --shard0_count=1 --shard00_endpoint="$IP":5556 \
    --agent_count=1 --agent0_endpoint="$IP":"$PORT" \
    --loglevel="$LOGLEVEL" scripts/gen_bytecode.lua $N_WALLETS
echo "done"; echo
