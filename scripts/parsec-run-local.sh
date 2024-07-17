#!/bin/bash

IP="localhost"
PORT="8888"
RUNNER_TYPE="evm"
LOGLEVEL="WARN"

function print_help() {
    echo "Usage: parsec-run-local.sh [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  --ip           The IP address to use. Default is localhost."
    echo "  --port         The port number to use. Default is 8888."
    echo "  --loglevel     The log level to use. Default is WARN."
    echo "  --runner_type  The runner type to use in the agent. Defaults to EVM."
    echo "  -h, --help     Show this help message and exit."
    echo ""
}

for arg in "$@"; do
    if [[ "$arg" == "-h" || "$arg" == "--help" ]]; then
        print_help
        exit 0
    elif [[ "$arg" == "--runner_type"* ]]; then
        if [[ "$arg" == "--runner_type=lua" ]]; then
            RUNNER_TYPE="lua"
        elif [[ "$arg" != "--runner_type=evm" ]]; then
            echo "unknown runner type, using evm"
        fi
    elif [[ "$arg" == "--ip"* ]]; then
        IP="${arg#--ip=}"
    elif [[ "$arg" == "--port"* ]]; then
        PORT="${arg#--port=}"
    elif [[ "$arg" == "--loglevel"* ]]; then
        LOGLEVEL="${arg#--loglevel=}"
    fi
done

mkdir -p logs
echo Running agent on $IP:$PORT
echo Log level = $LOGLEVEL
echo Runner type = $RUNNER_TYPE

./build/src/parsec/runtime_locking_shard/runtime_locking_shardd --shard_count=1 \
    --shard0_count=1 --shard00_endpoint=$IP:5556 \
    --shard00_raft_endpoint=$IP:5557 --node_id=0 --component_id=0 \
    --agent_count=1 --agent0_endpoint=$IP:6666 --ticket_machine_count=1 \
    --ticket_machine0_endpoint=$IP:7777 --loglevel=$LOGLEVEL \
    > logs/shardd.log &
sleep 1
./scripts/wait-for-it.sh -s $IP:5556 -t 60 -- \
    ./build/src/parsec/ticket_machine/ticket_machined --shard_count=1 \
    --shard0_count=1 --shard00_endpoint=$IP:5556 --node_id=0 \
    --component_id=0 --agent_count=1 --agent0_endpoint=$IP:6666 \
    --ticket_machine_count=1 --ticket_machine0_endpoint=$IP:7777 \
    --loglevel=$LOGLEVEL > logs/ticket_machined.log &
sleep 1
# create jail
# copy any desired binaries/dependencies - /agentd, /dev/urandom
# enter chroot environment and call agentd
sudo chroot --userspec=$(id -u $USER):$(id -u $USER) /sandbox /busybox sh


./scripts/wait-for-it.sh -s $IP:7777 -t 60 -- ./scripts/wait-for-it.sh -s \
    $IP:5556 -t 60 -- ./build/src/parsec/agent/agentd --shard_count=1 \
    --shard0_count=1 --shard00_endpoint=$IP:5556 --node_id=0 --component_id=0 \
    --agent_count=1 --agent0_endpoint=$IP:$PORT --ticket_machine_count=1 \
    --ticket_machine0_endpoint=$IP:7777 --loglevel=$LOGLEVEL \
    --runner_type=$RUNNER_TYPE > logs/agentd.log &
