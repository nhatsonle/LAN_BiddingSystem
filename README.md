# Real-time Auction System

A complete real-time auction platform featuring a multi-threaded C++ Server and a Qt-based C++ Client.

## 📂 Project Structure

This repository contains two main components:

-   **[Server](./Server/README.md)**: The backend logic handling socket connections, database management (SQLite), and auction rules.
-   **[AuctionClient](./AuctionClient/README.md)**: The desktop application (GUI) for users to register, login, create rooms, and participate in auctions.

## ✨ Key Features

-   **Real-time Interaction**: Bids, chat messages, and timer updates are broadcasted instantly to all participants in a room.
-   **Multi-Room Support**: Multiple auction rooms can run concurrently, each with its own product queue.
-   **Robust Persistence**: All data (users, rooms, products, history) is stored in a SQLite database.
-   **Deployment Friendly**: Includes guides for local usage and exposing to the internet via ngrok.

## 🚀 Quick Start

### 1. Start the Server
Navigate to the `Server` directory and build the project:
```bash
cd Server
make
./auction_server
```
The server will start on port `13541` (default).

### 2. Start the Client
Navigate to the `AuctionClient` directory (in a new terminal):
```bash
cd AuctionClient
qmake
make
./AuctionClient
```

## 📚 Documentation

For detailed instructions on setup, configuration, and usage, please refer to the specific README files in each directory:

-   [Server Documentation](./Server/README.md)
-   [Client Documentation](./AuctionClient/README.md)
-   [Deployment Guide](./deployment_guide.md) (Check artifacts if available, or ask the AI assistant)

## 🛠 Technologies

-   **Language**: C++ (C++11/17)
-   **Frameworks**: Qt 5/6 (Client), POSIX Threads/Sockets (Server)
-   **Database**: SQLite3
