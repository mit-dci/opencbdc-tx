#!/bin/bash
set -e

# Usage: ./scripts/clang-format.sh
echo "Running clang format..."

check_format_files=$(git ls-files | grep -E "tools|tests|src|cmake-tests" \
                     | grep -E "\..*pp")
clang-format -i --style=file ${check_format_files[@]}

echo "Done."
