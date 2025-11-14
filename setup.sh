#!/bin/bash

echo "Setting up Tor Chat Program..."

# Check if GCC is installed
if ! command -v gcc &> /dev/null; then
    echo "Error: GCC compiler is not installed."
    echo "Please install GCC:"
    echo "  Ubuntu/Debian: sudo apt install gcc"
    echo "  CentOS/RHEL: sudo yum install gcc"
    exit 1
fi

echo "Found GCC compiler"

# Check if Tor is installed (optional check)
if ! command -v tor &> /dev/null; then
    echo "Warning: Tor is not installed or not in PATH."
    echo "You need Tor to run this program."
    echo "Install Tor:"
    echo "  Ubuntu/Debian: sudo apt install tor"
    echo "  CentOS/RHEL: sudo yum install tor"
fi

# Compile server
echo "Compiling server..."
gcc -Wall -O2 -o server server.c
if [ $? -eq 0 ]; then
    echo "Server compiled successfully"
else
    echo "Error: Failed to compile server"
    exit 1
fi

# Compile client
echo "Compiling client..."
gcc -Wall -O2 -o client client.c
if [ $? -eq 0 ]; then
    echo "Client compiled successfully"
else
    echo "Error: Failed to compile client"
    exit 1
fi

echo ""
echo "Setup completed successfully!"
echo "You can now:"
echo "1. Run the server: ./server.sh"
echo "2. Configure Tor hidden service"
echo "3. Run the client: ./client <onion-address> <port>"
