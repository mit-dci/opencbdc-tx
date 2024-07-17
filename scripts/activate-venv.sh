#!/usr/bin/env bash

if ! (return 0 2>/dev/null); then
    echo -e "\nThis script must be sourced, see below\n"
    echo -e "Usage: 'source scripts/activate-venv.sh'\n"
    exit 1
fi

ENV_NAME=".py_venv"
PWD=$(pwd)

check_venv_location() {
    # see if directory for venv exists
    if [[ $# -lt 2 ]]; then
        echo -e "\nUsage: check_venv_location ENV_NAME ENV_LOCATION\n"
        return 1
    fi
    ENV_NAME="$1"; ENV_LOCATION="$2"
    # if venv does not exist, send error message and return
    if [[ ! -d "$ENV_LOCATION" ]]; then
        echo -e "\nVirtual environment '${ENV_NAME}' not found at location: '${ENV_LOCATION}'"
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            echo -e "run 'sudo ./scripts/install-build-tools.sh' to create it.\n"
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            echo -e "run './scripts/install-build-tools.sh' to create it.\n"
        fi
        return 1
    fi
    return 0
}

activate_venv() {
    # activate the virtual environment (we know venv exists)
    if [[ $# -lt 2 ]]; then
        echo -e "\nUsage: activate_venv ENV_NAME ENV_LOCATION\n"
        return 1
    fi
    ENV_NAME="$1"; ENV_LOCATION="$2"
    if source "${ENV_LOCATION}/bin/activate"; then
        echo -e "\nVirtual environment '${ENV_NAME}' activated."
        echo -e "Run 'deactivate' to exit the virtual environment.\n"
        return 0
    else
        echo -e "Failed to activate virtual environment '${ENV_NAME}'\n"
        return 1
    fi
}

# see if we are in a git repository
if git rev-parse --show-toplevel >/dev/null 2>&1; then
    ROOT="$(git rev-parse --show-toplevel)"
    ENV_LOCATION="${ROOT}/${ENV_NAME}"
    [[ "$ROOT" == "/" ]] && ENV_LOCATION="/${ENV_NAME}"
    if check_venv_location "$ENV_NAME" "$ENV_LOCATION"; then
        activate_venv "$ENV_NAME" "$ENV_LOCATION" && return 0
    fi
# try to find venv in current or parent directories
else
    # figure out env location; don't cd up beyond root
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    while [[ $ROOT != "/" ]]; do
        ENV_LOCATION="${ROOT}/${ENV_NAME}"
        if check_venv_location "$ENV_NAME" "$ENV_LOCATION"; then
            activate_venv "$ENV_NAME" "$ENV_LOCATION" && return 0
        fi
        ROOT="$(cd "${ROOT}"/.. && pwd)"
    done
    ENV_LOCATION="/${ENV_NAME}"
    if check_venv_location "$ENV_NAME" "$ENV_LOCATION"; then
        activate_venv "$ENV_NAME" "$ENV_LOCATION" && return 0
    fi
fi

echo -e "Virtual environment '${ENV_NAME}' not found and/or failed to activate."
return 1
