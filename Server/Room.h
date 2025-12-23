#ifndef ROOM_H
#define ROOM_H

#include <string>
#include <vector>
#include <queue>

typedef int SocketType;
// 1. Định nghĩa cấu trúc Sản phẩm
struct Product {
    std::string name;
    int startPrice;
    int buyNowPrice;
    int duration;
};

// 2. Cập nhật cấu trúc Room
struct Room {
    int id;
    
    // --- THÔNG TIN SẢN PHẨM ĐANG ĐẤU GIÁ (ACTIVE) ---
    std::string itemName;
    int currentPrice;
    int buyNowPrice;
    int highestBidderSocket;
    int timeLeft;
    int initialDuration;
    // ------------------------------------------------
    
    bool isClosed;
    std::vector<SocketType>participants; // Dùng int thay vì SocketType cho đơn giản hóa struct
    
    // --- THÊM HÀNG CHỜ ---
    std::queue<Product> productQueue; // Hàng đợi các sản phẩm tiếp theo
    // ---------------------
};

#endif