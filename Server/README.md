# Online Auction System – Server Side

This repository contains the **server-side (backend)** implementation of an **Online Auction System**, developed in **pure C++**.  
The server supports **multiple concurrent clients** using a **multithreaded architecture**, communicates via **raw TCP sockets**, and persists data using an embedded **SQLite** database.

---

## 📌 Overview

- **Architecture**: Multithreaded TCP Server  
- **Concurrency Model**: One thread per client  
- **Persistence**: SQLite (embedded, file-based)  
- **Design Patterns**:
  - Singleton Pattern (Managers)
  - Mutex / Recursive Mutex (thread safety)

The system is organized into three logical layers:
- **Network Layer** – Socket handling and protocol parsing  
- **Business Logic Layer** – Auction rooms and bidding logic  
- **Data Layer** – Database access and persistence  

---

## 🛠 Technology Stack

| Category | Technology |
|-------|-----------|
| Language | C++ (C++11 / C++17) |
| Networking | Raw TCP Sockets (POSIX / Linux) |
| Concurrency | `std::thread`, `pthread`, `mutex` |
| Database | SQLite (Amalgamation) |
| Build Tool | Makefile |
| Platform | Linux / WSL / Windows (MSVC compatible) |

---

## 📂 Directory Structure

Server/
├── main.cpp # Entry point: initializes database and server
├── AuctionServer.h/.cpp # Network layer: socket handling and client threads
├── RoomManager.h/.cpp # Business logic: rooms, bidding, timers
├── DatabaseManager.h/.cpp # Data layer: SQL operations (login, register, history)
├── Room.h # Data models: Room, Product, SoldItem
├── sqlite3.c / sqlite3.h # SQLite amalgamation source
├── auction_system.db # SQLite database (auto-generated)
└── Makefile # Build configuration


---

## 🚀 Build & Run

### Prerequisites

- **Compiler**
  - `g++` (Linux / WSL) or **MSVC** (Windows)
- **Libraries**
  - `pthread`
  - `libdl` (required by SQLite on Linux)

---

### Build (Makefile)

```bash
make

This command compiles:
- C source (sqlite3.c)
- C++ server source files

### Run the Server
- **Linux**
```bash
./server
```
The server listens on port 8080 by default.

### Communication Protocol

The server uses a simple text-based protocol for communication with clients. Each message is formatted as follows:

- Fields are separated by `|`
- Each command ends with `\n`
- Requests and responses are UTF-8 strings

### Supported Commands

| Function     | Client Request | Server Response |           |              |                     |               |
| -------  | ----- | -------------- | --------------- | --------- | ------------ | ------------------- | ------------- |
| Login        | `LOGIN         | username        | password` | `OK          | LOGIN_SUCCESS`/`ERR | LOGIN_FAILED` |
| Register     | `REGISTER      | username        | password` | `OK          | REGISTER_SUCCESS`   |               |
| Create Room  | `CREATE_ROOM   | RoomName`       | `OK       | ROOM_CREATED | RoomID`             |               |
| Join Room    | `JOIN_ROOM     | RoomID`         | `OK       | JOINED`      |                     |               |
| Bid          | `BID           | RoomID          | Amount`   | `OK          | BID_ACCEPTED`       |               |
| Get Products | `GET_PRODUCTS  | RoomID`         | `OK       | PRODUCT_LIST | ...`                |               |

## Extending the System
### Example: Add Kick User Feature
### Step 1: Define the Protocol
- Client Request: `KICK|RoomID|UserID`

### Step 2: Handle Command in AuctionServer.cpp
```cpp
else if (cmd == "KICK") {
    int roomId = std::stoi(tokens[1]);
    int userId = std::stoi(tokens[2]);

    if (RoomManager::getInstance().kickUser(roomId, userId)) {
        return "OK|KICK_SUCCESS";
    }
    return "ERR|CANNOT_KICK";
}

```

### Step 3: Implement Business Logic in RoomManager.cpp
```cpp
bool RoomManager::kickUser(int roomId, int userId) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);

    // 1. Find room
    // 2. Locate user in participants list
    // 3. Remove user from room
    // 4. Optionally close user's socket

    return true;
}
```
⚠️ Always lock shared resources using mutexes to ensure thread safety.

## Concurrency and Design Notes
- Each client connection is handled in a dedicated thread
- Shared data structures are protected with mutex locks
- Singleton Managers provide centralized and controlled access
- All database operations are abstracted via DatabaseManager
