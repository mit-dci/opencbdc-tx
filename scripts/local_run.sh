#!/bin/bash

function scriptDir
{
        SOURCE="${BASH_SOURCE[0]}"

        while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
                DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
                SOURCE="$(readlink "$SOURCE")"
                [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
        done
        DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
        echo $DIR
}

sdir="$( scriptDir )"

mkdir -p $sdir/local_log || exit 1
cd $sdir/local_log || exit 2

#TODO:
#sleep for fixed time isn't ideal but works for now
#it looks like services needs some interval maybe some reconnect logic should
#be added upon start to resolve this and let starting this daemons in parallel

echo "Starting locking shard"
$sdir/../build/src/uhs/twophase/locking_shard/locking-shardd $sdir/../2pc-local.cfg 0 0 &> locking-shardd.log&
sleep 5
echo "Starting coordinatord"
$sdir/../build/src/uhs/twophase/coordinator/coordinatord $sdir/../2pc-local.cfg 0 0 &> coordinatord.log&
sleep 5
echo "Starting sentineld"
$sdir/../build/src/uhs/twophase/sentinel_2pc/sentineld-2pc $sdir/../2pc-local.cfg 0 &> sentineld.log&

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

wait $(jobs -p)
