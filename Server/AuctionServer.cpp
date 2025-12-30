#include "AuctionServer.h"
#include "DatabaseManager.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

AuctionServer::AuctionServer(int port) : port(port) {}

AuctionServer::~AuctionServer() {}

std::vector<std::string> split(const std::string &s, char delimiter) {
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

  if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      -1) {
    std::cerr << "Bind failed" << std::endl;
    return;
  }
  if (listen(serverSocket, SOMAXCONN) == -1) {
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
          });
    }
  });
  timerThread.detach(); // Cho chạy ngầm độc lập
  // -----------------------

  std::cout << "=== SERVER STARTED ON PORT " << port << " ===" << std::endl;

  while (true) {
    sockaddr_in clientAddr;
    int len = sizeof(clientAddr);

    SocketType clientSocket =
        accept(serverSocket, (struct sockaddr *)&clientAddr, (socklen_t *)&len);
    // Tạo thread mới
    std::thread t(&AuctionServer::handleClient, this, clientSocket);
    t.detach();
  }
}


std::string AuctionServer::processCommand(SocketType clientSocket,
                                          const std::string &msg) {
  auto tokens = split(msg, '|');
  if (tokens.empty())
    return "";
  std::string cmd = tokens[0];

  if (cmd == "LOGIN") { // LOGIN|user|pass
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    std::string u = tokens[1];
    std::string p = tokens[2];

    if (DatabaseManager::getInstance().checkLogin(u, p)) {
      RoomManager::getInstance().loginUser(clientSocket, u);
      return "OK|LOGIN_SUCCESS|Welcome " + u;
    }
    return "ERR|LOGIN_FAILED";

  } else if (cmd == "REGISTER") { // REGISTER|user|pass
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    std::string u = tokens[1];
    std::string p = tokens[2];
    if (DatabaseManager::getInstance().registerUser(u, p)) {
      return "OK|REGISTER_SUCCESS";
    }
    return "ERR|USER_EXISTS";

  } else if (cmd == "CREATE_ROOM") {
    // CREATE_ROOM|RoomName|Item1,10,20,30;Item2,5,10,20
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";

    std::string roomName = tokens[1];
    std::string allProductsStr = tokens[2];

    std::vector<Product> productList;

    auto items = split(allProductsStr, ';');
    for (const std::string &itemStr : items) {
      auto details = split(itemStr, ',');
      if (details.size() < 4)
        continue;
      Product p;
      p.name = details[0];
      try {
        p.startPrice = std::stoi(details[1]);
        p.buyNowPrice = std::stoi(details[2]);
        p.duration = std::stoi(details[3]);
      } catch (...) {
        continue;
      }
      if (p.buyNowPrice > p.startPrice)
        productList.push_back(p);
    }

    if (productList.empty())
      return "ERR|NO_VALID_PRODUCTS";

    int newId = RoomManager::getInstance().createRoom(roomName, productList);
    return "OK|ROOM_CREATED|" + std::to_string(newId);

  } else if (cmd == "BUY_NOW") { // BUY_NOW|roomId
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    std::string broadcastMsg;

    auto broadcastFunc = [this](int roomId, std::string m) {
      this->broadcastToRoom(roomId, m);
    };

    bool success = RoomManager::getInstance().buyNow(rId, clientSocket,
                                                     broadcastMsg, broadcastFunc);
    if (success) {
      return "OK|BUY_SUCCESS";
    }
    // broadcastMsg có thể là ERR|... (manager set)
    if (broadcastMsg.rfind("ERR|", 0) == 0) {
      // trả về dòng đầu (không kèm \n)
      auto pos = broadcastMsg.find('\n');
      return broadcastMsg.substr(0, pos == std::string::npos ? broadcastMsg.size() : pos);
    }
    return "ERR|BUY_FAILED";

  } else if (cmd == "LIST_ROOMS") {
    std::string list = RoomManager::getInstance().getRoomList();
    return "OK|LIST|" + list;

  } else if (cmd == "JOIN_ROOM") { // JOIN_ROOM|roomId
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    std::string info;
    if (RoomManager::getInstance().joinRoom(rId, clientSocket, info)) {
      broadcastToRoom(rId, "USER_COUNT|" +
                              std::to_string((int)RoomManager::getInstance().getParticipants(rId).size()) +
                              "\n");
      return "OK|JOINED|" + info;
    }
    return "ERR|ROOM_NOT_FOUND";

  } else if (cmd == "LEAVE_ROOM") { // LEAVE_ROOM|roomId
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    if (RoomManager::getInstance().leaveRoom(rId, clientSocket)) {
      broadcastToRoom(rId, "USER_COUNT|" +
                              std::to_string((int)RoomManager::getInstance().getParticipants(rId).size()) +
                              "\n");
      return "OK|LEFT_ROOM";
    }
    return "ERR|ROOM_NOT_FOUND_OR_NOT_IN";

  } else if (cmd == "BID") { // BID|roomId|amount
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    int amount = std::stoi(tokens[2]);
    std::string broadcastMsg;

    if (RoomManager::getInstance().placeBid(rId, amount, clientSocket, broadcastMsg)) {
      broadcastToRoom(rId, broadcastMsg);
      return "OK|BID_SUCCESS|" + std::to_string(amount);
    }
    // manager trả lỗi cụ thể
    if (broadcastMsg.rfind("ERR|", 0) == 0) {
      auto pos = broadcastMsg.find('\n');
      return broadcastMsg.substr(0, pos == std::string::npos ? broadcastMsg.size() : pos);
    }
    return "ERR|PRICE_TOO_LOW";

  } else if (cmd == "CHAT") { // CHAT|roomId|Message
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    std::string chatMsg = tokens[2];
    std::string sender = RoomManager::getInstance().getUsername(clientSocket);
    if (sender.empty())
      sender = std::to_string(clientSocket);

    std::string broadcastMsg = "CHAT|" + sender + "|" + chatMsg + "\n";
    broadcastToRoom(rId, broadcastMsg);
    return "OK|CHAT_SENT";

  } else if (cmd == "VIEW_HISTORY") {
    std::string username = RoomManager::getInstance().getUsername(clientSocket);
    std::string history = DatabaseManager::getInstance().getHistoryList(username);
    return "OK|HISTORY|" + history;

  } else if (cmd == "LOGOUT") {
    return "OK|LOGOUT_SUCCESS";
  }

  return "ERR|UNKNOWN";
}


void AuctionServer::handleClient(SocketType clientSocket) {
  char buffer[1024];
  std::string dataBuffer = "";

  while (true) {
    memset(buffer, 0, 1024);
    int bytes = recv(clientSocket, buffer, 1024, 0);
    if (bytes <= 0)
      break;

    dataBuffer.append(buffer, bytes);

    // Process all complete commands (ending with \n)
    size_t pos = 0;
    while ((pos = dataBuffer.find('\n')) != std::string::npos) {
      std::string msg = dataBuffer.substr(0, pos);
      dataBuffer.erase(0, pos + 1);

      // Clean carriage return if present
      msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());

      if (msg.empty())
        continue;

      std::cout << "[RECV " << clientSocket << "]: " << msg << std::endl;

      std::string response = processCommand(clientSocket, msg);
      if (!response.empty()) {
        response += "\n";
        send(clientSocket, response.c_str(), response.length(), 0);
      }
    }
  }

  // --- CLEANUP WHEN CLIENT DISCONNECTS ---
  RoomManager::getInstance().removeClient(clientSocket);
  // --------------------------------------

  close(clientSocket);
}

void AuctionServer::broadcastToRoom(int roomId, const std::string &msg) {
  // Lấy danh sách socket từ Manager
  std::vector<SocketType> targets =
      RoomManager::getInstance().getParticipants(roomId);

  for (auto sock : targets) {
    send(sock, msg.c_str(), msg.length(), 0);
  }
  std::cout << "[BROADCAST Room " << roomId << "]: " << msg;
}

std::vector<std::string> AuctionServer::split(const std::string &s,
                                              char delimiter) {
  // (Giữ nguyên logic cũ)
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter))
    tokens.push_back(token);
  return tokens;
}
