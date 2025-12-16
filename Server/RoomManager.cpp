#include "RoomManager.h"
#include <iostream>

int RoomManager::createRoom(std::string itemName, int startPrice) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    Room newRoom;
    newRoom.id = roomIdCounter++;
    newRoom.itemName = itemName;
    newRoom.currentPrice = startPrice;
    newRoom.highestBidderSocket = -1;
    rooms.push_back(newRoom);
    return newRoom.id;
}

std::string RoomManager::getRoomList() {
    std::lock_guard<std::mutex> lock(roomsMutex);
    if (rooms.empty()) return "";
    
    std::string listStr = "";
    for (const auto& r : rooms) {
        listStr += std::to_string(r.id) + ":" + r.itemName + ":" + std::to_string(r.currentPrice) + ";";
    }
    return listStr;
}

bool RoomManager::joinRoom(int roomId, SocketType clientSocket, std::string& outRoomInfo) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    for (auto& r : rooms) {
        if (r.id == roomId) {
            r.participants.push_back(clientSocket);
            // Tạo chuỗi thông tin trả về: ID|Name|Price
            outRoomInfo = std::to_string(r.id) + "|" + r.itemName + "|" + std::to_string(r.currentPrice);
            return true;
        }
    }
    return false;
}

bool RoomManager::placeBid(int roomId, int amount, SocketType bidderSocket, std::string& outBroadcastMsg) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    for (auto& r : rooms) {
        if (r.id == roomId) {
            if (amount > r.currentPrice) {
                r.currentPrice = amount;
                r.highestBidderSocket = bidderSocket;
                
                // Chuẩn bị tin nhắn broadcast
                outBroadcastMsg = "NEW_BID|" + std::to_string(amount) + "|" + std::to_string(bidderSocket) + "\n";
                return true;
            }
            return false; // Giá thấp hơn
        }
    }
    return false; // Không tìm thấy phòng
}

std::vector<SocketType> RoomManager::getParticipants(int roomId) {
    std::lock_guard<std::mutex> lock(roomsMutex);
    for (const auto& r : rooms) {
        if (r.id == roomId) {
            return r.participants;
        }
    }
    return {};
}