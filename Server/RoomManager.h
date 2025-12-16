#ifndef ROOMMANAGER_H
#define ROOMMANAGER_H

#include "Room.h"
#include <vector>
#include <mutex>
#include <string>

class RoomManager {
private:
    std::vector<Room> rooms;
    std::mutex roomsMutex;
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
    int createRoom(std::string itemName, int startPrice);
    std::string getRoomList();
    bool joinRoom(int roomId, SocketType clientSocket, std::string& outRoomInfo);
    
    // Hàm xử lý Bid: Trả về true nếu thành công, cập nhật broadcastMsg
    bool placeBid(int roomId, int amount, SocketType bidderSocket, std::string& outBroadcastMsg);

    // Lấy danh sách socket trong phòng để gửi tin
    std::vector<SocketType> getParticipants(int roomId);
};

#endif