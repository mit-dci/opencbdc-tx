#!/usr/bin/env bash

ROOT="$(cd "$(dirname "$0")"/.. && pwd)"
MIN_CODE_QUALITY=8.0

get_code_score() {
    if [ -n "$1" ]; then
    # set minimum quality to user input (int/float) if provided and (5.0 <= input <= 10.0)
        if [[ $1 =~ ^([0-9]+)*([\.][0-9])?$ ]]; then
            if (( $(echo "$1 >= 5.0" | bc -l) )) && (( $(echo "$1 <= 10.0" | bc -l) )); then
                MIN_CODE_QUALITY=$1
            else
                # In the future, we want code quality to be at minimum 8.0/10.0
                echo "Code quality score must be between 5.0 and 10.0, inclusive."
                echo "Recommended code quality score is >= 8.0."
                exit 1
            fi
        else
            echo "Code quality score must be an integer or floating point number."
            exit 1
        fi
    fi
    echo "Linting Python code with minimum quality of $MIN_CODE_QUALITY/10.0..."
}

check_pylint() {
    if ! command -v pylint &>/dev/null; then
        echo "pylint is not installed."
        echo "Run 'sudo ./scripts/install-build-tools.sh' to install pylint."
        exit 1
    fi
}

get_code_score "$1"
if source "${ROOT}/scripts/activate-venv.sh"; then
    echo "Virtual environment activated."
else
    echo "Failed to activate virtual environment."
    exit 1
fi

check_pylint
# shellcheck disable=SC2046
if ! pylint scripts src tests tools --rcfile=.pylintrc \
    --fail-under="$MIN_CODE_QUALITY" $(git ls-files '*.py'); then
    echo "Linting failed, please fix the issues and rerun."
    exit 1
else
    echo "Linting passed."
fi
