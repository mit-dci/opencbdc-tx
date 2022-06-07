#!/bin/bash
# Exit script on failure.
set -e

function usage() { echo "
USAGE:
    ./test.sh

DESCRIPTION:
    This script runs unit and integration tests and measures test coverage. 

FLAGS: 
    -nu, --no-unit-tests            Do not run unit tests.
                                    Default:  false
    -ni, --no-integration-tests     Do not run integration tests.
                                    Default:  false
    -nc, --no-coverage              Do not measure test coverage.
                                    Default:  false
    -h, --help                      Show usage.
	"
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

if [ -z ${BUILD_DIR+x} ]; then
    export BUILD_DIR=build
fi

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

        cd ..
    else
        echo "Skipping coverage measurement."
    fi

    cd ..
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
    cp tests/integration/*.cfg "$BUILD_DIR"
    run_test_suite "tests/integration/run_integration_tests" \
        "integration_tests_coverage"
else
    echo "Skipping integration tests."
fi
