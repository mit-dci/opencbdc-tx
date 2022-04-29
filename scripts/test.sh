#!/bin/bash
# exit script on failure
set -e

if [ -z ${BUILD_DIR+x} ]; then
    export BUILD_DIR=build
fi

run_test_suite () {
    cd $BUILD_DIR
    find . -name '*.gcda' -exec rm {} \;
    $PWD/$1

    LOCATION=$2
    rm -rf $LOCATION
    mkdir -p $LOCATION
    find . \( -name '*.gcno' -or -name '*.gcda' \) -and -not -path '*coverage*' -exec cp --parents \{\} "$LOCATION" \;
    cd $LOCATION

    lcov -c -i -d . -o base.info --rc lcov_branch_coverage=1
    lcov -c -d . -o test.info --rc lcov_branch_coverage=1
    lcov -a base.info -a test.info -o total.info --rc lcov_branch_coverage=1
    lcov --remove total.info '*/3rdparty/*' '*/tests/*' '*/tools/*' -o trimmed.info --rc lcov_branch_coverage=1
    lcov --extract trimmed.info '*/src/*' -o cov.info --rc lcov_branch_coverage=1
    genhtml cov.info -o output --rc genhtml_branch_coverage=1

    cd ../..
}

echo "Running unit tests..."
cp tests/unit/*.cfg $BUILD_DIR
run_test_suite "tests/unit/run_unit_tests" "unit_tests_coverage"

echo "Running integration tests..."
cp tests/integration/*.cfg $BUILD_DIR
run_test_suite "tests/integration/run_integration_tests" "integration_tests_coverage"
