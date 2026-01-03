#include "RoomManager.h"
#include "DatabaseManager.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

static bool parseStartTime(const std::string &input,
                           std::chrono::system_clock::time_point &out) {
  if (input.empty())
    return false;
  std::tm tm = {};
  std::istringstream ss(input);
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  if (ss.fail())
    return false;
  tm.tm_isdst = -1;
  std::time_t timeVal = std::mktime(&tm);
  if (timeVal == -1)
    return false;
  out = std::chrono::system_clock::from_time_t(timeVal);
  return true;
}

int RoomManager::createRoom(std::string roomName, std::vector<Product> products,
                            SocketType ownerSocket, int ownerUserId,
                            const std::string &startTime) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);

  if (products.empty())
    return -1;

  Room newRoom;
  newRoom.hasStartTime = false;
  newRoom.startTimeString.clear();
  newRoom.isWaitingForStart = false;
  if (!startTime.empty()) {
    std::chrono::system_clock::time_point tp;
    if (parseStartTime(startTime, tp)) {
      newRoom.hasStartTime = true;
      newRoom.startTime = tp;
      newRoom.startTimeString = startTime;
      if (!newRoom.hasStarted()) {
        newRoom.isWaitingForStart = true;
      }
    } else {
      newRoom.startTimeString = startTime;
    }
  }
  // newRoom.id = roomIdCounter++;
  // THAY ĐỔI: Gọi DB để lấy ID phòng (Persistent)
  int dbRoomId =
      DatabaseManager::getInstance().createRoom(roomName, ownerUserId);
  if (dbRoomId == -1)
    return -1;
  newRoom.id = dbRoomId;

  // --- LƯU TẤT CẢ SẢN PHẨM VÀO DB ---
  // Lưu ý: products ở đây là tham số truyền vào (chưa có ID)
  // Ta cần lưu ID lại vào vector để đẩy vào queue
  std::vector<Product> productsWithId = products;

  for (size_t i = 0; i < productsWithId.size(); ++i) {
    int pid = DatabaseManager::getInstance().saveProduct(
        newRoom.id, productsWithId[i].name, productsWithId[i].startPrice,
        productsWithId[i].buyNowPrice, productsWithId[i].duration,
        productsWithId[i].description);
    productsWithId[i].id = pid;
    // Đảm bảo duration không vượt quá 30 phút (phù hợp DB)
    productsWithId[i].duration = std::min(productsWithId[i].duration, 1800);
  }

  // --- LẤY SẢN PHẨM ĐẦU TIÊN LÀM ACTIVE ---
  Product firstP = productsWithId[0];
  DatabaseManager::getInstance().updateProductStatus(firstP.id, "ACTIVE");
  newRoom.itemName = firstP.name;
  newRoom.currentProductId = firstP.id; // <-- Lưu ID
  newRoom.currentPrice = firstP.startPrice;
  newRoom.buyNowPrice = firstP.buyNowPrice;
  int cappedFirstDuration = std::min(firstP.duration, 1800);
  newRoom.initialDuration = cappedFirstDuration;
  newRoom.timeLeft = cappedFirstDuration;
  newRoom.highestBidderSocket = -1;
  newRoom.highestBidderUserId = -1;
  newRoom.highestBidderName = "";
  if (userMap.count(ownerSocket))
    newRoom.hostName = userMap[ownerSocket].username;
  else
    newRoom.hostName = "N/A";
  newRoom.hostUserId = ownerUserId;
  newRoom.bidCount = 0;
  newRoom.isClosed = false;
  newRoom.isWaitingNextItem = false;

  // --- ĐẨY CÁC SẢN PHẨM CÒN LẠI VÀO QUEUE ---
  for (size_t i = 1; i < productsWithId.size(); ++i) {
    newRoom.productQueue.push(productsWithId[i]);
  }

  rooms.push_back(newRoom);
  return newRoom.id;
}

bool RoomManager::loadNextProduct(Room &r) {
  if (r.productQueue.empty()) {
    return false; // Hết hàng -> Đóng phòng thật
  }

  // 1. Lấy sản phẩm tiếp theo
  Product nextP = r.productQueue.front();
  r.productQueue.pop();

  // 2. Cập nhật trạng thái phòng (Reset về ban đầu)
  r.itemName = nextP.name;
  r.currentProductId = nextP.id;
  r.currentPrice = nextP.startPrice;
  r.buyNowPrice = nextP.buyNowPrice;
  int cappedDuration = std::min(nextP.duration, 1800);
  r.initialDuration = cappedDuration;
  r.timeLeft = cappedDuration; // Reset thời gian
  r.highestBidderSocket = -1;  // Reset người thắng
  r.highestBidderUserId = -1;
  r.highestBidderName = "";
  r.bidCount = 0;
  r.isWaitingNextItem = false; // <-- Đảm bảo reset trạng thái

  // Đồng bộ DB: product mới chuyển sang ACTIVE
  DatabaseManager::getInstance().updateProductStatus(r.currentProductId,
                                                     "ACTIVE");

  std::cout << "[QUEUE] Switched to next item: " << r.itemName << std::endl;
  return true;
}

bool RoomManager::buyNow(int roomId, SocketType buyerSocket,
                         std::string &outMsg, BroadcastCallback callback) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);

  for (auto &r : rooms) {
    if (r.id == roomId) {
      if (r.isClosed)
        return false; // Phòng đóng rồi thì thôi
      int buyerUserId = getUserId(buyerSocket);
      if (buyerUserId == r.hostUserId)
        return false; // chủ phòng không được mua

      // --- PHẦN 1: CHỐT ĐƠN SẢN PHẨM HIỆN TẠI ---
      r.currentPrice = r.buyNowPrice; // Giá chốt = Giá mua ngay
      r.highestBidderSocket = buyerSocket;
      r.highestBidderUserId = buyerUserId;
      r.highestBidderName = getUsername(buyerSocket);
      r.timeLeft = 0; // Dừng đồng hồ sản phẩm này

      // Update DB status của sản phẩm hiện tại
      DatabaseManager::getInstance().updateProductStatus(r.currentProductId,
                                                         "SOLD");
      if (callback) {
        callback(r.id, "PRODUCT_STATUS|" + std::to_string(r.id) + "|" +
                           std::to_string(r.currentProductId) + "|SOLD\n");
      }

      // Chuẩn bị tin nhắn SOLD để trả về ngay cho người gọi (và broadcast)
      outMsg = "SOLD|" + std::to_string(r.buyNowPrice) + "|" +
               r.highestBidderName + "\n";

      // Nếu có callback (Server gửi broadcast), gửi thông báo SOLD ngay lập tức
      if (callback) {
        callback(r.id, outMsg);
      }

      // Lưu vào Database
      std::vector<int> participantIds;
      for (const auto &p : r.participants) {
        if (p.userId > 0) {
          participantIds.push_back(p.userId);
        }
      }

      // Lưu vào Database
      DatabaseManager::getInstance().saveAuctionResult(
          r.id, r.currentProductId, r.itemName, r.currentPrice,
          r.highestBidderUserId, participantIds);
      // --- PHẦN 2: XỬ LÝ HÀNG CHỜ (QUEUE) ---
      // Lưu ý: Đừng set r.isClosed = true vội, phải check hàng đã.

      if (loadNextProduct(r)) {
        // A. CÒN HÀNG -> Chuyển sang sản phẩm tiếp theo

        // Format: NEXT_ITEM|Name|Price|BuyNow|Duration
        std::string nextMsg = "NEXT_ITEM|" + r.itemName + "|" +
                              std::to_string(r.currentPrice) + "|" +
                              std::to_string(r.buyNowPrice) + "|" +
                              std::to_string(r.initialDuration) + "\n";

        // Delay 2 giây để Client kịp hiển thị thông báo "SOLD" trước khi đổi
        // giao diện
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Gửi thông báo chuyển sản phẩm
        if (callback) {
          callback(r.id, "PRODUCT_STATUS|" + std::to_string(r.id) + "|" +
                             std::to_string(r.currentProductId) + "|ACTIVE\n");
          callback(r.id, nextMsg);
        }

        // Quan trọng: Đảm bảo phòng vẫn MỞ
        r.isClosed = false;
      } else {
        // B. HẾT HÀNG -> Đóng phòng vĩnh viễn
        r.isClosed = true;
        // UPDATE DB STATUS
        DatabaseManager::getInstance().updateRoomStatus(r.id, "CLOSED");

        if (callback)
          callback(r.id, "CLOSED|Hết hàng đấu giá\n");
      }

      return true; // <-- RETURN Ở CUỐI CÙNG mới đúng
    }
  }
  return false;
}

std::string RoomManager::getRoomList() {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  std::string list = "";

  for (const auto &r : rooms) {
    if (!r.isClosed) {
      // Format trả về: ID:TênSảnPhẩmHiệnTại:Giá:MuaNgay
      // Mỗi phòng 1 dòng, kết thúc bằng \n
      list += std::to_string(r.id) + ":" + r.itemName + ":" +
              std::to_string(r.currentPrice) + ":" +
              std::to_string(r.buyNowPrice) + ";";
    }
  }
  return list;
}

bool RoomManager::joinRoom(int roomId, SocketType clientSocket,
                           std::string &outRoomInfo) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);

  if (!userMap.count(clientSocket)) {
    std::cerr << "[WARN] Unknown user trying to join room" << std::endl;
    return false;
  }

  const auto &session = userMap[clientSocket];

  for (auto &r : rooms) {
    if (r.id == roomId) {
      // tránh nhân bản socket
      auto it = std::find_if(
          r.participants.begin(), r.participants.end(),
          [&](const Participant &p) { return p.socket == clientSocket; });
      if (it == r.participants.end()) {
        Participant p;
        p.socket = clientSocket;
        p.userId = session.userId;
        p.username = session.username;
        p.role = (session.userId == r.hostUserId) ? "HOST" : "BIDDER";
        r.participants.push_back(p);

        DatabaseManager::getInstance().addRoomMember(roomId, session.userId,
                                                     p.role);
      }

      std::string participantNames;
      int participantCount = static_cast<int>(r.participants.size());

      std::string startField = r.startTimeString;
      std::string startedFlag = r.hasStarted() ? "1" : "0";
      outRoomInfo = std::to_string(r.id) + "|" + r.itemName + "|" +
                    std::to_string(r.currentPrice) + "|" +
                    std::to_string(r.buyNowPrice) + "|" + r.hostName + "|" +
                    r.highestBidderName + "|" + std::to_string(r.bidCount) +
                    "|" + std::to_string(participantCount) + "|" + startField +
                    "|" + startedFlag;
      // -------------------------

      return true;
    }
  }
  return false;
}

bool RoomManager::isUserLoggedIn(const std::string &username) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  for (const auto &pair : userMap) {
    if (pair.second.username == username) {
      return true;
    }
  }
  return false;
}

bool RoomManager::placeBid(int roomId, int amount, SocketType bidderSocket,
                           std::string &outBroadcastMsg,
                           std::string &outError) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  outError.clear();
  for (auto &r : rooms) {
    if (r.id == roomId) {
      // Kiểm tra: Nếu phòng đã đóng thì không cho bid
      if (r.isClosed)
        return false;
      int bidderUserId = getUserId(bidderSocket);
      if (bidderUserId == r.hostUserId)
        return false; // chủ phòng không được bid
      if (!r.hasStarted()) {
        outError = "ERR|ROOM_NOT_STARTED";
        return false;
      }
      // Nếu giá bid chạm/qua giá mua ngay -> xử lý mua ngay
      if (amount >= r.buyNowPrice) {
        std::string bidderName = getUsername(bidderSocket);
        int soldProductId = r.currentProductId;
        r.currentPrice = r.buyNowPrice;
        r.highestBidderSocket = bidderSocket;
        r.highestBidderUserId = bidderUserId;
        r.highestBidderName = bidderName;
        r.bidCount += 1;
        r.timeLeft = 0;

        // Update DB status của sản phẩm vừa chốt
        DatabaseManager::getInstance().updateProductStatus(soldProductId,
                                                           "SOLD");

        // Lưu DB
        std::vector<int> participantIds;
        for (const auto &p : r.participants) {
          if (p.userId > 0)
            participantIds.push_back(p.userId);
        }
        DatabaseManager::getInstance().saveAuctionResult(
            r.id, r.currentProductId, r.itemName, r.currentPrice,
            r.highestBidderUserId, participantIds);

        std::string soldMsg =
            "SOLD|" + std::to_string(r.currentPrice) + "|" + bidderName + "\n";
        std::string soldStatusMsg = "PRODUCT_STATUS|" + std::to_string(roomId) +
                                    "|" + std::to_string(soldProductId) +
                                    "|SOLD\n";

        if (loadNextProduct(r)) {
          std::string nextMsg = "NEXT_ITEM|" + r.itemName + "|" +
                                std::to_string(r.currentPrice) + "|" +
                                std::to_string(r.buyNowPrice) + "|" +
                                std::to_string(r.initialDuration) + "\n";
          std::string activeStatusMsg =
              "PRODUCT_STATUS|" + std::to_string(roomId) + "|" +
              std::to_string(r.currentProductId) + "|ACTIVE\n";
          outBroadcastMsg = soldStatusMsg + soldMsg + activeStatusMsg + nextMsg;
          r.isClosed = false;
        } else {
          r.isClosed = true;
          DatabaseManager::getInstance().updateRoomStatus(r.id, "CLOSED");
          outBroadcastMsg =
              soldStatusMsg + soldMsg + "CLOSED|Hết hàng đấu giá\n";
        }
        return true;
      }
      // Yêu cầu: Giá mới phải cao hơn hoặc bằng giá cũ + 10.000
      if (amount >= r.currentPrice + 10000) {
        std::string bidderName = getUsername(bidderSocket);
        r.currentPrice = amount;
        r.highestBidderSocket = bidderSocket;
        r.highestBidderUserId = bidderUserId;
        r.highestBidderName = bidderName;
        r.bidCount += 1;

        // --- LUẬT 30 GIÂY ---
        // Nếu còn dưới 30s mà có người Bid -> Reset về 30s
        if (r.timeLeft < 30) {
          r.timeLeft = 30;
        }
        // --------------------

        outBroadcastMsg = "NEW_BID|" + std::to_string(amount) + "|" +
                          bidderName + "|" + std::to_string(r.bidCount) + "\n";
        return true;
      }
      outError = "ERR|PRICE_TOO_LOW";
      return false;
    }
  }
  return false;
}

bool RoomManager::isRoomStarted(int roomId) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  for (const auto &r : rooms) {
    if (r.id == roomId) {
      return r.hasStarted();
    }
  }
  return false;
}

std::vector<SocketType> RoomManager::getParticipants(int roomId) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  for (const auto &r : rooms) {
    if (r.id == roomId) {
      std::vector<SocketType> sockets;
      for (const auto &p : r.participants) {
        sockets.push_back(p.socket);
      }
      return sockets;
    }
  }
  return {};
}

int RoomManager::getParticipantCount(int roomId) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  for (const auto &r : rooms) {
    if (r.id == roomId) {
      return static_cast<int>(r.participants.size());
    }
  }
  return 0;
}

std::vector<std::pair<int, int>> RoomManager::removeClient(SocketType clientSocket) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  std::vector<std::pair<int, int>> affectedRooms;

  // Xóa khỏi userMap
  if (userMap.count(clientSocket)) {
    std::cout << "[INFO] User " << userMap[clientSocket].username
              << " disconnected." << std::endl;
    userMap.erase(clientSocket);
  }

  for (auto &room : rooms) {
    auto it = std::remove_if(
        room.participants.begin(), room.participants.end(),
        [&](const Participant &p) { return p.socket == clientSocket; });

    if (it != room.participants.end()) {
      room.participants.erase(it, room.participants.end());
      affectedRooms.push_back({room.id, static_cast<int>(room.participants.size())});
      std::cout << "[INFO] Removed client " << clientSocket << " from Room "
                << room.id << std::endl;
    }
  }

  return affectedRooms;
}

void RoomManager::loginUser(SocketType sock, int userId, std::string name) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  userMap[sock] = {userId, name};
}

std::string RoomManager::getUsername(SocketType sock) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  if (userMap.count(sock))
    return userMap[sock].username;
  return "Unknown";
}

int RoomManager::getUserId(SocketType sock) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  if (userMap.count(sock))
    return userMap[sock].userId;
  return -1;
}

bool RoomManager::leaveRoom(int roomId, SocketType clientSocket) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);

  for (auto &r : rooms) {
    if (r.id == roomId) {
      auto it = std::remove_if(
          r.participants.begin(), r.participants.end(),
          [&](const Participant &p) { return p.socket == clientSocket; });

      if (it != r.participants.end()) {
        r.participants.erase(it, r.participants.end());
        std::cout << "[INFO] Client " << clientSocket << " left Room " << roomId
                  << std::endl;

        // Nếu phòng trống -> Timer sẽ tự động dừng ở lần updateTimers tiếp theo
        return true;
      }
    }
  }
  return false;
}

void RoomManager::updateTimers(BroadcastCallback callback) {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);

  for (auto &r : rooms) {
    if (r.isClosed)
      continue;
    if (r.participants.empty())
      continue;
    if (r.isWaitingForStart) {
      if (!r.hasStarted())
        continue;
      r.isWaitingForStart = false;
      std::string startMsg =
          "ROOM_STATUS|" + std::to_string(r.id) + "|STARTED";
      if (!r.startTimeString.empty())
        startMsg += "|" + r.startTimeString;
      startMsg += "\n";
      callback(r.id, startMsg);
    }

    r.timeLeft--;

    // --- TRẠNG THÁI CHỜ (GIỮA 2 SẢN PHẨM) ---
    if (r.isWaitingNextItem) {
      if (r.timeLeft <= 0) {
        // Hết 3s chờ -> Chuyển sản phẩm
        if (loadNextProduct(r)) {
          callback(r.id, "PRODUCT_STATUS|" + std::to_string(r.id) + "|" +
                             std::to_string(r.currentProductId) + "|ACTIVE\n");
          // Format: NEXT_ITEM|Name|Price|BuyNow|Duration
          std::string nextMsg = "NEXT_ITEM|" + r.itemName + "|" +
                                std::to_string(r.currentPrice) + "|" +
                                std::to_string(r.buyNowPrice) + "|" +
                                std::to_string(r.initialDuration) + "\n";
          callback(r.id, nextMsg);
        } else {
          // Hết hàng -> Đóng
          r.isClosed = true;
          DatabaseManager::getInstance().updateRoomStatus(r.id, "CLOSED");
          callback(r.id, "CLOSED|Hết hàng đấu giá\n");
        }
        // Đã chuyển xong (hoặc đóng), tắt trạng thái chờ
        r.isWaitingNextItem = false;
      }
      continue; // Không xử lý logic đếm ngược bình thường
    }

    // --- TRẠNG THÁI ĐẤU GIÁ BÌNH THƯỜNG ---
    if (r.timeLeft > 0) {
      std::string msg = "TIME_UPDATE|" + std::to_string(r.timeLeft) + "\n";
      callback(r.id, msg);
      if (r.timeLeft == 30) {
        std::string alert = "TIME_ALERT|30\n";
        callback(r.id, alert);
      }
    } else {
      // --- HẾT GIỜ (TIMEOUT) ---
      // 1. Lưu kết quả
      if (r.highestBidderSocket != -1) {
        DatabaseManager::getInstance().updateProductStatus(r.currentProductId,
                                                           "SOLD");
        callback(r.id, "PRODUCT_STATUS|" + std::to_string(r.id) + "|" +
                           std::to_string(r.currentProductId) + "|SOLD\n");

        std::vector<int> participantIds;
        for (const auto &p : r.participants) {
          if (p.userId > 0)
            participantIds.push_back(p.userId);
        }

        if (r.highestBidderName.empty())
          r.highestBidderName = getUsername(r.highestBidderSocket);
        DatabaseManager::getInstance().saveAuctionResult(
            r.id, r.currentProductId, r.itemName, r.currentPrice,
            r.highestBidderUserId, participantIds);

        std::string winnerName = r.highestBidderName;
        std::string soldMsg = "SOLD_ITEM|" + r.itemName + "|" +
                              std::to_string(r.currentPrice) + "|" +
                              winnerName + "\n";
        callback(r.id, soldMsg);
        std::cout << "winner name: " << winnerName << std::endl;
      } else {
        DatabaseManager::getInstance().updateProductStatus(r.currentProductId,
                                                           "NO_SALE");
        callback(r.id, "PRODUCT_STATUS|" + std::to_string(r.id) + "|" +
                           std::to_string(r.currentProductId) + "|NO_SALE\n");
        callback(r.id, "Pass_Item|Không ai mua " + r.itemName + "\n");
      }

      // 2. CHUYỂN SANG TRẠNG THÁI CHỜ (Thay vì sleep)
      r.isWaitingNextItem = true;
      r.timeLeft = 3; // Chờ 3 giây
    }
  }
}

void RoomManager::loadState() {
  std::lock_guard<std::recursive_mutex> lock(roomsMutex);
  rooms = DatabaseManager::getInstance().loadOpenRooms();

  // Update Max ID để tránh trùng (Room ID tự tăng trong DB rồi, nhưng biến
  // counter...)
  if (!rooms.empty()) {
    int maxId = 0;
    for (auto &r : rooms) {
      if (r.id > maxId)
        maxId = r.id;
      r.highestBidderUserId = -1;
      std::cout << "[RECOVERY] Loaded Room " << r.id
                << " - Item: " << r.itemName << std::endl;
    }
    roomIdCounter = maxId + 1;
  } else {
    std::cout << "[RECOVERY] No open rooms found." << std::endl;
  }
}
