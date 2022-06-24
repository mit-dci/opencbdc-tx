#!/bin/bash
# Exit script on failure.
set -e

function echo_help_msg(){
    echo "Run test.sh -h for help."
}

trap '[ "$?" -eq 0 ] || echo_help_msg' EXIT

function usage() { echo "
USAGE:
    ./test.sh

DESCRIPTION:
    This script runs unit and integration tests and measures test coverage.

FLAGS:
    -d, --build-dir <dir. name>     Set the directory containing the built code.
                                    Default:  opencbdc-tx/build/
    -nu, --no-unit-tests            Do not run unit tests.
    -ni, --no-integration-tests     Do not run integration tests.
    -nc, --no-coverage              Do not measure test coverage.
    -h, --help                      Show usage.

    All Google Test flags are also accepted.  A subset is listed below.  For a
    complete list of Google Test flags and their usage, run:
    $ ./test.sh -ni -nc --gtest_help

    --gtest_filter=<filter>         Define filter that specifies which tests
                                    to run.
                                    Default:  no filter (i.e. run all tests)
    --gtest_repeat=<integer>        Repeat tests.
                                    Default:  1 (i.e. do not repeat tests)
    --gtest_break_on_failure        Stop running tests if a test fails.

EXAMPLES:
    - Run unit tests and integration tests and measure test coverage.
      The build directory is set by an environment variable called 'BUILD_DIR'
      or will be set to 'opencbdc-tx/build' by default.
    $ ./test.sh

    - Run integration tests but do not run unit tests.  Do not measure
      test coverage. The build directory is set by an environment variable
      called 'BUILD_DIR' or will be set to 'opencbdc-tx/build' by default.
    $ ./test.sh -nu -nc

    - Run unit tests and integration tests and measure test coverage.  Set the
      build directory to 'mybuild'.
    $ ./test.sh -d mybuild

    - Run only the ArchiverTest tests and measure test coverage.
    $ ./test.sh --gtest_filter=ArchiverTest*

    - Run only the ArchiverTest.client test.  Repeat it 4 times.
      Don't measure test coverage.
    $ ./test.sh --gtest_filter=ArchiverTest.client --gtest_repeat=4 -nc

    - Run only unit tests and don't measure test coverage.
      Stop the tests if any test fails.
    $ ./test.sh -ni -nc --gtest_break_on_failure
	"
}

# Parse command-line arguments.
RUN_UNIT_TESTS="true"
RUN_INTEGRATION_TESTS="true"
MEASURE_COVERAGE="true"
GTEST_FLAGS=()
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
        --gtest_*)
            GTEST_FLAGS+=("$1")
            shift
            ;;
        -*)
            echo "ERROR:  unknown command-line option '${1}'."
            exit 1
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
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REPO_TOP_DIR="${SCRIPT_DIR}/.."
if [ -z ${BUILD_DIR+x} ]
then
    BUILD_DIR="${REPO_TOP_DIR}/build"
fi

if [ ! -d "$BUILD_DIR" ]
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

    if [[ "$MEASURE_COVERAGE" == "true" ]]
    then
        echo "Checking test coverage."
        LOCATION="$2"
        rm -rf "$LOCATION"
        mkdir -p "$LOCATION"
        find . \( -name '*.gcno' -or -name '*.gcda' \) \
            -and -not -path '*coverage*' -exec rsync -R \{\} "$LOCATION" \;
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
