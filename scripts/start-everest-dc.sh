#!/usr/bin/env bash

# Get the directory of the current script
SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"

# Change to the script's directory and then to its parent
cd "$SCRIPT_DIR" || exit 1 # Change to script's directory, exit if failed
cd .. || exit 1            # Change to parent directory, exit if failed

# Ensure the MQTT Docker container is running
docker start mqtt-server

# Invoke the Everest DC run script
./build/run-scripts/run-sil-dc-test.sh
