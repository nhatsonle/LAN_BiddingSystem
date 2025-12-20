#ifndef ROOM_H
#define ROOM_H

#include <string>
#include <vector>


typedef int SocketType;


struct Room {
    int id;
    std::string itemName;
    int currentPrice;
    int highestBidderSocket; // Socket của người đang thắng (có thể đổi thành UserID sau này)
    std::vector<SocketType> participants; // Danh sách người đang xem
    int timeLeft; // Thời gian còn lại để đấu giá (tính bằng giây)
    bool isClosed;//Trạng thái phòng đấu giá
    int initialDuration; // Thời gian đấu giá ban đầu (tính bằng giây)
    int buyNowPrice; // Giá mua ngay
};

#endif