#!/usr/bin/env python3

"""
This script enables local running of parsec with the following features:
- Launching a single agent, shard, and ticket machine
- Launching multiple agents, shards, and ticket machines
- Allowing replication [shards and ticket machines]
- - to support the Raft consensus protocol
- Killing all running processes

Example:
$ python3 parsec-run-local.py --ip localhost --port 8888 --log_level DEBUG --runner_type evm
    --num_agents 100 --num_shards 100 --num_ticket_machines 10 --replication_factor 3
# this will launch 100 agents, 300 shards, 30 ticket machines
# 100 logical shards and shard clusters
# 300 physical shards
# 100 agents
# 30 ticket machines
"""

import os
import sys
import argparse
import logging
from datetime import datetime
import time
import subprocess
from functools import wraps
import shlex
from math import sqrt, ceil
import socket
import heapq
# FIXME: either use or remove unused imports
import multiprocessing
import threading
import asyncio
import concurrent.futures


# pylint: disable=C0103
# using caps to mimic the bash script
IP = "localhost"
PORT = 8888
LOG_LEVEL = "WARN"
RUNNER_TYPE = "evm" # lua, evm, pyrunner (future)

NUM_AGENTS, NUM_SHARDS, NUM_TICKET_MACHINES, REPL_FACTOR = 1, 1, 1, 1

# mac doesn't connect to ports above 10k it seems, memory error
# look into this more, possibly a firewall issue
# max usage is 5000 ports of the possible 10k ports per machine type
# built in buffer in case some ports are unavailable
AGENT_PORT_ID =         20000
SHARD_EP_PORT_ID =      30000
SHARD_RAFT_EP_PORT_ID = 40000
TMC_PORT_ID =           50000

# pylint: enable=C0103

# min-heaps to store + kill the pids of the processes
pids_shards_min_heap = heapq.heapify([])
pids_agents_min_heap = heapq.heapify([])
pids_tmcs_min_heap =   heapq.heapify([])

# FIXME: add root directory and path checking
logger = logging.getLogger(__name__)
logger_shard = logging.getLogger("shardd")
logger_tmc = logging.getLogger("ticket_machined")
logger_agent = logging.getLogger("agentd")


def parse_args() -> argparse.Namespace:
    """
    Overwrite the default values if a user chooses
    """
    parser = argparse.ArgumentParser(description="Run a local Parsec agent")
    # pylint: disable=C0301
    parser.add_argument("--ip", type=str, default="localhost", dest="IP",
                        help="The IP address to use. Default is localhost.")
    parser.add_argument("--port", type=int, default=8888, dest="PORT",
                        help="The port number to use. Default is 8888.")
    parser.add_argument("--log_level", type=str, default="WARN", dest="LOG_LEVEL",
                        help="The log level to use. Default is WARN. \
                        Choose from DEBUG, INFO, WARN, ERROR, CRITICAL.")
    parser.add_argument("--runner_type", type=str, default="evm", dest="RUNNER_TYPE",
                        help="The runner type to use in the agent. Defaults to EVM.")
    parser.add_argument("--num_agents", type=int, default=1, dest="NUM_AGENTS",
                        help="The number of agents to run. Defaults to 1.")
    parser.add_argument("--num_shards", type=int, default=1, dest="NUM_SHARDS",
                        help="The number of shards to run. Defaults to 1.")
    parser.add_argument("--num_ticket_machines", type=int, default=1, dest="NUM_TMCS",
                        help="The number of ticket machines to run. Defaults to 1.")
    parser.add_argument("--replication_factor", type=int, default=1, dest="REPL_FACTOR",
                        help="The replication factor to use. Defaults to 1.")
    parser.add_argument("--kill_pids", type=bool, default=False, dest="KILL_PIDS",
                        help="Kill the processes strictly in the order they were created.")
    parser.add_argument("--kill_pnames", type=bool, default=False, dest="KILL_PNAMES",
                        help="Kill the processes by name such as agentd, shardd, ticket_machined.")
    # pylint: enable=C0301
    return parser.parse_args()


def initiate_logging():
    """
    Set up and log the initial configuration message.

    This function logs various configuration details including:
    - IP address and port
    - Log level
    - Number of agents and shards
    """
    if LOG_LEVEL in ["DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"]:
        logging.basicConfig(level=getattr(logging, LOG_LEVEL))
    else:
        logging.basicConfig(level=logging.WARN)

    curr_time = datetime.now()
    fmt_curr_time = curr_time.strftime("%Y-%m-%d_%H-%M-%S")
    os.makedirs("logs", exist_ok=True)

    # Main logger setup
    if LOG_LEVEL in ["DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"]:
        logging.basicConfig(level=getattr(logging, LOG_LEVEL))
    else:
        logging.basicConfig(level=logging.WARN)

    curr_time = datetime.now()
    # fmt_curr_time = curr_time.strftime("%Y-%m-%d_%H-%M-%S")
    os.makedirs("logs", exist_ok=True)

    # Main logger file handler
    handler = logging.FileHandler("logs/parsec-run-local.log")
    formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
    handler.setFormatter(formatter)
    logger.addHandler(handler)

    # Setup for shard logger
    handler_shard = logging.FileHandler("logs/shardd.log")
    handler_shard.setFormatter(formatter)
    logger_shard.addHandler(handler_shard)

    # Setup for ticket machine logger
    handler_tmc = logging.FileHandler("logs/ticket_machined.log")
    handler_tmc.setFormatter(formatter)
    logger_tmc.addHandler(handler_tmc)

    # Setup for agent logger
    handler_agent = logging.FileHandler("logs/agentd.log")
    handler_agent.setFormatter(formatter)
    logger_agent.addHandler(handler_agent)


def record_program_vars(kill_by_pids: bool, kill_by_pnames: bool):
    """
    Write the program variables to the log file
    """
    logger.info("Running agent on %s:%s", IP, PORT)
    logger.info("log level = %s", LOG_LEVEL)
    logger.info("Runner type = %s", RUNNER_TYPE)
    logger.info("-------------------------------")
    logger.info("Number of agents = %s", NUM_AGENTS)
    logger.info("Replication factor = %s", REPL_FACTOR)
    logger.info("Number of logical shards and shard clusters = %s", NUM_SHARDS)
    logger.info("Number of physical shards = %s", NUM_SHARDS * REPL_FACTOR)
    logger.info("Number of ticket machines = %s ", NUM_TMCS * REPL_FACTOR)
    logger.info("-------------------------------")
    logger.info("Kill by pids = %s", kill_by_pids)
    logger.info("Kill by process names = %s", kill_by_pnames)


def timeit(func):
    @wraps(func)
    def measure_time(*args, **kwargs):
        start_time = time.time()
        result = func(*args, **kwargs)
        end_time = time.time()
        print("@timefn: {} took {} seconds.\n".format(func.__name__, round(end_time - start_time, 2)))
        return result
    return measure_time


def check_num_ports_avail(ports_needed: list[int], start_port=20000, end_port=59999):
    """
    See if there are enough available ports to meet the user's demand
    using raw nums and replication factor to determine feasibility
    """
    total_num_ports = sum(ports_needed)
    # try nmap on the ports we can allocate to see if total num open ports is enough
    if end_port < start_port or min(start_port, end_port) < 1024 or max(start_port, end_port) > 65535:
        logger.error("Port bounds (%s-%s) out of range", start_port, end_port)
        sys.exit(-1)

    # FIXME: syntax read error
    result = subprocess.run(["nmap", "-p", f"{start_port}-{end_port}", IP],
                            check=True, stdout=subprocess.PIPE)
    open_ports = result.stdout.decode("utf-8").split("\n")
    num_open_ports = len(open_ports) - 2 # subtract 2 for the header and footer
    if num_open_ports < total_num_ports:
        logger.error("Not enough open ports to meet the user's demand")
        logger.error("Available ports: %d, Ports needed: %d", num_open_ports, total_num_ports)
        sys.exit(1)
    else:
        logger.info("Enough open ports to meet the user's demand: %d open ports", num_open_ports)


def check_port(port_id, max_attempts=100):
    """
    Find an open port starting from port_id.
    Try to find an open port in the range of max_num_test_ports.
    Return the open port if found, otherwise sys exit on -1.
    """
    for i in range(max_attempts):
        port_tested = port_id + i
        # don't try on reserved ports or ports out of bounds
        if port_tested < 1024 or port_tested > 65535:
            logger.error("Port id %d out of range", port_tested)
            sys.exit(1)

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            result_sock = sock.connect_ex((IP, port_tested))
            if result_sock != 0:
                logger.info("Open port %s found", port_tested)
                return int(port_tested)
        # elif subprocess.run(["/usr/bin/env", "bash", "-c", "nc", "localhost", \
        #                       str(port_tested)], check=True).returncode != 0:

    logger.error("Could not find an open port in range %d to %d",
                 port_id, port_id + max_attempts)
    sys.exit(1)


def setup(args) -> tuple[bool, bool]:
    """
    validate input args for program boundaries
    create logger and record program variables if log_level is INFO or stricter
    """
    kill_by_pids, kill_by_pnames = validate_args(args)

    initiate_logging()

    record_program_vars(kill_by_pids, kill_by_pnames)

    return (kill_by_pids, kill_by_pnames)


def validate_args(args, max_num_machines=5000) -> tuple[bool, bool]:
    """
    Validate the arguments provided by the user
    # FIXME: add type checking and check bounds for new variables
    """
    global IP, PORT, LOG_LEVEL, RUNNER_TYPE
    IP = args.IP
    PORT = args.PORT
    LOG_LEVEL = args.LOG_LEVEL
    RUNNER_TYPE = args.RUNNER_TYPE
    # see if indicated port is open and if not, try more
    PORT = check_port(PORT)
    print(f'{PORT = }; {type(PORT) = }')

    global NUM_AGENTS, REPL_FACTOR, NUM_SHARDS, NUM_TMCS
    NUM_AGENTS = args.NUM_AGENTS
    REPL_FACTOR = args.REPL_FACTOR
    NUM_SHARDS = args.NUM_SHARDS
    NUM_TMCS = args.NUM_TMCS

    num_lgc_shards = NUM_SHARDS
    num_phys_shards = NUM_SHARDS * REPL_FACTOR
    # FIXME: need to handle ticket machine replication
    num_tmcs = NUM_TMCS * REPL_FACTOR

    if any([NUM_AGENTS, NUM_SHARDS, NUM_TMCS, REPL_FACTOR]) < 1:
        logger.error("Number of agents, shards, ticket machines, and replication factor must be at least 1")
        sys.exit(1)
    elif any([NUM_AGENTS, num_phys_shards, num_tmcs]) > max_num_machines:
        logger.error("Number of agents, shards, ticket machines, and replication factor must be at most 5000")
        sys.exit(1)

    # before running parsec - see if enough ports are open as a preliminary filter
    check_num_ports_avail([num_phys_shards, num_lgc_shards, NUM_AGENTS, num_tmcs])

    kill_by_pids, kill_by_pnames = False, False
    if args.KILL_PIDS:
        kill_by_pids = True
        logger.info("Selecting to kill processes at end by pids")
    elif args.KILL_PNAMES:
        kill_by_pnames = True
        logger.info("Selecting to kill processes at end by process names")
    else:
        logger.info("Opting to keep processes running after program finishes")
    return (kill_by_pids, kill_by_pnames)


def check_cmd_and_return_pid(result_proc: subprocess.CompletedProcess, machine_name: str) -> int:
    """
    Check the return code of the process and return the pid if successful
    """
    # validate input type
    if not isinstance(result_proc, subprocess.CompletedProcess):
        logger.error("Invalid input type for result_proc")
        return None

    if result_proc.returncode == 0:
        pid = int(result_proc.stdout.strip())
        return pid
    else:
        logger.error("Failed to launch %s process: %s", machine_name, result_proc.stderr)

    return None


@timeit
def launch_basic_parsec():
    """
    designed for 1 shard, 1 shard cluster, 1 agent
    """
    shard_ep_port_id = check_port(SHARD_EP_PORT_ID, max_attempts=100)
    shard_raft_ep_port_id = check_port(SHARD_RAFT_EP_PORT_ID, max_attempts=100)
    agent_port_id = check_port(AGENT_PORT_ID, max_attempts=100)
    tmc_port_id = check_port(TMC_PORT_ID, max_attempts=100)
    # print all vars and their types
    print(f"{shard_ep_port_id = }; {type(shard_ep_port_id) = }")
    print(f"{shard_raft_ep_port_id = }; {type(shard_raft_ep_port_id) = }")
    print(f"{agent_port_id = }; {type(agent_port_id) = }")
    print(f"{tmc_port_id = }; {type(tmc_port_id) = }")

    # generate the commands to launch the processes for each shard, agent, ticket machine
    # log handling below not inside cmd itself
    cmd_shardd = [
        "./build/src/parsec/runtime_locking_shard/runtime_locking_shardd",
        "--shard_count=1", # number of shards
        "--shard0_count=1", # number of shard clusters
        f"--shard00_endpoint={IP}:{shard_ep_port_id}",
        f"--shard00_raft_endpoint={IP}:{shard_raft_ep_port_id}",
        "--node_id=0", # which node the cluster is this shard
        "--component_id=0", # which cluster is this shard in
        "--agent_count=1", # number of agents
        f"--agent0_endpoint={IP}:{agent_port_id}",
        "--ticket_machine_count=1", # number of ticket machines
        f"--ticket_machine0_endpoint={IP}:{tmc_port_id}",
        f"--log_level={LOG_LEVEL}"
    ]

    cmd_tmcd = [
        "./scripts/wait-for-it.sh", "-s", f"{IP}:{shard_ep_port_id}", "-t", "60", "--",
        "./build/src/parsec/ticket_machine/ticket_machined",
        "--shard_count=1",
        "--shard0_count=1",
        f"--shard00_endpoint={IP}:5556",
        "--node_id=0",
        "--component_id=0",
        "--agent_count=1",
        f"--agent0_endpoint={IP}:{agent_port_id}",
        "--ticket_machine_count=1",
        f"--ticket_machine0_endpoint={IP}:{tmc_port_id}",
        f"--log_level={LOG_LEVEL}"
    ]

    cmd_agentd = [
        "./scripts/wait-for-it.sh", "-s", f"{IP}:{tmc_port_id}", "-t", "60", "--",
        "./scripts/wait-for-it.sh", "-s", f"{IP}:{agent_port_id}", "-t", "60", "--",
        "./build/src/parsec/agent/agentd",
        "--shard_count=1",
        "--shard0_count=1",
        f"--shard00_endpoint={IP}:{shard_ep_port_id}",
        "--node_id=0",
        "--component_id=0",
        "--agent_count=1",
        f"--agent0_endpoint={IP}:{agent_port_id}",
        "--ticket_machine_count=1",
        f"--ticket_machine0_endpoint={IP}:{tmc_port_id}",
        f"--log_level={LOG_LEVEL}"
        f"--runner_type={RUNNER_TYPE}"
    ]

    # execute the commands
    result_shardd, result_tmcd, result_agentd = None, None, None
    logger.info("Attempting to launch shardd")
    try:
        with open('logs/shardd.log', 'a+') as log_shard:
            result_shardd = subprocess.run(cmd_shardd, check=True, text=True, \
                                        stdout=log_shard, stderr=log_shard)

    except subprocess.CalledProcessError as e:
        if e.stderr is not None:
            print("Error output:", e.stderr.decode())
        else:
            print("No error output available.")
    time.sleep(1)

    logger.info("Attempting to launch ticket machine")
    try:
        with open('logs/ticket_machined.log', 'a+') as log_tmc:
            result_tmcd = subprocess.run(cmd_tmcd, check=True, text=True, \
                                        stdout=log_tmc, stderr=log_tmc, timeout=5)
    except subprocess.CalledProcessError as e:
        if e.stderr is not None:
            print("Error output:", e.stderr.decode())
        else:
            print("No error output available.")
    time.sleep(1)

    logger.info("Attempting to launch agent")
    try:
        with open('logs/agentd.log', 'a+') as log_agent:
            result_agentd = subprocess.run(cmd_agentd, check=True, text=True, \
                                        stdout=log_agent, stderr=log_agent, timeout=5)
    except subprocess.CalledProcessError as e:
        if e.stderr is not None:
            print("Error output:", e.stderr.decode())
        else:
            print("No error output available.")

    # capture the pids of the processes to kill now on failure or later on teardown
    pid_shardd = check_cmd_and_return_pid(result_shardd, "shardd")
    pid_tmcd =   check_cmd_and_return_pid(result_tmcd,   "ticket_machined")
    pid_agentd = check_cmd_and_return_pid(result_agentd, "agentd")

    if not any([result_shardd, result_tmcd, result_agentd]):
        logger.error("Failed to launch all processes")
        logger.error("Undoing this batch of launches")
        for pid in [pid_shardd, pid_tmcd, pid_agentd]:
            if pid is not None:
                kill_pid(pid)
    else:
        heapq.heappush(pids_shards_min_heap, pid_shardd)
        heapq.heappush(pids_tmcs_min_heap,   pid_tmcd)
        heapq.heappush(pids_agents_min_heap, pid_agentd)

        # FIXME: create log file for each category of machine (shardd, ticket_machined, agentd)?
        logger.info("pid = %s created; shardd", pid_shardd)
        logger.info("pid = %s created; ticket_machined", pid_tmcd)
        logger.info("pid = %s created; agentd", pid_agentd)

    # think about user argument and do we want to do teardown automatically or let user choose
    # we can also run teardown to supercede the user choice and kill all processes
    # matching runtime_locking_shardd, ticket_machined, agentd
    # at the start of the program - but proceed with caution
    # or even specify how long we want parsec processes active before killing this script + all processes

@timeit
def launch_full_parsec():
    """
    Launch multiple agents, shards, and ticket machines
    FIXME: explain what this function does
    # poor runtime complexity - can split into asyncio or threading to speed up
    # do several batches of launches and specify starting port IDs / ranges
    # if our user commands demand a decent amount of machines like 100 +
    """
    num_active_agents, num_active_tmcs, num_active_shards = 0, 0, 0

    req_lgc_shards = NUM_SHARDS
    req_phys_shards = NUM_SHARDS * REPL_FACTOR
    req_num_tmcs = NUM_TICKET_MACHINES * REPL_FACTOR

    prev_agent_port = AGENT_PORT_ID
    prev_tmc_port = TMC_PORT_ID
    prev_shard_port = SHARD_EP_PORT_ID
    prev_shard_raft_port = SHARD_RAFT_EP_PORT_ID

    for agent_num in range(NUM_AGENTS):
        agent_port = check_port(prev_agent_port + 1, max_attempts=5)

        # FUXNE: unused variables ticket_machine_num, repl_tmc_num
        for _ in range(req_num_tmcs):
        # for ticket_machine_num in range(req_num_tmcs):
            # FIXME: we don't have support for ticket machine replication yet in the bash code
            # for repl_tmc_num in range(REPL_FACTOR):
            curr_tmc_num = num_active_tmcs
            tmc_port = check_port(prev_tmc_port + 1, max_attempts=5)

            # shard_count  - the number of shard clusters
            # shardN_count - the number of shard replicas in the Nth cluster
            # node_id      - which node the cluster is this shard
            # component_id - which cluster is this shard in

            for shard_lgc_num in range(NUM_SHARDS): # aka cluster ID
                for shard_repl_num in range(REPL_FACTOR): # replica count, shard ID within cluster

                    cluster_id = shard_lgc_num
                    shard_phys_num = shard_repl_num
                    shard_ep_port = check_port(prev_shard_port + 1, max_attempts=5)
                    shard_cluster_raft_ep_port = check_port(prev_shard_raft_port + 1, max_attempts=5)

                    # FIXME: validate user input before subprocess runs as that can be a security risk
                    # pylint: disable=C0301
                    # gemerate the commands to launch the processes for each shard, agent, ticket machine
                    cmd_shardd = [
                        "./build/src/parsec/runtime_locking_shard/replicated_shard",
                        f"--shard_count={req_phys_shards}",
                        f"--shard{shard_lgc_num}_count={req_lgc_shards}", # number of shard replicas per cluster
                        f"--shard{shard_lgc_num}-{shard_phys_num}_endpoint={IP}:{shard_ep_port}",
                        f"--shard{shard_lgc_num}-{shard_phys_num}_raft_endpoint={IP}:{shard_cluster_raft_ep_port}",
                        f"--node_id={num_active_shards}",
                        f"--component_id={cluster_id}",
                        f"--agent_count={NUM_AGENTS} --agent{agent_num}_endpoint={IP}:{agent_port}",
                        f"--ticket_machine_count={req_num_tmcs}"
                        f"--ticket_machine{curr_tmc_num}_endpoint={IP}:{tmc_port}",
                        f"--log_level={LOG_LEVEL}", "&"
                    ]

                    cmd_tmcd = [
                        "-s", f"{IP}:{shard_ep_port}", "-t", "60", "--",
                        "./build/src/parsec/ticket_machine/ticket_machined",
                        f"--shard_count={req_phys_shards}",
                        f"--shard{shard_lgc_num}_count={req_lgc_shards}",
                        f"--shard{shard_lgc_num}-{shard_phys_num}_endpoint={IP}:{shard_ep_port}",
                        f"--node_id={num_active_shards}",
                        f"--component_id={cluster_id}",
                        f"--agent_count={NUM_AGENTS}",
                        f"--agent0_endpoint={IP}:{agent_port}",
                        f"--ticket_machine_count={req_num_tmcs}",
                        f"--ticket_machine{curr_tmc_num}_endpoint={IP}:{tmc_port}",
                        f"--log_level={LOG_LEVEL}", "&"
                    ]

                    cmd_agentd = [
                        "-s", f"{IP}:{tmc_port}", "-t", "60", "--",
                        "-s", f"{IP}:{shard_ep_port}", "-t", "60", "--",
                        "./build/src/parsec/agent/agentd",
                        f"--shard_count={req_phys_shards}",
                        f"--shard{shard_lgc_num}_count={req_lgc_shards}",
                        f"--shard{shard_lgc_num}-{shard_phys_num}_endpoint={IP}:{shard_ep_port}",
                        f"--node_id={num_active_shards}",
                        f"--component_id={cluster_id}",
                        f"--agent_count={NUM_AGENTS}",
                        f"--agent{agent_num}_endpoint={IP}:{agent_port}",
                        f"--ticket_machine_count={req_num_tmcs}",
                        f"--ticket_machine{curr_tmc_num}_endpoint={IP}:{tmc_port}",
                        f"--log_level={LOG_LEVEL}",
                        f"--runner_type={RUNNER_TYPE}"
                    ]
                    # pylint: enable=C0301

                    # execute the commands
                    result_shardd, result_tmcd, result_agentd = None, None, None
                    # either all work and update variables and pid min-heaps
                    # or gen new ports and retry, unroll any processes that were launched

                    # FIXME: add logs for each machine later
                    # (line ~20 at beginning except add IDs to machine type)
                    # f"logs/shardd_{shard_lgc_num}-{shard_lgc_num}.log",
                    # f"logs/ticket_machined_{curr_tmc_num}.log",
                    # f"logs/agentd_{agent_num}.log"

                    logger.info("Attempting to launch shardd")
                    try:
                        with open('logs/shardd.log', 'a+') as log_shard:
                            result_shardd = subprocess.run(cmd_shardd, check=True, text=True, \
                                                            stdout=log_shard, stderr=log_shard)
                    except subprocess.CalledProcessError as e:
                        if e.stderr is not None:
                            print("Error output:", e.stderr.decode())
                        else:
                            print("No error output available.")
                    # FIXME: test min reasonable sleep time and adjust wait period
                    time.sleep(0.2)

                    logger.info("Attempting to launch ticket machine")
                    try:
                        with open('logs/ticket_machined.log', 'a+') as log_tmc:
                            result_tmcd = subprocess.run(cmd_tmcd, check=True, text=True, \
                                                        stdout=log_tmc, stderr=log_tmc, timeout=5)
                    except subprocess.CalledProcessError as e:
                        if e.stderr is not None:
                            print("Error output:", e.stderr.decode())
                        else:
                            print("No error output available.")
                    time.sleep(1)

                    logger.info("Attempting to launch agent")
                    try:
                        with open('logs/agentd.log', 'a+') as log_agent:
                            result_agentd = subprocess.run(cmd_agentd, check=True, text=True, \
                                                        stdout=log_agent, stderr=log_agent, timeout=5)
                    except subprocess.CalledProcessError as e:
                        if e.stderr is not None:
                            print("Error output:", e.stderr.decode())
                        else:
                            print("No error output available.")

                    pid_shardd = check_cmd_and_return_pid(result_shardd, "shardd")
                    pid_tmcd =   check_cmd_and_return_pid(result_tmcd,   "ticket_machined")
                    pid_agentd = check_cmd_and_return_pid(result_agentd, "agentd")

                    # by doing this we can accurately hit our target counts for each machine
                    # FIXME: add in a while loop countdown after this to launch any remaining process batches
                    if any([result_shardd.returncode, result_tmcd.returncode, result_agentd.returncode]) is None:
                        logger.error("Failed to launch all processes")
                        logger.error("Undoing this batch of launches")
                        for pid in [pid_shardd, pid_tmcd, pid_agentd]:
                            if pid is not None:
                                kill_pid(pid)

                    # successful launch
                    else:
                        heapq.heappush(pids_shards_min_heap, pid_shardd)
                        heapq.heappush(pids_tmcs_min_heap,   pid_tmcd)
                        heapq.heappush(pids_agents_min_heap, pid_agentd)

                        num_active_shards += 1
                        num_active_agents += 1
                        num_active_tmcs += 1

                        logger.info("pid = %s created; shardd #%i", pid_shardd, num_active_shards)
                        logger.info("pid = %s created; ticket_machined #%i", pid_tmcd, num_active_tmcs)
                        logger.info("pid = %s created; agentd #%i", pid_agentd, num_active_agents)

                        prev_agent_port, prev_tmc_port, prev_shard_port, prev_shard_raft_port = \
                            agent_port, tmc_port, shard_ep_port, shard_cluster_raft_ep_port


    # did we create the desired number of agents and ticket machines and shards?
    if num_active_tmcs < NUM_TICKET_MACHINES * REPL_FACTOR:
        logger.error("Total ticket machine count does not match expected ticket machine count")

    if num_active_agents < NUM_AGENTS:
        logger.error("Total agent count does not match expected agent count")

    if num_active_shards < NUM_SHARDS * REPL_FACTOR:
        logger.error("Total shard count does not match expected shard count")

    # FIXME: we can do a while loop here to retry the failed launches or just exit or change the for-loops
    # could build in redundancy for the port allocation and process launching like ports wanted * 1.2


def show_min_heap(min_heap: list[int]) -> None:
    """
    Show the min heap in binary search tree format
    """
    def print_tree(heap, index=0, prefix="", is_left=True):
        if index < len(heap):
            print(prefix + ("└── " if is_left else "┌── ") + str(heap[index]))
            new_prefix = prefix + ("    " if is_left else "│   ")
            print_tree(heap, 2 * index + 2, new_prefix, False)
            print_tree(heap, 2 * index + 1, new_prefix, True)

    print("Min Heap as Binary Search Tree:")
    print_tree(min_heap)


def kill_pid(pid: int) -> None:
    """
    Kill a pid and log the error if it fails
    """
    if pid is not None:
        try:
            subprocess.run(["/usr/bin/env", "bash", "-c", "kill", str(pid)], check=True)
        except (subprocess.SubprocessError, OSError) as e:
            logger.error("Failed to kill pid %s: %s", pid, e)


@timeit
def kill_pids_in_parallel(pids_min_heap: list[int]) -> None:
    """
    Kill a pids in parallel by popping from the pids min-heap
    """
    num_processes = ceil(sqrt(len(pids_min_heap)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_processes) \
                                                            as executor:
        while pids_min_heap:
            pid = heapq.heappop(pids_min_heap)
            executor.submit(kill_pid, pid)

@timeit
def kill_pnames_in_parallel(pname: str) -> None:
    """
    caution: greedy approach
    Find process names matching agentd, shardd, ticket
    and kill all corresponding pids
    """
    if pname not in ["agentd", "shardd", "ticket_machined"]:
        logger.error("Invalid process name %s", pname)
        return

    proc_name = shlex.quote(pname)
    cmd_grep = f"pgrep -f {proc_name}"
    grep_output = subprocess.run(["/usr/bin/env", "bash", "-c", cmd_grep],
                                    check=True, capture_output=True, text=True).stdout
    kill_list = [ int(pid) for pid in grep_output.split() ]

    num_processes = ceil(sqrt(len(kill_list)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_processes) as executor:
        for pid in kill_list:
            executor.submit(kill_pid, pid)


@timeit
def teardown(kill_pids: bool, kill_pnames: bool) -> None:
    """
    Kill all running processes if user requests
    prefer by pids since those will only stem from this current program's execution
    """
    if not kill_pids and not kill_pnames:
        return

    logger.info("Shutting down processes")
    if kill_pids:
        logger.info("Killing shards")
        kill_pids_in_parallel(pids_shards_min_heap)
        logger.info("Killing agents")
        kill_pids_in_parallel(pids_agents_min_heap)
        logger.info("Killing ticket machine processes")
        kill_pids_in_parallel(pids_tmcs_min_heap)
    elif kill_pnames:
        logger.info("Killing processes by name")
        logger.info("Killing shards")
        kill_pnames_in_parallel("runtime_locking_shardd")
        logger.info("Killing agents")
        kill_pnames_in_parallel("agentd")
        logger.info("Killing ticket machine processes")
        kill_pnames_in_parallel("ticket_machined")
    logger.info("Processes killed")
    logger.info("Teardown complete")


def main() -> None:
    """
    Main function to run the program
    """
    args = parse_args()

    kill_by_pids, kill_by_pnames = setup(args)

    if max(NUM_AGENTS, NUM_SHARDS, REPL_FACTOR, NUM_TICKET_MACHINES) == 1:
        launch_basic_parsec()
    else:
        launch_full_parsec()

    if LOG_LEVEL == "DEBUG":
        show_min_heap(pids_shards_min_heap)
        show_min_heap(pids_agents_min_heap)
        show_min_heap(pids_tmcs_min_heap)

    teardown(kill_by_pids, kill_by_pnames)


if __name__ == "__main__":
    main()
