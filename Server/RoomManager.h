#ifndef ROOMMANAGER_H
#define ROOMMANAGER_H

#include "Room.h"
#include <vector>
#include <mutex>
#include <functional>
#include <string>

class RoomManager {
private:
    std::vector<Room> rooms;
    std::recursive_mutex roomsMutex;
    int roomIdCounter = 1;

    // Singleton: Constructor private
    RoomManager() {} 

public:
    // Singleton: Hàm lấy instance duy nhất
    static RoomManager& getInstance() {
        static RoomManager instance;
        return instance;
    }

    // Xóa copy constructor
    RoomManager(const RoomManager&) = delete;
    void operator=(const RoomManager&) = delete;

    // Các hàm nghiệp vụ
    int createRoom(std::string itemName, int startPrice, int buyNowPrice, int duration);    std::string getRoomList();
    bool joinRoom(int roomId, SocketType clientSocket, std::string& outRoomInfo);
    bool buyNow(int roomId, SocketType buyerSocket, std::string& outMsg);
    // Hàm xử lý Bid: Trả về true nếu thành công, cập nhật broadcastMsg
    bool placeBid(int roomId, int amount, SocketType bidderSocket, std::string& outBroadcastMsg);
    bool leaveRoom(int roomId, SocketType clientSocket);
    // Lấy danh sách socket trong phòng để gửi tin
    std::vector<SocketType> getParticipants(int roomId);
    // Định nghĩa kiểu hàm Callback: Nhận vào roomID và nội dung tin nhắn
    using BroadcastCallback = std::function<void(int roomId, std::string msg)>;

    // Hàm cập nhật thời gian (Được gọi mỗi giây)
    void updateTimers(BroadcastCallback callback);
    void removeClient(SocketType clientSocket);
};

#endif