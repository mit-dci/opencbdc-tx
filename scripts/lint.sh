#!/bin/bash
set -e

echo "Linting..."

check_files=$(git ls-files \
              | grep -v -E ".jpg|.pdf|3rdparty" | cat)

whitespace_files=$(printf '%s' "${check_files[@]}" | xargs egrep -l " +$" | cat)

if [ -n "$whitespace_files" ]; then
    echo "The following files have trailing whitespace:"
    printf '%s\n' "${whitespace_files[@]}"
fi

newline_files=$(printf '%s' "${check_files[@]}" | xargs -r -I {} bash -c 'test "$(tail -c 1 "{}" | wc -l)" -eq 0 && echo {}' | cat)

if [ -n "$newline_files" ] ; then
    echo "The following files need an EOF newline:"
    printf '%s\n' "${newline_files[@]}"
fi

if [ -n "$whitespace_files" ] || [ -n "$newline_files" ] ; then
  exit 1
fi

check_format_files=$(git ls-files | grep -E "tools|tests|src|cmake-tests" \
                     | grep -E "\..*pp")
clang-format --style=file --Werror --dry-run ${check_format_files[@]}

if ! command -v clang-tidy &>/dev/null; then
  echo "clang-tidy does not appear to be installed. Please run ./scripts/configure.sh to install dependencies or install manually."
  exit 1
fi

if [ -z ${BUILD_DIR+x} ]; then
  echo "BUILD_DIR environment variable not found. Assuming default: build"
  export BUILD_DIR=build
  if [ ! -d "${BUILD_DIR}" ]; then
    echo "${BUILD_DIR} directory not found. Please set BUILD_DIR or run \`export BUILD_DIR=${BUILD_DIR}; build.sh\` before linting."
    exit 1
  fi
fi

python3 /usr/local/bin/run-clang-tidy.py -p ${BUILD_DIR} "tests/.*/.*\.cpp|src/.*/.*\.cpp|tools/.*/.*\.cpp"
