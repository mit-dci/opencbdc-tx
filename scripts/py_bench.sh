#!/bin/bash

# A sample script for running the Python Bench test

IP="127.0.0.1"
PORT="8889"

#cp ./scripts/pythonContractConverter.py ./build/tools/bench/parsec/py/

rr record ./build/tools/bench/parsec/py/py_bench --component_id=0 --ticket_machine0_endpoint=$IP:7777 --ticket_machine_count=1 --shard_count=1 --shard0_count=1 --shard00_endpoint=$IP:5556 --agent_count=1 --agent0_endpoint=$IP:$PORT
echo done
