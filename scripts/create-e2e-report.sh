#!/usr/bin/env bash
set -e

TESTRUN_PATH=$1

function readAndFormatLogs() {
    logdir="$1"
    message=""
    if [[ ! -d $logdir ]]; then
        echo "$logdir does not exist"
        return
    fi

    for logfile in "$logdir"/*; do
        logfile_path="$logdir/$logfile"
        logfile_content=$(<"$logfile_path")
        message+="\n<details>\n<summary>$logfile</summary>\n\n\`\`\`\n$logfile_content\n\`\`\`\n</details>\n"
    done
    echo "$message"
}

testrun_logs="\n<details>\n<summary>View Testrun</summary>\n\n\`\`\`\n$(cat "$TESTRUN_PATH"/testrun.log)\n\`\`\`\n</details>\n\n"
container_logs=$(readAndFormatLogs "$TESTRUN_PATH"/logs)

printf "# E2E Results\n# TestRun Logs\n%b\n\n# Container Logs\n%b\n" "$testrun_logs" "$container_logs"
