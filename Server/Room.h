#ifndef ROOM_H
#define ROOM_H

#include <string>
#include <vector>

// Định nghĩa kiểu Socket để tránh include lồng nhau phức tạp
#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET SocketType;
#else
    typedef int SocketType;
#endif

struct Room {
    int id;
    std::string itemName;
    int currentPrice;
    int highestBidderSocket; // Socket của người đang thắng (có thể đổi thành UserID sau này)
    std::vector<SocketType> participants; // Danh sách người đang xem
};

#endif