#include "AuctionServer.h"
#include "DatabaseManager.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

static bool sendAll(SocketType sock, const char *data, size_t len) {
  size_t totalSent = 0;
  while (totalSent < len) {
    ssize_t sent = send(sock, data + totalSent, len - totalSent, 0);
    if (sent < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (sent == 0)
      return false;
    totalSent += static_cast<size_t>(sent);
  }
  return true;
}

static std::string sanitizeDescription(const std::string &input) {
  std::string cleaned;
  cleaned.reserve(input.size());
  for (char c : input) {
    if (c == ',' || c == ';' || c == '|' || c == '\n' || c == '\r') {
      cleaned.push_back(' ');
    } else {
      cleaned.push_back(c);
    }
  }

  std::istringstream iss(cleaned);
  std::string word;
  std::string out;
  int count = 0;
  while (iss >> word) {
    if (count >= 50)
      break;
    if (count > 0)
      out.push_back(' ');
    out += word;
    ++count;
  }
  return out;
}

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

static std::vector<Product> parseProductPayload(const std::string &payload) {
  std::vector<Product> productList;
  auto items = split(payload, ';');
  for (const auto &item : items) {
    if (item.empty())
      continue;
    auto details = split(item, ',');
    if (details.size() < 4)
      continue;
    Product p;
    p.name = details[0];
    if (p.name.empty())
      continue;
    p.description = (details.size() >= 5) ? sanitizeDescription(details[4]) : "";
    try {
      int startPrice = std::stoi(details[1]);
      int buyNowPrice = std::stoi(details[2]);
      int duration = std::stoi(details[3]);
      if (startPrice < 0 || buyNowPrice <= startPrice || duration <= 0)
        continue;
      p.startPrice = startPrice;
      p.buyNowPrice = buyNowPrice;
      p.duration = std::min(duration, 1800);
    } catch (...) {
      continue;
    }
    productList.push_back(p);
  }
  return productList;
}

void AuctionServer::start() {
  // 1. Tạo Socket & Bind
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

  // --- GỌI QUA ROOM MANAGER ---

  // ... trong processCommand ...
  if (cmd == "LOGIN") { // LOGIN|user|pass
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    std::string u = tokens[1];
    std::string p = tokens[2];
    int userId = -1;

    // CHECK DUPLICATE LOGIN
    if (RoomManager::getInstance().isUserLoggedIn(u)) {
      return "ERR|ALREADY_LOGGED_IN|Account is already active";
    }

    // GỌI DB ĐỂ CHECK
    if (DatabaseManager::getInstance().checkLogin(u, p, userId)) {
      RoomManager::getInstance().loginUser(clientSocket, userId, u);
      return "OK|LOGIN_SUCCESS|Welcome " + u;
    } else {
      return "ERR|LOGIN_FAILED";
    }
  } else if (cmd == "REGISTER") { // Client gửi: REGISTER|user|pass
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";

    std::string u = tokens[1];
    std::string p = tokens[2];

    // Gọi DB
    if (DatabaseManager::getInstance().registerUser(u, p)) {
      return "OK|REGISTER_SUCCESS";
    } else {
      return "ERR|USER_EXISTS"; // Báo lỗi trùng tên
    }
  } else if (cmd == "CREATE_ROOM") {
    // Format: CREATE_ROOM|RoomName|Name,Start,BuyNow,Duration[,Description];...
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";

    std::string roomName = tokens[1];
    std::string allProductsStr = tokens[2];
    std::string startTime = tokens.size() >= 4 ? tokens[3] : "";

    std::vector<Product> productList = parseProductPayload(allProductsStr);
    if (productList.empty())
      return "ERR|NO_VALID_PRODUCTS";

    int ownerUserId = RoomManager::getInstance().getUserId(clientSocket);
    if (ownerUserId <= 0)
      return "ERR|NOT_LOGIN";

    int newId = RoomManager::getInstance().createRoom(
        roomName, productList, clientSocket, ownerUserId, startTime);
    if (newId == -1)
      return "ERR|CREATE_FAILED";
    return "OK|ROOM_CREATED|" + std::to_string(newId);
  } else if (cmd == "EDIT_ROOM") {
    if (tokens.size() < 4)
      return "ERR|MISSING_ARGS";
    int roomId = std::stoi(tokens[1]);
    std::string roomName = tokens[2];
    std::string productPayload = tokens[3];
    std::string startTime = tokens.size() >= 5 ? tokens[4] : "";

    std::vector<Product> productList = parseProductPayload(productPayload);
    if (productList.empty())
      return "ERR|NO_VALID_PRODUCTS";

    std::string error;
    if (RoomManager::getInstance().editRoom(roomId, clientSocket, roomName,
                                            productList, startTime, error)) {
      return "OK|ROOM_UPDATED";
    }
    return error.empty() ? "ERR|EDIT_FAILED" : error;
  } else if (cmd == "BUY_NOW") {
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    std::string broadcastMsg;

    if (!RoomManager::getInstance().isRoomStarted(rId))
      return "ERR|ROOM_NOT_STARTED";

    // 1. Tạo một lambda function để làm callback broadcast
    auto broadcastFunc = [this](int roomId, std::string msg) {
      this->broadcastToRoom(roomId, msg);
    };

    // 2. Gọi hàm buyNow
    bool success = RoomManager::getInstance().buyNow(
        rId, clientSocket, broadcastMsg, broadcastFunc);

    if (success) {
      return "OK|BUY_SUCCESS";
    } else {
      return "ERR|BUY_FAILED";
    }
  } else if (cmd == "MY_ROOMS") {
    int userId = RoomManager::getInstance().getUserId(clientSocket);
    if (userId <= 0)
      return "ERR|NOT_LOGGED_IN";
    std::string list = DatabaseManager::getInstance().getMyRooms(userId);
    auto now = std::chrono::system_clock::now();
    std::time_t nowT = std::chrono::system_clock::to_time_t(now);
    return "OK|MY_ROOMS|" + std::to_string((long long)nowT) + "|" + list;
  } else if (cmd == "LIST_ROOMS") {
    std::string list = RoomManager::getInstance().getRoomList();
    return "OK|LIST|" + list;
  } else if (cmd == "STOP_ROOM") {
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int roomId = std::stoi(tokens[1]);
    std::string broadcast;
    std::string error;
    if (RoomManager::getInstance().stopRoom(roomId, clientSocket, broadcast,
                                            error)) {
      if (!broadcast.empty())
        broadcastToRoom(roomId, broadcast);
      return "OK|ROOM_STOPPED";
    }
    return error.empty() ? "ERR|STOP_FAILED" : error;
  } else if (cmd == "GET_ROOM_EDIT_DATA") { // GET_ROOM_EDIT_DATA|RoomID
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int roomId = std::stoi(tokens[1]);
    std::string name;
    std::string startTime;
    std::string products;
    std::string error;
    if (!RoomManager::getInstance().getRoomEditData(
            roomId, clientSocket, name, startTime, products, error)) {
      return error.empty() ? "ERR|EDIT_DATA_FAILED" : error;
    }
    auto now = std::chrono::system_clock::now();
    std::time_t nowT = std::chrono::system_clock::to_time_t(now);
    return "OK|ROOM_EDIT_DATA|" + std::to_string((long long)nowT) + "|" +
           std::to_string(roomId) + "|" + name + "|" + startTime + "|" +
           products;
  } else if (cmd == "JOIN_ROOM") { // JOIN_ROOM|ID
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";

    int roomId = std::stoi(tokens[1]);
    std::string info;
    if (RoomManager::getInstance().joinRoom(roomId, clientSocket, info)) {
      // Broadcast thông báo vào phòng
      std::string username =
          RoomManager::getInstance().getUsername(clientSocket);
      std::string joinMsg = "CHAT|" + username + "|đã tham gia phòng\n";
      broadcastToRoom(roomId, joinMsg);

      std::string countMsg =
          "ROOM_MEMBER_COUNT|" + std::to_string(roomId) + "|" +
          std::to_string(RoomManager::getInstance().getParticipantCount(roomId)) +
          "\n";
      broadcastToRoom(roomId, countMsg);
      // Response chỉ gồm 1 dòng OK|JOINED (chat join được gửi qua broadcast)
      auto now = std::chrono::system_clock::now();
      std::time_t nowT = std::chrono::system_clock::to_time_t(now);
      return "OK|JOINED|" + info + "|" + std::to_string((long long)nowT);
    }
    return "ERR|ROOM_NOT_FOUND";
  } else if (cmd == "BID") { // BID|ID|Amount
    int rId = std::stoi(tokens[1]);
    int amount = std::stoi(tokens[2]);
    std::string broadcastMsg;

    // Gọi hàm placeBid của Manager
    std::string errorMsg;
    if (RoomManager::getInstance().placeBid(rId, amount, clientSocket,
                                            broadcastMsg, errorMsg)) {
      // Nếu thành công thì Broadcast ngay tại đây
      broadcastToRoom(rId, broadcastMsg);
      return "OK|BID_SUCCESS|" + std::to_string(amount);
    } else {
      return errorMsg.empty() ? "ERR|PRICE_TOO_LOW" : errorMsg;
    }
  } else if (cmd == "CHAT") { // CHAT|ID|Message
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    int rId = std::stoi(tokens[1]);
    std::string chatMsg = tokens[2];
    std::string username = RoomManager::getInstance().getUsername(clientSocket);

    // Broadcast chat với username
    std::string broadcastMsg = "CHAT|" + username + "|" + chatMsg + "\n";
    broadcastToRoom(rId, broadcastMsg);
    return "OK|CHAT_SENT";
  } else if (cmd == "LEAVE_ROOM") { // Client gửi: LEAVE_ROOM|<room_id>
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";

    int rId = std::stoi(tokens[1]);
    if (RoomManager::getInstance().leaveRoom(rId, clientSocket)) {
      std::string countMsg =
          "ROOM_MEMBER_COUNT|" + std::to_string(rId) + "|" +
          std::to_string(RoomManager::getInstance().getParticipantCount(rId)) +
          "\n";
      broadcastToRoom(rId, countMsg);
      return "OK|LEFT_ROOM";
    } else {
      return "ERR|ROOM_NOT_FOUND_OR_NOT_IN";
    }
  } else if (cmd == "VIEW_HISTORY") {
    std::string username = RoomManager::getInstance().getUsername(clientSocket);
    std::string history =
        DatabaseManager::getInstance().getHistoryList(username);
    return "OK|HISTORY|" + history;
  } else if (cmd == "CHANGE_PASS") { // CHANGE_PASS|old|new
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    std::string oldPass = tokens[1];
    std::string newPass = tokens[2];

    int userId = RoomManager::getInstance().getUserId(clientSocket);
    if (userId == -1)
      return "ERR|NOT_LOGGED_IN";

    if (DatabaseManager::getInstance().checkPassword(userId, oldPass)) {
      if (DatabaseManager::getInstance().updatePassword(userId, newPass)) {
        return "OK|PASS_CHANGED";
      } else {
        return "ERR|DB_ERROR";
      }
    } else {
      return "ERR|WRONG_PASS";
    }
  } else if (cmd == "GET_WON") {
    std::string username = RoomManager::getInstance().getUsername(clientSocket);
    if (username.empty())
      return "ERR|NOT_LOGGED_IN";

    std::string list = DatabaseManager::getInstance().getWonItems(username);
    return "OK|WON_LIST|" + list;
  } else if (cmd == "GET_HISTORY") {
    std::string username = RoomManager::getInstance().getUsername(clientSocket);
    if (username.empty())
      return "ERR|NOT_LOGGED_IN";

    std::string history =
        DatabaseManager::getInstance().getHistoryList(username);
    return "OK|HISTORY|" + history;
  } else if (cmd == "GET_PRODUCTS") { // GET_PRODUCTS|RoomID
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";

    int roomId = std::stoi(tokens[1]);
    std::string list = DatabaseManager::getInstance().getProductList(roomId);
    return "OK|PRODUCT_LIST|" + std::to_string(roomId) + "|" + list;
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
        if (response.back() != '\n') {
          response += "\n";
        }
        sendAll(clientSocket, response.c_str(), response.length());
      }
    }
  }

  // --- CLEANUP WHEN CLIENT DISCONNECTS ---
  auto affectedRooms = RoomManager::getInstance().removeClient(clientSocket);
  for (const auto &entry : affectedRooms) {
    int roomId = entry.first;
    int count = entry.second;
    std::string countMsg = "ROOM_MEMBER_COUNT|" + std::to_string(roomId) + "|" +
                           std::to_string(count) + "\n";
    broadcastToRoom(roomId, countMsg);
  }
  // --------------------------------------

  close(clientSocket);
}

void AuctionServer::broadcastToRoom(int roomId, const std::string &msg) {
  // Lấy danh sách socket từ Manager
  std::vector<SocketType> targets =
      RoomManager::getInstance().getParticipants(roomId);

  for (auto sock : targets) {
    sendAll(sock, msg.c_str(), msg.length());
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
