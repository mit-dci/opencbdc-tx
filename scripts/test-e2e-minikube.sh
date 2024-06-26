#!/usr/bin/env bash
set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
MINIKUBE_PROFILE=${MINIKUBE_PROFILE:-opencbdc}
BUILD_DOCKER=${TESTRUN_BUILD_DOCKER:-1}

# Make sure we have the necessary tools installed
required_executables=(minikube docker go helm kubectl)
for e in ${required_executables[@]}; do
    if ! command -v $e &> /dev/null; then
        echo "'$e' command not be found! This is required to run. Please install it."
        exit 1
    fi
done

# Start minikube cluster with opencbdc profile
minikube_status=$(minikube -p $MINIKUBE_PROFILE status | grep apiserver | awk '{ print $2 }')
if [ "$minikube_status" != "Running" ]; then
    echo "🔄 Starting minkube cluster with profile '$MINIKUBE_PROFILE'..."
    minikube -p $MINIKUBE_PROFILE start
else
    echo "✅ minikube cluster with profile '$MINIKUBE_PROFILE' is currently running."
fi

# Update Kubernetes context to default to this minikube cluster profile
minikube -p $MINIKUBE_PROFILE update-context

# Connect host docker cli to docker daemon in minikube vm
eval $(minikube -p $MINIKUBE_PROFILE docker-env)

# Build docker image
if [[ $BUILD_DOCKER -eq 1 ]]; then
    echo "🔄 Building docker image for 'opencbdc-tx'"
    $SCRIPT_DIR/build-docker.sh
fi

# Change to the test directory and fetch dependencies
cd $SCRIPT_DIR/../charts/tests
echo "🔄 Downloading Go dependencies for testing..."
go mod download -x

# Set testrun_id and path to store logs from testrun and containers
TESTRUN_ID=${TESTRUN_ID:-$(uuidgen)}
TESTRUN_PATH=$SCRIPT_DIR/../testruns/$TESTRUN_ID
TESTRUN_LOG_PATH="$TESTRUN_PATH/testrun.log"
mkdir -p $TESTRUN_PATH

# Run test and output test logs to console as well as a file for reference later
echo "🔄 Running tests..."
TESTRUN_ID=$TESTRUN_ID go test 2>&1 | tee -a $TESTRUN_LOG_PATH

# Generate a markdown report of the testrun with logs
$SCRIPT_DIR/create-e2e-report.sh $TESTRUN_PATH >> $TESTRUN_PATH/report.md
echo "View test results at $(realpath $TESTRUN_PATH)"
