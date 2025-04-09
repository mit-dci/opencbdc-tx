#!/usr/bin/env bash
set -e

# calling scripts can use this script to capture the project ROOT dir
# SCRIPT_DIR="$( cd -- "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# ROOT="$( "$SCRIPT_DIR"/get-root.sh )"

# returns the fullpath to the ROOT dir without a slash at the end
ROOT=
# if we are in a git repository
if git rev-parse --show-toplevel >/dev/null 2>&1; then
    ROOT="$( git rev-parse --show-toplevel )"
else
    # $BASH_SOURCE[0] is the full path of the script being executed
    SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd )"
    # scripts dir will always be one level below ROOT level: "$ROOT"/scripts
    ROOT="$( cd -- "$SCRIPT_DIR"/.. && pwd )"
fi

# check for directory existence
if [[ ! -d "$ROOT" ]]; then
    echo "Error: ROOT '${ROOT}' not found."
    exit 1
fi

# use other scripts in the script dir to capture the ROOT fullpath
printf "%s\n" "$ROOT"

