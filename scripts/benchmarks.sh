#!/bin/bash
# Exit script on failure.
set -e

function echo_help_msg(){
    echo "Run benchmarks.sh -h for help."
}

trap '[ "$?" -eq 0 ] || echo_help_msg' EXIT

function usage() { echo "
USAGE:
    ./benchmarks.sh

DESCRIPTION:
    This script runs microbenchmarks for commonly used methods in this repository.

FLAGS:
    -d, --build-dir <dir. name>     Set the directory containing the built code.
    -h, --help                      Show usage.

EXAMPLES:
    - Run all benchmarks.
      The build directory is set by an environment variable called 'BUILD_DIR'
      or will be set to 'opencbdc-tx/build' by default.
    $ ./benchmarks.sh

    - Run benchmarks and set the
      build directory to 'mybuild'.
    $ ./benchmarks.sh -d mybuild
	"
}

# Parse command-line arguments.
while [[ $# -gt 0 ]]
do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -d|--build-dir)
            shift
            ARG="$1"
            if [[ $ARG == "" || ${ARG:0:1} == "-" ]]
            then
                echo -n "ERROR:  The -d flag was used, "
                echo "but a valid build folder was not given."
                exit 1
            fi
            BUILD_DIR=$ARG
            shift
            ;;
        *)
            echo "ERROR:  unknown command-line option '${1}'."
            exit 1
            ;;
    esac
done

# If the build folder is not provided as a command-line argument or as an
# environment variable, assume it's named 'build' and is located in the
# top-level directory of the repo.  By defining the top-level directory relative
# to the location of this script, the user can run this script from any folder.
SCRIPT_DIR="$( cd -- "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REPO_TOP_DIR="$( "$SCRIPT_DIR"/get-root.sh )"
if [[ -z "${BUILD_DIR+x}" ]]
then
    BUILD_DIR="${REPO_TOP_DIR}/build"
fi

if [[ ! -d "$BUILD_DIR" ]]
then
    echo "ERROR:  The folder '${BUILD_DIR}' was not found."
    exit 1
fi

# If the build folder is a relative path, convert it to an absolute path
# to avoid potential relative path errors and to improve readability
# if the path is written to stdout.
export BUILD_DIR=$(cd "$BUILD_DIR"; pwd)
echo "Build folder: '${BUILD_DIR}'"
echo

run_test_suite () {
    cd "$BUILD_DIR"
    find . -name '*.gcda' -exec rm {} \;
    "$PWD"/"$1" "${GTEST_FLAGS[@]}"
}

run_test_suite "benchmarks/run_benchmarks"
