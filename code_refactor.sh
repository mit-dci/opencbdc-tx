#!/bin/bash

# Code map
#  -> PARSEC
#  -> parsec
#  -> parsec

# Docs map
# programmability -> PARSEC
exclude_dirs=(".git")
# List of directories to edit
directories=("scripts" "src" "tools" "tests" ".")
FLAGS=
# Loop through directories
for dir in "${directories[@]}"; do
  if [[ "$dir" == "." ]]; then
    # ignore directories that aren't specified, and ignore the rename scripts
    FLAGS="-maxdepth 1 ! -name file_rename.sh ! -name code_refactor.sh"
    #FLAGS=""
  else
    FLAGS=""
  fi

  find "$dir" $FLAGS -type f -exec sed -i "s/3PC/PARSEC/g" {} +

  find "$dir" $FLAGS -type f -exec sed -i "s/3pc/parsec/g" {} +

  find "$dir" $FLAGS -type f -exec sed -i "s/threepc/parsec/g" {} +
  find "$dir" $FLAGS -type f -exec sed -i "s/programmability/parsec/g" {} +
  find "$dir" $FLAGS -type f -exec sed -i "s/Programmability/PArSEC/g" {} +
  find "$dir" $FLAGS -type f -exec sed -i "s/THREEPC/PARSEC/g" {} +
done


# docs and scripts have similar naming convention
proper_directories=("docs")
FLAGS="-maxdepth 1 ! -name contributing.md"
for dir in "${proper_directories[@]}"; do
  find "$dir" $FLAGS -type f -exec sed -i "s/3PC/PArSEC/g" {} +

  find "$dir" $FLAGS -type f -exec sed -i "s/3pc/PArSEC/g" {} +

  find "$dir" $FLAGS -type f -exec sed -i "s/threepc/PArSEC/g" {} +
  find "$dir" $FLAGS -type f -exec sed -i "s/programmability/PArSEC/g" {} +
  find "$dir" $FLAGS -type f -exec sed -i "s/Programmability/PArSEC/g" {} +
done

