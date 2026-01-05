# Auction Client (Qt C++)

This is the desktop client application for the Real-time Auction System, built with the Qt Framework.

## Features

-   **Graphical User Interface (GUI)**:
    -   **Lobby**: List of active auction rooms using `QTableWidget` for clear columns (ID, Name, Product, Price).
    -   **Room View**: Real-time updates of bids, chat, countdown timer, and product status.
    -   **Profile**: View auction history, won items, and **manage finance** (Add Funds).
-   **Real-time Updates**: Uses `QTcpSocket` to receive live updates (bids, chat, timers, balance) from the Server.
-   **Finance Integration**:
    -   Input field to add funds to account.
    -   Real-time balance display in Lobby and Room views.
-   **Advanced UI**:
    -   Refactored "History" and "Won Items" to `QTableWidget` for better data readability.
    -   Sort/Filter rooms by Price or Name.

## Prerequisites

-   **Qt 5 or Qt 6**: Core, Gui, Widgets, Network modules.
-   **C++ Compiler**: Compatible with your Qt version (e.g., g++).
-   **Make/QMake**: For building.

## Build & Run

1.  **Run qmake**:
    ```bash
    qmake
    ```
    *Note: If `qmake` is not found, ensure it's in your PATH or typically located at `/usr/lib/qt5/bin/qmake`.*

2.  **Build**:
    ```bash
    make
    ```

3.  **Run**:
    ```bash
    ./AuctionClient
    ```

## Configuration

By default, the client tries to connect to `0.tcp.ap.ngrok.io:13541` (or localhost depending on recent edits).
To change the server address, modify `mainwindow.cpp` inside `on_btnLogin_clicked` or `on_btnOpenRegister_clicked`.

## Usage

1.  **Register/Login**: Create an account (Starts with 500k VND) or log in.
2.  **Lobby**: Browse active rooms. Search or Sort as needed.
3.  **Join Room**: Double click or select and click "Join".
4.  **Bidding**: Watch the countdown. Place a bid (must be +10k > current).
5.  **Profile**: Check "Tài chính" tab to add fake money for testing.

## Troubleshooting

-   **Connection Refused**: Ensure the Server is running and the IP/Port in `mainwindow.cpp` matches the Server.
-   **Compilation Errors**: If `ui_*.h` files are missing, run `qmake` again before `make`.
