#include "AuctionServer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>

AuctionServer::AuctionServer(int port) : port(port) {
}

AuctionServer::~AuctionServer() {
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
    
    if (cmd == "LOGIN") {
        return "OK|LOGIN_SUCCESS|Welcome";
    }
    else if (cmd == "CREATE_ROOM") { // CREATE_ROOM|Name|Price
        int newId = RoomManager::getInstance().createRoom(tokens[1], std::stoi(tokens[2]));
        return "OK|ROOM_CREATED|" + std::to_string(newId);
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