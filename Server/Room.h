#ifndef ROOM_H
#define ROOM_H

#include <queue>
#include <string>
#include <vector>

typedef int SocketType;
// 1. Định nghĩa cấu trúc Sản phẩm
struct Product {
  int id; // <-- ID tu Database
  std::string name;
  int startPrice;
  int buyNowPrice;
  int duration;
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
  int timeLeft;
  int initialDuration;
  // ------------------------------------------------

  // ------------------------------------------------

  bool isClosed;
  bool isWaitingNextItem; // <-- Thêm biến trạng thái chờ
  std::vector<SocketType>
      participants; // sockets hiện đang trong phòng

  // Lưu username đã từng tham gia/bid (không xoá khi rời phòng)
  std::vector<std::string> participantUsernames;

  // --- THÊM HÀNG CHỜ ---
  std::queue<Product> productQueue; // Hàng đợi các sản phẩm tiếp theo
                                    // ---------------------
};

#endif