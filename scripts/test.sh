#!/bin/bash
# Exit script on failure.
set -e

function usage() { echo "
USAGE:
    ./test.sh

DESCRIPTION:
    This script runs unit and integration tests and measures test coverage. 

FLAGS: 
    -d, --build-dir <dir. name>     The directory containing the built code.
                                    Default:  opencbdc-tx/build/
    -nu, --no-unit-tests            Do not run unit tests.
                                    Default:  false
    -ni, --no-integration-tests     Do not run integration tests.
                                    Default:  false
    -nc, --no-coverage              Do not measure test coverage.
                                    Default:  false
    -h, --help                      Show usage.
	"
}

function check_if_folder_exists(){
    if [ ! -d "$1" ]
    then
        echo "ERROR:  The folder '${BUILD_DIR}' was not found."
        usage
        exit 1
    fi
}

# Parse command-line arguments.
RUN_UNIT_TESTS="true"
RUN_INTEGRATION_TESTS="true"
MEASURE_COVERAGE="true"
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
            if [[ $ARG == "" || ${ARG:0:1} == "-" ]]; then      
                echo -n "ERROR:  The -d flag was used, "
                echo "but a valid build folder was not given."
                echo 
                usage
                exit 1
            fi 
            BUILD_DIR=$ARG
            check_if_folder_exists "$BUILD_DIR"
            # If a relative path is given, convert it to an absolute path
            # to avoid potential relative path errors and to improve readability
            # if the path is written to stdout.
            BUILD_DIR=$(cd "$BUILD_DIR"; pwd)
            shift
            ;;
        -ni|--no-integration-tests)
            RUN_INTEGRATION_TESTS="false"
            shift 
            ;;
        -nu|--no-unit-tests)
            RUN_UNIT_TESTS="false"
            shift 
            ;;
        -nc|--no-coverage)
            MEASURE_COVERAGE="false"
            shift
            ;; 
        -*)
            echo "ERROR:  unknown command-line option '${1}'."
            usage
            exit 1
            ;;
        *)
            echo "ERROR:  unknown command-line option '${1}'."
            usage
            exit 1
            ;;
    esac
done

# If the build folder is not provided as a command-line argument, assume it's 
# named 'build' and is located in the top-level directory of the repo.  
# By defining the top-level directory relative to the location of this script, 
# the user can run this script from any folder.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REPO_TOP_DIR="${SCRIPT_DIR}/.."
# Convert the relative path to an absolute path to avoid potential relative path 
# errors and to improve readability if the path is written to stdout.
REPO_TOP_DIR=$(cd "${REPO_TOP_DIR}"; pwd)

if [ -z ${BUILD_DIR+x} ]
then
    BUILD_DIR="${REPO_TOP_DIR}/build"
fi
check_if_folder_exists "${BUILD_DIR}"
export BUILD_DIR
echo "Build folder: '${BUILD_DIR}'"
echo

run_test_suite () {
    cd "$BUILD_DIR"
    find . -name '*.gcda' -exec rm {} \;
    "$PWD"/"$1"

    if [[ "$MEASURE_COVERAGE" == "true" ]]
    then
        echo "Checking test coverage."
        LOCATION=$2
        rm -rf "$LOCATION"
        mkdir -p "$LOCATION"
        find . \( -name '*.gcno' -or -name '*.gcda' \) \
            -and -not -path '*coverage*' -exec cp --parents \{\} "$LOCATION" \;
        cd "$LOCATION"

        lcov -c -i -d . -o base.info --rc lcov_branch_coverage=1
        lcov -c -d . -o test.info --rc lcov_branch_coverage=1
        lcov -a base.info -a test.info -o total.info --rc lcov_branch_coverage=1
        lcov --remove total.info '*/3rdparty/*' '*/tests/*' '*/tools/*' \
            -o trimmed.info --rc lcov_branch_coverage=1
        lcov --extract trimmed.info '*/src/*' -o cov.info \
            --rc lcov_branch_coverage=1
        genhtml cov.info -o output --rc genhtml_branch_coverage=1
    else
        echo "Skipping coverage measurement."
    fi
}

if [[ $RUN_UNIT_TESTS == "true" ]]
then 
    echo "Running unit tests..."
    run_test_suite "tests/unit/run_unit_tests" "unit_tests_coverage"
else
    echo "Skipping unit tests."
fi

echo
if [[ $RUN_INTEGRATION_TESTS == "true" ]]
then
    echo "Running integration tests..."
    cp "${REPO_TOP_DIR}"/tests/integration/*.cfg "$BUILD_DIR"
    run_test_suite "tests/integration/run_integration_tests" \
        "integration_tests_coverage"
else
    echo "Skipping integration tests."
fi
