#!/usr/bin/env python3

import os
import sys
import argparse
import logging
import time
from datetime import datetime
from math import sqrt, ceil
from collections import OrderedDict
import csv
import re
import queue
import heapq
from functools import wraps
import subprocess
import socket
# FIXME: either use or remove unused imports
import multiprocessing
import threading
import asyncio
import concurrent.futures


# FIXME: need to source .py_venv/bin/activate # before running this
"""
This script enables local running of parsec with the following features:
- Launching a single agent, shard, and ticket machine
- Launching multiple agents, shards, and ticket machines
- Allowing replication [shards and ticket machines]
- - to support the Raft consensus protocol
- Killing all running parsec processes

Example:
$ source .py_venv/bin/activate
$ (.py_venv) python3 parsec-run-local.py --ip localhost --port 8888 --log_level DEBUG --runner_type evm
    --num_agents 100 --num_shards 100 --num_ticket_machines 10 --replication_factor 3
# this will launch 100 agents, 300 shards, 30 ticket machines
# 100 logical shards and shard clusters
# 300 physical shards
# 100 agents
# 30 ticket machines
"""

# required GLOBALS before __main__ located here
ROOT = os.path.abspath(os.path.dirname(os.path.join(__file__, "../../")))


class ProgramArgs:
    def __init__(self):
        """
        default values for the program arguments
        """
        # pylint: disable=C0103
        self.IP: str =          "127.0.0.1" # "localhost"
        self.PORT: int =        8888
        self.LOG_LEVEL: str =   "WARN"
        self.RUNNER_TYPE: str = "evm"
        self.NUM_AGENTS: int =  1
        self.NUM_SHARDS: int =  1
        self.NUM_TMCS: int =    1
        self.REPL_FACTOR: int = 1
        self.KILL_PIDS: bool =  False

        # parse cli args
        self.parse_args()

        # kill and exit. No need to run the rest of the program
        if self.KILL_PIDS:
            Pids = ProcessIDs()
            Pids.kill_pids_from_ps()
            sys.exit(0)

        # ensure types are correct
        self.type_checks()
        # ensure values are within bounds such as port numbers and IP addresses
        self.bounds_check()
        # FIXME: consider option of reading from config file in the future

    def parse_args(self) -> None:
        """
        Overwrite the default values if a user chooses
        """
        parser = argparse.ArgumentParser(description="Run a local Parsec agent")
        # pylint: disable=C0301
        parser.add_argument("--ip", type=str, default=self.IP,
                            help="The IP address to use. Default is 127.0.0.1")
        parser.add_argument("--port", type=int, default=self.PORT,
                            help="The port number to use. Default is 8888.")
        parser.add_argument("--log_level", type=str, default=self.LOG_LEVEL,
                            help="The log level to use. Default is WARN")
        parser.add_argument("--runner_type", type=str, default=self.RUNNER_TYPE,
                            help="The runner type to use. Default is evm")
        parser.add_argument("--num_agents", type=int, default=self.NUM_AGENTS,
                            help="The number of agents to use. Default is 1")
        parser.add_argument("--num_shards", type=int, default=self.NUM_SHARDS,
                            help="The number of shards to use. Default is 1")
        parser.add_argument("--num_tmcs", type=int, default=self.NUM_TMCS,
                            help="The number of TMCs to use. Default is 1")
        parser.add_argument("--repl_factor", type=int, default=self.REPL_FACTOR,
                            help="The replication factor to use. Default is 1")
        parser.add_argument("--kill_pids", action='store_true', default=self.KILL_PIDS,
                            help="Kill the processes strictly in the order they were created.")
        # pylint: enable=C0301
        args = parser.parse_args() # given we have the dest set

        self.IP = args.ip
        self.PORT = args.port
        self.LOG_LEVEL = args.log_level
        self.RUNNER_TYPE = args.runner_type
        self.NUM_AGENTS = args.num_agents
        self.NUM_SHARDS = args.num_shards
        self.NUM_TMCS = args.num_tmcs
        self.REPL_FACTOR = args.repl_factor
        self.KILL_PIDS = args.kill_pids

    def type_checks(self):
        """
        With subprocess, it is important for security not to run args unless we validate them
        """
        compliant = True

        if self.IP == "localhost":
            self.IP = "127.0.0.1"
        else:
            ip = self.IP.split('.')
            if not (len(ip) == 4 and all(0 <= int(x) <= 255 for x in ip)):
                print("Invalid IP format")
                compliant = False
        if not isinstance(self.PORT, int):
            print("Invalid type for PORT")
            compliant = False
        if self.LOG_LEVEL not in ["DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"]:
            print("Invalid log level")
            compliant = False
        if self.RUNNER_TYPE not in ["lua", "evm", "pyrunner"]:
            print("Invalid runner type")
            compliant = False
        if not isinstance(self.NUM_AGENTS, int):
            print("Invalid type for NUM_AGENTS")
            compliant = False
        if not isinstance(self.NUM_SHARDS, int):
            print("Invalid type for NUM_SHARDS")
            compliant = False
        if not isinstance(self.NUM_TMCS, int):
            print("Invalid type for NUM_TMCS")
            compliant = False
        if not isinstance(self.REPL_FACTOR, int):
            print("Invalid type for REPL_FACTOR")
            compliant = False
        if not isinstance(self.KILL_PIDS, bool):
            print("Invalid type for KILL_PIDS")
            compliant = False
        # exit if any violation occurs
        if not compliant:
            sys.exit(1)

    def bounds_check(self):
        """
        make sure user arguments are within computer limits
        """
        if any([self.NUM_AGENTS, self.NUM_SHARDS, self.NUM_TMCS, self.REPL_FACTOR]) < 1:
            print("Number of agents, shards, ticket machines, and replication factor must be at least 1")
            sys.exit(1)
        # FIXME:
        if self.REPL_FACTOR * max([self.NUM_SHARDS, self.NUM_TMCS]) > 5000:
            max_repl = 5000 / max([self.NUM_SHARDS, self.NUM_TMCS])
            print(f"Replication factor must be at most {max_repl} for the number of shards and ticket machines")
            sys.exit(1)
        elif any([self.NUM_AGENTS, self.NUM_SHARDS, self.NUM_TMCS]) > 5000:
            print("Number of agents, shards, and ticket machines must be at most 5000")
            sys.exit(1)
        if not (1024 <= self.PORT <= 65535):
            print("Port number out of bounds")
            sys.exit(1)

    def __repr__(self):
        return \
        f"""
        {self.IP = }
        {self.PORT = }
        {self.LOG_LEVEL = }
        {self.RUNNER_TYPE = }
        {self.NUM_AGENTS = }
        {self.NUM_SHARDS = }
        {self.NUM_TMCS = }
        {self.REPL_FACTOR = }
        {self.KILL_PIDS = }
        """

    def print_args(self):
        print(self.__repr__())


class ProcessIDs:
    """
    queue to store the pids of the processes
    faster than other data structures for this purpose of FIFO
    """
    def __init__(self, log_dir: str = os.path.join(ROOT, "logs_parsec")):
        # FIXME: multiprocessing Queue may be better
        self.shardd_pids =   queue.Queue()
        self.tmcd_pids =     queue.Queue()
        self.agentd_pids =   queue.Queue()
        self.csv_pids =      queue.Queue()
        self.csv_pids_file = os.path.join(log_dir, "parsec-pids.csv")

    def add_pid(self, pid: int, machine_name: str):
        if machine_name == 'shardd':
            self.shardd_pids.put(pid)
        elif machine_name == 'ticket_machined':
            self.tmcd_pids.put(pid)
        elif machine_name == 'agentd':
            self.agentd_pids.put(pid)

    def remove_pid(self, machine_name: str):
        if machine_name == 'shardd':
            self.shardd_pids.get()
        elif machine_name == 'ticket_machined':
            self.tmcd_pids.get()
        elif machine_name == 'agentd':
            self.agentd_pids.get()

    def show_pids(self):
        # Access all elements without removing them
        print("\nShardd pids: ")
        with self.shardd_pids.mutex:  # Protect against concurrent access
            for item in self.shardd_pids.queue:
                print(item)
        print("\nTicket Machined pids: ")
        with self.tmcd_pids.mutex:
            for item in self.tmcd_pids.queue:
                print(item)
        print("\nAgentd pids: ")
        with self.agentd_pids.mutex:
            for item in self.agentd_pids.queue:
                print(item)
        print()

    def kill_pids_from_ps(self):
        cmd_kill = [
            "ps aux | grep -v 'grep' | grep -E 'parsec' | awk '{print $2}' | xargs kill -9"
        ]
        subprocess.run(cmd_kill, check=True, shell=True)


class LogsParsec:
    def __init__(self, log_level: str = "INFO"):
        self.log_dir = os.path.join(ROOT, "logs_parsec")
        self.log_dir_archive = os.path.join(ROOT, "logs_parsec_archived")
        self.log_level = Args.LOG_LEVEL

        self.log_dir_setup()

        self.logger_main = self.setup_log_prefs("main")
        self.logger_shardd = self.setup_log_prefs("shardd")
        self.logger_tmcd = self.setup_log_prefs("ticket_machined")
        self.logger_agentd = self.setup_log_prefs("agentd")

    def log_dir_setup(self):
        # create the log directory if it doesn't exist
        if not os.path.isdir(self.log_dir):
            os.makedirs(self.log_dir)
            return

        # move old logs to archive dir
        curr_time = datetime.now()
        fmt_curr_time = curr_time.strftime("%Y-%m-%d_%H-%M-%S")

        log_dir_archive = os.path.join(ROOT, f"logs_parsec_archived_{fmt_curr_time}")
        os.makedirs(log_dir_archive, exist_ok=True)
        cmd_mv = [ "mv", f"{self.log_dir}", f"{log_dir_archive}" ]
        subprocess.run(cmd_mv, check=True, text=True)
        print(f'Old parsec logs moved to: {log_dir_archive}\n')

        os.makedirs(self.log_dir, exist_ok=True)
        return

    def setup_log_prefs(self, log_name: str) -> logging.Logger:
        """
        Create and return a logger each time requested
        """
        logger = logging.getLogger(log_name)
        logger.setLevel(getattr(logging, self.log_level))

        handler = logging.FileHandler(os.path.join(self.log_dir, f"{log_name}.log"))
        handler.setLevel(getattr(logging, self.log_level))

        formatter = logging.Formatter("%(name)s - %(levelname)s - %(message)s")
        # formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
        handler.setFormatter(formatter)
        logger.addHandler(handler)

        return logger

    def print_log_filename(self, logger: logging.Logger) -> None:
        """
        Print the log filename from the logger's handlers.
        """
        if logger.handlers:
            print(f'log {logger.name}\n\t{logger.handlers[0].baseFilename}\n')
        else:
            print("No log handlers found.")

    def print_log_filenames(self):
        """
        Print the log filenames from the loggers
        """
        self.print_log_filename(self.logger_main)
        self.print_log_filename(self.logger_shardd)
        self.print_log_filename(self.logger_tmcd)
        self.print_log_filename(self.logger_agentd)


def timeit(func):
    @wraps(func)
    def measure_time(*args, **kwargs):
        start_time = time.time()
        result = func(*args, **kwargs)
        end_time = time.time()
        print("@timefn: {} took {} seconds.\n".format(func.__name__, round(end_time - start_time, 2)))
        return result
    return measure_time


def run_cmd(cmd, logger):
    """
    Run a command and redirect output to the logger's log file.
    """
    with open(logger.handlers[0].baseFilename, 'a') as log_file:
        process = subprocess.Popen(cmd.split(), stdout=log_file, stderr=log_file)
    return process


def wait_for_port(host, port, timeout=60):
    """
    Wait timeout amount of time til the host:port is available
    """
    time_init = time.time()
    while time.time() - time_init < timeout:

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(1)
            result = sock.connect_ex((host, port))
            # port is not occupied (goal)
            if result == 0: return True
            time.sleep(1)

    return False


# FIXME: add full scale parsec once confirmed functionality of basic parsec
@timeit
def launch_basic_parsec():
    """
    designed for 1 shard, 1 shard cluster, 1 agent
    """
    # FIXME: flexible port assignment next outside of main function
    # allocate properly so no overlap with parallel launches
    shard_ep_port_id = 5556
    shard_raft_ep_port_id = 5557
    agent_port_id = 6666
    tmc_port_id = 7777
    agent_ep_port_id = 8888

    req_lgc_shards = Args.NUM_SHARDS
    req_phys_shards = Args.NUM_SHARDS * Args.REPL_FACTOR
    req_num_tmcs = Args.NUM_TMCS * Args.REPL_FACTOR

    IP = Args.IP
    LOG_LEVEL = Args.LOG_LEVEL
    RUNNER_TYPE = Args.RUNNER_TYPE

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

    # ./scripts/wait-for-it.sh:
    # [-s|--strict] Only execute subcommand if the test succeeds
    # [-t num_secs|--timeout=TIMEOUT]
    # [-- COMMAND ARGS] execute subcmd (args following) if test succeeds
    cmd_tmcd = [
        # "./scripts/wait-for-it.sh",
        # "-s", f"{IP}:{shard_ep_port_id}", "-t", "60", "--",
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
        # "./scripts/wait-for-it.sh",
        # "-s", f"{IP}:{tmc_port_id}", "-t", "60", "--",
        # "./scripts/wait-for-it.sh",
        # "-s", f"{IP}:{agent_port_id}", "-t", "60", "--",
        "./build/src/parsec/agent/agentd",
        "--shard_count=1",
        "--shard0_count=1",
        f"--shard00_endpoint={IP}:{shard_ep_port_id}",
        "--node_id=0",
        "--component_id=0",
        "--agent_count=1",
        f"--agent0_endpoint={IP}:{agent_ep_port_id}",
        "--ticket_machine_count=1",
        f"--ticket_machine0_endpoint={IP}:{tmc_port_id}",
        f"--log_level={LOG_LEVEL}",
        f"--runner_type={RUNNER_TYPE}"
    ]

    shardd_cmd = " ".join(cmd_shardd)
    ticket_machined_cmd = " ".join(cmd_tmcd)
    agentd_cmd = " ".join(cmd_agentd)
    Logs.logger_main.info("\nAttempting to launch machine batch #1")

    print("starting shardd...\n")
    proc_shardd = run_cmd(shardd_cmd, Logs.logger_shardd)
    time.sleep(1)

    # launch ticket machine when shard_ep_port_id is available
    if wait_for_port(IP, shard_ep_port_id):
        print(f"Port {shard_ep_port_id = } is available,\nstarting ticket_machined...\n")
        proc_tmcd = run_cmd(ticket_machined_cmd, Logs.logger_tmcd)
        time.sleep(1)  # Delay to ensure ticket_machined starts properly
    else:
        print(f"Timeout waiting for port {shard_ep_port_id = } to start ticket_machined.")
        proc_shardd.terminate()
        exit(1)

    # launch agent when tmc_port_id and shard_ep_port_id are available
    if wait_for_port(IP, tmc_port_id) and wait_for_port(IP, shard_ep_port_id):
        print(f"Ports {tmc_port_id = } and {shard_ep_port_id = } are available\nstarting agentd...\n")
        proc_agentd = run_cmd(agentd_cmd, Logs.logger_agentd)
    else:
        print(f"Timeout waiting for ports {tmc_port_id = } and {shard_ep_port_id = } to start agentd.")
        proc_shardd.terminate()
        proc_tmcd.terminate()
        exit(1)

    # add the pids to the queue (if we want selective kill)
    Pids.add_pid(proc_shardd.pid, "shardd")
    Pids.add_pid(proc_tmcd.pid, "ticket_machined")
    Pids.add_pid(proc_agentd.pid, "agentd")

    print("Parsec machines batch launched successfully.")

    # FIXME - think about timeout for the processes? / wait-for-it.sh
    # sys.exit(0)


# class objects for global use
Args = ProgramArgs()
Pids = ProcessIDs()
Logs = LogsParsec(Args.LOG_LEVEL)


if __name__ == "__main__":

    try:
        launch_basic_parsec()
        # main_sequential()
        # asyncio.run(main_parallel())

    except KeyboardInterrupt:
        print("Interruption detected")

    finally:
        Logs.print_log_filenames() # on DEBUG
        # Pids.write_to_csv()
        print("The processes are running in the background and can be killed by running:")
        print("\t'scripts/parsec-local-multi.py --kill_pids'\n")
        print('Exiting...')
        # cleanup
        # rm -rf logs_parsec* ticket_machine_raft_* runtime_locking_shard0_raft_*



# FIXME: common vars class object to store ROOT, start port for each machine, etc
# max usage is 5000 ports of the possible 10k ports per machine type
# built in buffer in case some ports are unavailable
# SHARD_EP_PORT_ID =      5556
# SHARD_RAFT_EP_PORT_ID = 5557
# TMC_PORT_ID =           7777
# AGENT_PORT_ID =         6666
# SHARD_EP_PORT_ID =      20000
# SHARD_RAFT_EP_PORT_ID = 30000
# TMC_PORT_ID =           40000
# AGENT_PORT_ID =         50000
# pylint: enable=C0103
