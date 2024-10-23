#!/bin/bash
set -e

BUILD_DIR="build"
config_file=$1

wallet0="wallet0.dat"
client0="client0.dat"

wallet1="wallet1.dat"
client1="client1.dat"

# Create wallet0 and save wallet id to var, mint
wallet0_id=$($BUILD_DIR/src/uhs/client/client-cli "$config_file" $client0 $wallet0 mint 10 10 | grep -E '^[^\W]+$')

echo "Wallet0_PK: $wallet0_id"

# Sync wallet0
$BUILD_DIR/src/uhs/client/client-cli "$config_file" $client0 $wallet0 sync

# Create wallet1 and save to var
wallet1_id=$($BUILD_DIR/src/uhs/client/client-cli "$config_file" $client1 $wallet1 newaddress | grep -E '^[^\W]+$')

echo "Wallet1_PK: $wallet1_id"

# Simulate transaction
importinput=$($BUILD_DIR/src/uhs/client/client-cli "$config_file" $client0 $wallet0 send 33 "$wallet1_id" | grep -E '^[a-zA-z0-9]{70,}')

echo "ImportInput: $importinput"

$BUILD_DIR/src/uhs/client/client-cli "$config_file" $client1 $wallet1 importinput "$importinput"

# Sync wallet1
$BUILD_DIR/src/uhs/client/client-cli "$config_file" $client1 $wallet1 sync

# Sync wallet1
$BUILD_DIR/src/uhs/client/client-cli "$config_file" $client1 $wallet1 info
