#!/bin/bash

echo "Setting up build environment..."

green="\033[0;32m"
cyan="\033[0;36m"
end="\033[0m"

# for debugging, 'set -x' can be added to trace the commands
set -e

SUDO=''
if (( $EUID != 0 )); then
    echo -e "non-root user, sudo required"
    SUDO='sudo'
fi

# Supporting these versions for buildflow
PYTHON_VERSIONS=("3.10" "3.11" "3.12")
echo "Python3 versions supported: ${PYTHON_VERSIONS[@]}"

# check if supported version of python3 is already installed, and save the version
PY_INSTALLED=''
for PY_VERS in "${PYTHON_VERSIONS[@]}"; do
    if which "python${PY_VERS}" &> /dev/null; then
        # save the installed version if found
        PY_INSTALLED=${PY_VERS}
        echo "Detected compatible python${PY_VERS} installed"
        break
    fi
done

SCRIPT_DIR="$( cd -- "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
ROOT="$( "$SCRIPT_DIR"/get-root.sh )"
ENV_NAME=".py_venv"

# make a virtual environement to install python packages
create_venv_install_python() {
    PY_LOC=$1
    if [[ -z "$PY_LOC" ]]; then
        echo "Python path not provided"
        exit 1
    fi
    PY_VERSION=$2
    if [[ -z "$PY_VERSION" ]]; then
        echo "python version not provided"
        exit 1
    fi
    # remove existing virtual environment if it exists - global var
    ENV_LOCATION="${ROOT}/${ENV_NAME}"
    if [[ -d "${ENV_LOCATION}" ]]; then
        if rm -rf -- "${ENV_LOCATION}"; then
            echo "Removed existing virtual environment"
        else
            echo "Failed to remove existing virtual environment"
            exit 1
        fi
    fi
    # install pip for linux
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if ! $SUDO apt install -y python3-pip; then
            echo "Failed to install python3-pip"
            wget https://bootstrap.pypa.io/get-pip.py
            $SUDO python${PY_VERSION} get-pip.py
            rm get-pip.py
        fi
        # add deadsnakes to download the python venv module
        $SUDO add-apt-repository -y ppa:deadsnakes/ppa
        # make sure deadsnakes is available
        DEADSNAKES_AVAIL=$(wget -q --spider http://ppa.launchpad.net/deadsnakes/ppa/ubuntu/dists/focal/Release; echo $?)
        if [[ $DEADSNAKES_AVAIL -ne 0 ]]; then
            echo "Failed to add deadsnakes which is needed to install python3"
            exit 1
        fi
        # install python3 venv module for linux
        if ! $SUDO apt install -y "python${PY_VERSION}-venv"; then
            echo "Failed to install python${PY_VERSION}-venv"
            exit 1
        else
            echo "Installed python${PY_VERSION}-venv"
        fi
    fi
    # create virtual environment with specific python version
    if "${PY_LOC}" -m venv "${ENV_NAME}"; then
        echo "Virtual environment '${ENV_NAME}' created"
    else
        echo "Virtual environment creation failed"
        exit 1
    fi
    # activate virtual environment
    if ! . "${ROOT}/scripts/activate-venv.sh"; then
        echo "Failed to activate virtual environment"
        exit 1
    fi
    # install python packages
    if pip install -r "${ROOT}/requirements.txt"; then
        echo "Success installing python packages"
    else
        echo "Failure installing python packages"
        deactivate
        exit 1
    fi
    deactivate
}

echo "OS Type: $OSTYPE"
# macOS install with homebrew
if [[ "$OSTYPE" == "darwin"* ]]; then

    # macOS does not support running shell scripts as root with homebrew
    if [[ $EUID -eq 0 ]]; then
        echo -e "Mac users should run this script without 'sudo'. Exiting..."
        exit 1
    fi

    CPUS=$(sysctl -n hw.ncpu)
    # ensure development environment is set correctly for clang
    $SUDO xcode-select -switch /Library/Developer/CommandLineTools

    if ! brew --version &>/dev/null; then
        echo -e "${cyan}Homebrew is required to install dependencies.${end}"
        exit 1
    fi

    brew install llvm@14 googletest google-benchmark lcov make wget cmake bash bc
    brew upgrade bash

    BREW_ROOT=$(brew --prefix)

    CLANG_TIDY=/usr/local/bin/clang-tidy
    if [[ ! -L "$CLANG_TIDY" ]]; then
        $SUDO ln -s "${BREW_ROOT}/opt/llvm@14/bin/clang-tidy" /usr/local/bin/clang-tidy
    fi
    GMAKE=/usr/local/bin/gmake
    if [[ ! -L "$GMAKE" ]]; then
        $SUDO ln -s $(xcode-select -p)/usr/bin/gnumake /usr/local/bin/gmake
    fi

    # install valid python version if not installed yet
    if [[ -z "$PY_INSTALLED" ]]; then
        PY_VERS=${PYTHON_VERSIONS[0]}
        FULL_PY="python${PY_VERS}"

        MAX_RETRIES=2
        while [[ $MAX_RETRIES -gt 0 ]]; do
            # try to install python version from homebrew and verify installation
            if brew install "${FULL_PY}"; then
                echo "${FULL_PY} installed successfully"
                PY_INSTALLED=${PY_VERS}
                break
            fi
            MAX_RETRIES=$((MAX_RETRIES - 1))
            sleep 1
        done
        if [[ $MAX_RETRIES -eq 0 ]]; then
            echo "Python3 install with homebrew failed, attempted on ${FULL_PY}"
            exit 1
        fi
    fi
# Linux install with apt
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # avoids getting stuck on interactive prompts which is essential for CI/CD
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt update -y
    $SUDO apt install -y build-essential wget cmake libgtest-dev libbenchmark-dev \
        lcov git software-properties-common rsync unzip bc

    # Add LLVM GPG key (apt-key is deprecated in Ubuntu 21.04+ so using gpg)
    wget -qO - https://apt.llvm.org/llvm-snapshot.gpg.key | \
        gpg --dearmor -o /usr/share/keyrings/llvm-archive-keyring.gpg
    echo "deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main" | \
        $SUDO tee /etc/apt/sources.list.d/llvm.list

    $SUDO apt update -y
    $SUDO apt install -y clang-format-14 clang-tidy-14
    $SUDO ln -sf $(which clang-format-14) /usr/local/bin/clang-format
    $SUDO ln -sf $(which clang-tidy-14) /usr/local/bin/clang-tidy

    # install valid python version if not installed yet
    if [[ -z "$PY_INSTALLED" ]]; then
        PY_VERS=${PYTHON_VERSIONS[0]}
        FULL_PY="python${PY_VERS}"

        # try to install python version from apt and verify installation
        $SUDO apt install -y software-properties-common
        $SUDO add-apt-repository -y ppa:deadsnakes/ppa
        $SUDO apt update -y

        DEADSNAKES_AVAIL=$(wget -q --spider http://ppa.launchpad.net/deadsnakes/ppa/ubuntu/dists/focal/Release; echo $?)
        if [[ $DEADSNAKES_AVAIL -ne 0 ]]; then
            echo "Failed to add deadsnakes which is needed to install python3"
            exit 1
        fi

        MAX_RETRIES=2
        while [[ $MAX_RETRIES -gt 0 ]]; do
            # install python3 valid version and venv module
            if $SUDO apt install -y ${FULL_PY}; then
                echo "${FULL_PY} installed successfully"
                PY_INSTALLED=${PY_VERS}
                break
            fi
            MAX_RETRIES=$((MAX_RETRIES - 1))
            sleep 1
        done
        if [[ $MAX_RETRIES -eq 0 ]]; then
            echo "Python3 install with apt and deadsnakes failed, attempted on ${FULL_PY}"
            exit 1
        fi
    fi
fi

# leave if no valid python version is installed
if ! which "python${PY_INSTALLED}" &> /dev/null; then
    echo "Python${PY_INSTALLED} not found in user path"
    exit 1
else
    # create virtual environment and install python packages for the valid python version
    PYTHON_PATH=$(which "python${PY_INSTALLED}")
    create_venv_install_python "${PYTHON_PATH}" ${PY_INSTALLED}
fi
echo "To activate the virtual env to run python, run 'source ./scripts/activate-venv.sh'"

PYTHON_TIDY=/usr/local/bin/run-clang-tidy.py
if [[ ! -f "${PYTHON_TIDY}" ]]; then
    echo -e "${green}Copying run-clang-tidy to /usr/local/bin${end}"
    wget https://raw.githubusercontent.com/llvm/llvm-project/e837ce2a32369b2e9e8e5d60270c072c7dd63827/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py
    $SUDO mv run-clang-tidy.py /usr/local/bin
fi

echo "Build environment setup complete."
echo "Next run './scripts/setup-dependencies.sh'."
