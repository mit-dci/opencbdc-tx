#!/bin/bash
# Retain our current working directory
PWD_DIR=$(pwd)

# Go to the telemetry-processor directory
SCRIPT_DIR="$(dirname "$0")"

cd $SCRIPT_DIR

echo "Building telemetry processor..."

# Build the processor binary
go build

echo "Starting postgres..."

docker-compose down

# Ensure the postgres instance is running
docker-compose up -d

# Get IP of postgres and pgadmin
#POSTGRES_IP=$(docker inspect --format '{{ .NetworkSettings.Networks.telemetry.IPAddress }}' opencbdc-tx-telemetry-db)
POSTGRES_IP="127.0.0.1"

sleep 5

../wait-for-it.sh -h $POSTGRES_IP -p 5432 -t 15

echo "Postgres IP: $POSTGRES_IP"

cd $PWD_DIR

echo "Importing telemetry data to database instance..."

DATABASE_URL="postgres://root:root@$POSTGRES_IP:5432/telemetry" $SCRIPT_DIR/telemetry

docker exec -it opencbdc-tx-telemetry-db psql -d telemetry

cd $SCRIPT_DIR

docker-compose down
