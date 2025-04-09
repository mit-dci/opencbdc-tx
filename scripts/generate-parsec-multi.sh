#!/bin/bash

IP="localhost" # 127.0.0.1
port_alloc_agent=20000
port_alloc_tmc=30000
port_alloc_shard=40000 # increment by += 2
port_alloc_raft=50000
LOGLEVEL="WARN"
RUNNER_TYPE="evm"

# FIXME: update args later
function print_help() {
    echo "Usage: ./scripts/generate-parsec-multi.sh > launch-parsec-multi.sh"
    echo ""
    echo "OPTIONS:"
    echo "  --ip           The IP address to use. Default is localhost."
    echo "  --port         The port number to use. Default is 8888."
    echo "  --loglevel     The log level to use. Default is WARN."
    echo "  --runner_type  The runner type to use in the agent. Defaults to EVM."
    echo "  -h, --help     Show this help message and exit."
    echo ""
}

# FIXME: update args later
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
# FIXME: multiple ports...
# echo Running agent on $IP:$port_alloc_agent
# echo Log level = $LOGLEVEL
# echo Runner type = $RUNNER_TYPE

# Parsec configurations
# FIXME: don't hardcode these values in future
num_agents=10

repl_factor_tmc=3
num_tmcs=$((1 * repl_factor_tmc))

num_shards_non_repl=10
repl_factor_shards=3
num_shards_repl="$repl_factor_shards"
num_shards_total=$(( num_shards_non_repl * num_shards_repl ))

all_shell_cmds=()

# helper functions
function build_shard_line() {
    # build out line like this:
    # --shard_count=1 --shard0_count=1 --shard00_endpoint=$IP:5556 \
    # input: shard port id
    local id_shard=$1
    local id_shard_repl=$2
    local port_shard=$3
    local line=""
    line+=" --shard_count=$num_shards_non_repl"
    line+=" --shard${id_shard}_count=$num_shards_repl"
    line+=" --shard${id_shard}${id_shard_repl}_endpoint=$IP:$port_shard"
    # FIXME: test if captured correctly into shell cmd data structure
    echo "$line"
    # printf "%s\n" "$line"
}

function build_raft_line() {
    # build out line like this:
    # --shard00_raft_endpoint=$IP:5557
    # inputs:
    # idx_shard: shard id
    # idx_repl: replica id
    # port_raft: raft port id
    local idx_shard=$1
    local idx_repl=$2
    local port_raft=$3
    local line=""
    line+=" --shard${idx_shard}${idx_repl}_raft_endpoint=$IP:$port_raft"

    printf "%s\n" "$line"
}

function build_agent_line() {
    # build out line like this:
    # --agent_count=4 --agent0_endpoint=$IP:$PORT --agent1_endpoint=$IP:$PORT2 --agent2_endpoint=$IP:$PORT3 --agent3_endpoint=$IP:$PORT4 \
    # inputs:
    # idx_agent: agent id
    # increment by += 1 for agent port id from base port
    local idx_agent=$1
    local line=""
    line+=" --agent_count=$num_agents"
    line+=" --agent${idx_agent}_endpoint=$IP:$((port_alloc_agent + idx_agent))"

    printf "%s\n" "$line"
}

function build_tmc_line() {
    # build out line like this:
    # --ticket_machine_count=1 --ticket_machine0_endpoint=$IP:7777
    # input: tmc port id
    idx_tmc=$1
    port_tmc=$2
    local line=""
    line+=" --ticket_machine_count=$num_tmcs"
    line+=" --ticket_machine${idx_tmc}_endpoint=$IP:$port_tmc"

    printf "%s\n" "$line"
}

# main script
line_log=" --loglevel=$LOGLEVEL"
line_runner=" --runner_type=$RUNNER_TYPE"

# FIXME: think about fixing O(n^4) runtime lol
# ticket machine replication loop
for (( idx_tmc=0; idx_tmc<repl_factor_tmc; idx_tmc++ )); do
    tmc_port_id=$(( port_alloc_tmc + idx_tmc ))

    # agent loop
    for (( idx_agent=0; idx_agent<num_agents; idx_agent++ )); do
        agent_port_id=$(( port_alloc_agent + idx_agent ))
        line_component=" --component_id=$idx_agent"

        # shards loop
        for (( idx_shard=0; idx_shard<num_shards_non_repl; idx_shard++ )); do

            # shard replicas loop
            for (( idx_repl=0; idx_repl<num_shards_repl; idx_repl++ )); do

                shard_port_id=$(( port_alloc_shard + idx_shard * 2 ))
                line_shard=$(build_shard_line $idx_shard $idx_repl $shard_port_id)

                raft_port_id=$(( port_alloc_raft + idx_shard * 2 ))
                line_raft=$(build_raft_line $idx_shard $idx_repl $raft_port_id)

                line_tmc=$(build_tmc_line $idx_tmc $tmc_port_id)

                line_agent=$(build_agent_line $idx_agent)

                line_node=" --node_id=${idx_shard}" # FIXME, cluster shard is in

                # FIXME: check if ports are available before generating shell commands
                # netstat etc
                shard_cmd="./build/src/parsec/runtime_locking_shard/runtime_locking_shardd"
                shard_cmd+="$line_shard"
                shard_cmd+="$line_raft"
                shard_cmd+="$line_tmc"
                shard_cmd+="$line_agent"
                shard_cmd+="$line_component"
                shard_cmd+="$line_node"
                shard_cmd+="$line_log"
                shard_cmd+="$line_runner"
                shard_cmd+=" > logs/shardd${idx_shard}-${idx_repl}.log 2>&1 &" # dash in case of double digits

                all_shell_cmds+=("$shard_cmd")

                # ticket_machined
                tmc_cmd="./scripts/wait-for-it.sh -s $IP:$shard_port_id -t 60 --"
                tmc_cmd+=" ./build/src/parsec/ticket_machine/ticket_machined"
                tmc_cmd+="$line_shard"
                tmc_cmd+="$line_tmc"
                tmc_cmd+="$line_agent"
                tmc_cmd+="$line_component"
                tmc_cmd+="$line_log"
                tmc_cmd+=" > logs/ticket_machined${idx_tmc}.log 2>&1 &"

                all_shell_cmds+=("$tmc_cmd")

                # can hardcode tmc port (line 0) for now as there is just 1
                agent_cmd="./scripts/wait-for-it.sh -s $IP:$tmc_port_id -t 60 --"
                agent_cmd+=" ./scripts/wait-for-it.sh -s $IP:$shard_port_id -t 60 --"
                agent_cmd+=" ./build/src/parsec/agent/agentd"
                agent_cmd+="$line_shard"
                agent_cmd+="$line_tmc"
                agent_cmd+="$line_agent"
                agent_cmd+="$line_component"
                agent_cmd+="$line_log"
                agent_cmd+="$line_runner"
                agent_cmd+=" > logs/agentd${idx_agent}.log 2>&1 &"

                all_shell_cmds+=("$agent_cmd")

            done
        done
    done
done

printf "#!/bin/bash\n\n"
for cmd in "${all_shell_cmds[@]}"; do
    printf "%s\n" "$cmd"
    printf "sleep 1\n\n"
done
