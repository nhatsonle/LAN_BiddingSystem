# Online Auction System – Client Side

This repository contains the **client-side (GUI)** application for the **Online Auction System**, built using the **Qt Framework**.  
The client connects to the server via **TCP sockets** to support **real-time interactions** such as live bidding updates and countdown timers.

---

## 📌 Overview

- **Application Type**: Desktop GUI Client  
- **Architecture**: Event-driven (Signal & Slots)  
- **Communication**: TCP Socket (asynchronous)  
- **Purpose**: Provide a real-time user interface for participating in online auctions  

---

## 🛠 Technology Stack

| Category | Technology |
|--------|------------|
| Framework | Qt 5 / Qt 6 |
| Language | C++ |
| Networking | `QTcpSocket` |
| UI Components | `QTableWidget`, `QStackedWidget`, Qt Designer |
| Architecture | Event-driven (Signals & Slots) |
| Platform | Windows / Linux |

---

## 📂 Project Structure

### AuctionClient/
- AuctionClient.pro # Qt project configuration file
- main.cpp # Application entry point, global stylesheets
- mainwindow.ui # [View] Main UI (Login, Lobby, Room)
- mainwindow.h/.cpp # [Controller] UI events & socket data handling
- createroomdialog.ui # [View] Create room dialog & product input
- createroomdialog.h/.cpp # [Logic] Add/Edit/Delete products in temporary queue
- registerdialog.h/.cpp # [Logic] User registration dialog

---

## 🚀 Build & Run Guide

### Prerequisites

- **Qt Creator**
- **Compiler**
  - MinGW (Windows)
  - GCC / Clang (Linux)

---

### Build & Run Steps

1. Open **Qt Creator**
2. Double-click `AuctionClient.pro`
3. Select the appropriate **Kit** (e.g., *Desktop Qt 6.5 MinGW*)
4. Click **Run** (▶) or press **Ctrl + R**

> ⚠️ The **Server must be running first** before performing login or registration from the client.

---

## 🧩 Data Flow & Communication Model

The client operates in a **fully asynchronous** manner.

### Sending Commands

```cpp
m_socket->write("CMD|ARG\n");
```
- Commands are sent immediately
- The client does not block waiting for a response.

### Receiving Data
- All incoming data from the server is handled in:
``` cpp
MainWindow::onReadyRead()
```
- Incoming messages are:
 - Read from the socket
 - Parsed as text commands
 - Reflected directly in the UI

### Best Practice Tip
Room IDs, product IDs, and bid values are commonly stored in:
``` cpp
Qt::UserRole
```
This allows efficient access when handling UI click events without string parsing.

## Extending the Client 
### Example: Add a Quick Chat Feature (Already implemented)
### Step 1: Design UI (.ui)
- Open mainwindow.ui
- Drag a QPushButton
- Set objectName to:
``` nginx
btnChatQuick
```
### Step 2: Create Slot Function
- Right-click the button
- Select Go to slot... -> clicked()
Qt Creator generates:
```cpp
void MainWindow::on_btnChatQuick_clicked()
{
    
}
```
### Step 3: Send Command to Server
``` cpp
void MainWindow::on_btnChatQuick_clicked() {
    if (m_currentRoomId != 0) {
        // Command format: CHAT|RoomID|Content
        QString msg = "CHAT|" 
                    + QString::number(m_currentRoomId) 
                    + "|Giá cao quá!\n";
        m_socket->write(msg.toUtf8());
    }
}
```
### Step 4: Handle Response (Optional)
If the server broadcasts chat messages, handle them in onReadyRead():
``` cpp
else if (line.startsWith("CHAT_MSG")) {
    // Format: CHAT_MSG|User|Content
    QStringList parts = line.split('|');
    ui->txtRoomLog->append(parts[1] + ": " + parts[2]);
}
```

## Design Notes
- The UI is non-blocking and fully event-driven
- All network I/O is handled asynchronously via Qt signals
- Business logic remains minimal on the client side
- The client strictly follows the server-defined protocol




