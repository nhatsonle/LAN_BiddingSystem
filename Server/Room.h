#ifndef ROOM_H
#define ROOM_H

#include <chrono>
#include <queue>
#include <string>
#include <vector>

typedef int SocketType;
// 1. Định nghĩa cấu trúc Sản phẩm
struct Product {
  int id; // <-- ID tu Database
  std::string name;
  std::string description;
  int startPrice;
  int buyNowPrice;
  int duration;
};

struct Participant {
  SocketType socket;
  int userId;
  std::string username;
  std::string role;
};

// 2. Cập nhật cấu trúc Room
struct Room {
  int id;

  // --- THÔNG TIN SẢN PHẨM ĐANG ĐẤU GIÁ (ACTIVE) ---
  int currentProductId; // <-- ID san pham hien tai
  std::string itemName;
  int currentPrice;
  int buyNowPrice;
  int highestBidderSocket;
  int highestBidderUserId;
  std::string highestBidderName;
  std::string hostName;
  int bidCount = 0;
  int hostUserId;
  int timeLeft;
  int initialDuration;
  bool hasStartTime = false;
  std::chrono::system_clock::time_point startTime;
  std::string startTimeString;
  bool isWaitingForStart = false;
  // ------------------------------------------------

  // ------------------------------------------------

  bool isClosed;
  bool isWaitingNextItem; // Trạng thái chờ giữa 2 sản phẩm
  std::vector<Participant> participants; // Danh sách tham gia kèm role

  // --- THÊM HÀNG CHỜ ---
  std::queue<Product> productQueue; // Hàng đợi các sản phẩm tiếp theo
                                    // ---------------------

  bool hasStarted() const {
    if (!hasStartTime)
      return true;
    return std::chrono::system_clock::now() >= startTime;
  }
};

#endif
