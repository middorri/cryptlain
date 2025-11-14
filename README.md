Chat Over Tor
  
[![License](https://img.shields.io/badge/license-Unlicense-blue.svg)](https://unlicense.org/) [![x](https://img.shields.io/badge/x-@middorri-blue.svg)](https://x.com/middorri)  
  
A simple chat program that works over Tor for anonymous communication. The server runs as a Tor hidden service, and clients connect through the Tor network.
# Features

    Secure authentication with password protection

    Anonymous communication over Tor

    Simple command-line interface

    Real-time messaging

    Color-coded messages for better readability

# Requirements

    GCC compiler

    Tor service running on the system

    Basic networking libraries (usually included in Linux)

On Ubuntu/Debian

    sudo apt update
    sudo apt install tor

On CentOS/RHEL

    sudo yum install tor

# Installation

Clone or download the source files:

    server.c - The chat server

    client.c - The chat client

    setup.sh - Setup script

    server.sh - Server startup script

Setup

    chmod +x setup.sh
    ./setup.sh

Starting the Server

    chmod +x server.sh
    ./server.sh

Configure Tor hidden service:
Edit the Tor configuration file (/etc/tor/torrc or your system's equivalent):
text

HiddenServiceDir /var/lib/tor/chat_service/
HiddenServicePort 1234 127.0.0.1:1234

Restart Tor:

    sudo systemctl restart tor

Get your onion address:
sudo cat /var/lib/tor/chat_service/hostname

    This will show your onion address (e.g., abcdefghijklmnop.onion)

Using the Client

    Run the client with the server's onion address and port:
    bash

./client abcdefghijklmnop.onion 1234

    Enter the password when prompted (default: secret123)

    Start chatting - type messages and press Enter to send

# Commands

Server commands:

    /quit or /exit - Shut down the server

    /status - Show server status

Client commands:

    /quit or /exit - Disconnect from server

    /help - Show available commands

    /clear - Clear the screen

# Configuration

    Change the default password in server.c (look for PASSWORD "secret123")

    Modify the listening port in server.c (look for PORT 1234)

    The server listens on localhost by default for security

Security Notes

    Change the default password before using in production

    The server runs as a Tor hidden service for anonymity

    All communications go through the Tor network

    Keep your Tor service updated

Troubleshooting

    Make sure Tor is running: sudo systemctl status tor

    Check firewall settings if connections fail

    Verify the onion address and port number

    Ensure the password matches between server and client
