#include "AuctionServer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>
#include <sstream>
#include "DatabaseManager.h"

AuctionServer::AuctionServer(int port) : port(port) {
}

AuctionServer::~AuctionServer() {
}


std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void AuctionServer::start() {
    // 1. Tạo Socket & Bind (Code cũ của bạn)
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if(bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Bind failed" << std::endl;
        return;
    }
    if(listen(serverSocket, SOMAXCONN) == -1) {
        std::cerr << "Listen failed" << std::endl;
        return;
    }

    // --- TẠO LUỒNG TIMER ---
    std::thread timerThread([this]() {
        while (true) {
            // 1. Ngủ 1 giây
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 2. Gọi Manager cập nhật và cung cấp hàm gửi tin (Lambda Function)
            RoomManager::getInstance().updateTimers(
                [this](int roomId, std::string msg) {
                    // Đây là hàm callback: Manager nhờ Server gửi msg đến roomId
                    this->broadcastToRoom(roomId, msg);
                }
            );
        }
    });
    timerThread.detach(); // Cho chạy ngầm độc lập
    // -----------------------

    std::cout << "=== SERVER STARTED ON PORT " << port << " ===" << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int len = sizeof(clientAddr);

        SocketType clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, (socklen_t*)&len);
        // Tạo thread mới
        std::thread t(&AuctionServer::handleClient, this, clientSocket);
        t.detach();
    }
}

void AuctionServer::handleClient(SocketType clientSocket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int bytes = recv(clientSocket, buffer, 1024, 0);
        if (bytes <= 0) break;

        std::string msg(buffer);
        // Clean chuỗi
        msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());
        msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());
        
        std::cout << "[RECV " << clientSocket << "]: " << msg << std::endl;
        
        std::string response = processCommand(clientSocket, msg);
        if (!response.empty()) {
            response += "\n";
            send(clientSocket, response.c_str(), response.length(), 0);
        }
    }
    
    close(clientSocket);
}

std::string AuctionServer::processCommand(SocketType clientSocket, const std::string& msg) {
    auto tokens = split(msg, '|');
    if (tokens.empty()) return "";
    std::string cmd = tokens[0];

    // --- GỌI QUA ROOM MANAGER ---
    
    // ... trong processCommand ...
    if (cmd == "LOGIN") { // LOGIN|user|pass
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        std::string u = tokens[1];
        std::string p = tokens[2];

        // GỌI DB ĐỂ CHECK
        if (DatabaseManager::getInstance().checkLogin(u, p)) {
            return "OK|LOGIN_SUCCESS|Welcome " + u;
        } else {
            return "ERR|LOGIN_FAILED";
        }
    }
    else if (cmd == "REGISTER") { // Client gửi: REGISTER|user|pass
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        
        std::string u = tokens[1];
        std::string p = tokens[2];

        // Gọi DB
        if (DatabaseManager::getInstance().registerUser(u, p)) {
            return "OK|REGISTER_SUCCESS";
        } else {
            return "ERR|USER_EXISTS"; // Báo lỗi trùng tên
        }
    }
    else if (cmd == "CREATE_ROOM") {
        // Format: CREATE_ROOM|RoomName|Item1,10,20,30;Item2,5,10,20
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        
        std::string roomName = tokens[1];
        std::string allProductsStr = tokens[2]; // Chuỗi dài chứa tất cả SP
        
        std::vector<Product> productList;
        
        // 1. Tách các sản phẩm bằng dấu chấm phẩy ';'
        std::vector<std::string> items = split(allProductsStr, ';');
        
        for (const std::string& itemStr : items) {
            // 2. Tách chi tiết từng sản phẩm bằng dấu phẩy ','
            // Format: Name,Start,BuyNow,Duration
            std::vector<std::string> details = split(itemStr, ',');
            
            if (details.size() >= 4) {
                Product p;
                p.name = details[0];
                try {
                    p.startPrice = std::stoi(details[1]);
                    p.buyNowPrice = std::stoi(details[2]);
                    p.duration = std::stoi(details[3]);
                    
                    // Logic validate cơ bản
                    if (p.buyNowPrice > p.startPrice) {
                        productList.push_back(p);
                    }
                } catch (...) {
                    continue; // Bỏ qua nếu lỗi format số
                }
            }
        }
        
        if (productList.empty()) return "ERR|NO_VALID_PRODUCTS";

        // Gọi Manager tạo phòng
        int newId = RoomManager::getInstance().createRoom(roomName, productList);
        return "OK|ROOM_CREATED|" + std::to_string(newId);
    }
    else if (cmd == "BUY_NOW") {
        if (tokens.size() < 2) return "ERR|MISSING_ARGS";
        int rId = std::stoi(tokens[1]);
        std::string broadcastMsg;

        // --- SỬA ĐOẠN NÀY ---
        
        // 1. Tạo một lambda function để làm callback broadcast
        // Lambda này sẽ gọi hàm broadcast() của AuctionServer
        auto broadcastFunc = [this](int roomId, std::string msg) {
            this->broadcastToRoom(roomId, msg);
        };

        // 2. Gọi hàm buyNow với 4 tham số (tham số cuối là broadcastFunc)
        bool success = RoomManager::getInstance().buyNow(rId, clientSocket, broadcastMsg, broadcastFunc);
        
        // --------------------

        if (success) {
            // Server tự broadcast kết quả bên trong RoomManager rồi, 
            // ở đây chỉ cần trả về OK cho người mua
            return "OK|BUY_SUCCESS"; 
        } else {
            return "ERR|BUY_FAILED";
        }
    }
    else if (cmd == "LIST_ROOMS") {
        std::string list = RoomManager::getInstance().getRoomList();
        return "OK|LIST|" + list;
    }
    else if (cmd == "JOIN_ROOM") { // JOIN_ROOM|ID
        std::string info;
        if (RoomManager::getInstance().joinRoom(std::stoi(tokens[1]), clientSocket, info)) {
            return "OK|JOINED|" + info;
        }
        return "ERR|ROOM_NOT_FOUND";
    }
    else if (cmd == "BID") { // BID|ID|Amount
        int rId = std::stoi(tokens[1]);
        int amount = std::stoi(tokens[2]);
        std::string broadcastMsg;
        
        // Gọi hàm placeBid của Manager
        if (RoomManager::getInstance().placeBid(rId, amount, clientSocket, broadcastMsg)) {
            // Nếu thành công thì Broadcast ngay tại đây
            broadcastToRoom(rId, broadcastMsg);
            return "OK|BID_SUCCESS|" + std::to_string(amount);
        } else {
            return "ERR|PRICE_TOO_LOW";
        }
    }
    else if (cmd == "CHAT") { // CHAT|ID|Message
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        int rId = std::stoi(tokens[1]);
        std::string chatMsg = tokens[2];

        // Broadcast chat (hiện tại gắn nhãn bằng socket id người gửi)
        std::string broadcastMsg = "CHAT|" + std::to_string(clientSocket) + "|" + chatMsg + "\n";
        broadcastToRoom(rId, broadcastMsg);
        return "OK|CHAT_SENT";
    }
    else if (cmd == "LEAVE_ROOM") { // Client gửi: LEAVE_ROOM|<room_id>
        if (tokens.size() < 2) return "ERR|MISSING_ARGS";
        
        int rId = std::stoi(tokens[1]);
        if (RoomManager::getInstance().leaveRoom(rId, clientSocket)) {
            return "OK|LEFT_ROOM";
        } else {
            return "ERR|ROOM_NOT_FOUND_OR_NOT_IN";
        }
    }
    else if (cmd == "VIEW_HISTORY") {
        std::string history = DatabaseManager::getInstance().getHistoryList();
        return "OK|HISTORY|" + history;
    }
    // ...
    
    return "ERR|UNKNOWN";
}

void AuctionServer::broadcastToRoom(int roomId, const std::string& msg) {
    // Lấy danh sách socket từ Manager
    std::vector<SocketType> targets = RoomManager::getInstance().getParticipants(roomId);
    
    for (auto sock : targets) {
        send(sock, msg.c_str(), msg.length(), 0);
    }
    std::cout << "[BROADCAST Room " << roomId << "]: " << msg;
}

std::vector<std::string> AuctionServer::split(const std::string &s, char delimiter) {
    // (Giữ nguyên logic cũ)
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) tokens.push_back(token);
    return tokens;
}
