#!/bin/bash

echo "Starting Tor Chat Server..."

# Check if server executable exists
if [ ! -f "./server" ]; then
    echo "Error: Server executable not found."
    echo "Please run ./setup.sh first to compile the server."
    exit 1
fi

# Check if executable
if [ ! -x "./server" ]; then
    chmod +x ./server
fi

# Start the server
./server
