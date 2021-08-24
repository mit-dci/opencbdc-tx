#!/bin/bash
NUM_LOADGENS=$(expr $1 - 1)
LOADGEN_DELAY=$(expr $2 + 0)

printf "Using $1 loadgens and waiting $LOADGEN_DELAY seconds between starting and stopping them\n"

set -e
OUTPUT_BASE_FOLDER="test-output"
TESTRUN_ID="$(date +%s)-$(git describe --always)"
OUTPUT_FOLDER="$OUTPUT_BASE_FOLDER/$TESTRUN_ID"
mkdir -p $OUTPUT_FOLDER

printf "$(date +%s%N) 0\n" >> "$OUTPUT_FOLDER/client-count.log"

export TESTRUN_ID=$TESTRUN_ID

docker-compose -f docker-compose-3pc-test.yml down
echo "Building"
docker-compose -f docker-compose-3pc-test.yml build
echo "Starting system"
docker-compose -f docker-compose-3pc-test.yml up -d agent0 shard0 ticket0
echo "Waiting for agent to be available"
scripts/wait-for-it.sh -s -t 60 172.16.1.30:8080 -- echo "Agent up"

for (( i=0; i<=$NUM_LOADGENS; i++ ))
do
    echo "Starting load gen $i"
    docker-compose -f docker-compose-3pc-test.yml up -d "loadgen$i"
    printf "$(date +%s%N) $((i+1))\n" >> "$OUTPUT_FOLDER/client-count.log"
    echo "Load gen $i running, waiting for $LOADGEN_DELAY seconds"
    sleep $LOADGEN_DELAY
done

echo "All load gens running, waiting for another $(expr $LOADGEN_DELAY \* 2) seconds"
sleep "$(expr $LOADGEN_DELAY \* 2)"

set +e #Ignore errors from here

#for (( i=$NUM_LOADGENS; i>=0; i-- ))
for (( i=0; i<=$NUM_LOADGENS; i++ ))
do
    echo "Stopping load gen $i"
    docker-compose -f docker-compose-3pc-test.yml stop "loadgen$i"
    printf "$(date +%s%N) $(expr $NUM_LOADGENS - $i)\n" >> "$OUTPUT_FOLDER/client-count.log"
    echo "Copying latency samples for load gen $i"
    docker cp "opencbdc-loadgen$i:/opt/tx-processor/tx_samples_$i.txt" "$OUTPUT_FOLDER/"
    docker cp "opencbdc-loadgen$i:/opt/tx-processor/telemetry.bin" "$OUTPUT_FOLDER/telemetry_loadgen$i.bin"
    echo "Archiving logs for load gen $i"
    docker logs "opencbdc-loadgen$i" 2>&1 1>"$OUTPUT_FOLDER/loadgen$i.log"
done

echo "Stopping agent"
docker-compose -f docker-compose-3pc-test.yml stop agent0
echo "Copying agent logs"
docker logs opencbdc-agent0 2>&1 1>"$OUTPUT_FOLDER/agent0.log"
docker cp "opencbdc-agent0:/opt/tx-processor/telemetry.bin" "$OUTPUT_FOLDER/telemetry_agent0.bin"

echo "Stopping shard"
docker-compose -f docker-compose-3pc-test.yml stop shard0
echo "Copying shard logs"
docker logs opencbdc-shard0 2>&1 1>"$OUTPUT_FOLDER/shard0.log"
docker cp "opencbdc-shard0:/opt/tx-processor/telemetry.bin" "$OUTPUT_FOLDER/telemetry_shard0.bin"
#docker cp opencbdc-atomizer0:/opt/tx-processor/event_sampler_atomizer.bin "$OUTPUT_FOLDER/"
#docker cp opencbdc-atomizer0:/opt/tx-processor/event_sampler_atomizer_controller.bin "$OUTPUT_FOLDER/"
#docker cp opencbdc-atomizer0:/opt/tx-processor/event_sampler_send_complete_txs.bin "$OUTPUT_FOLDER/"
#docker cp opencbdc-atomizer0:/opt/tx-processor/event_sampler_state_machine.bin "$OUTPUT_FOLDER/"
#docker cp opencbdc-atomizer0:/opt/tx-processor/event_sampler_tx_notify.bin "$OUTPUT_FOLDER/"

echo "Stopping ticketer"
docker-compose -f docker-compose-3pc-test.yml stop ticket0
echo "Copying ticketer logs"
docker logs opencbdc-ticket0 2>&1 1>"$OUTPUT_FOLDER/ticket0.log"
docker cp "opencbdc-ticket0:/opt/tx-processor/telemetry.bin" "$OUTPUT_FOLDER/telemetry_ticket0.bin"

echo "Calculating result"
cd "$OUTPUT_BASE_FOLDER"
python3 ../scripts/local-benchmark-result.py
