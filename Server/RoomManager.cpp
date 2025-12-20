#include "RoomManager.h"
#include <iostream>

int RoomManager::createRoom(std::string itemName, int startPrice) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    Room newRoom;
    newRoom.id = roomIdCounter++;
    newRoom.itemName = itemName;
    newRoom.currentPrice = startPrice;
    newRoom.highestBidderSocket = -1;

    // --- KHỞI TẠO TIMER ---
    newRoom.initialDuration = 30; // Mặc định 5 phút (300 giây)
    newRoom.timeLeft = 30;
    newRoom.isClosed = false;
    // ---------------------

    rooms.push_back(newRoom);
    return newRoom.id;
}

std::string RoomManager::getRoomList() {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    // Debug: In ra xem server có bao nhiêu phòng ?
    std::cout << "[DEBUG] Current rooms count: " << rooms.size() << std::endl;
    if (rooms.empty()) return "";
    
    std::string listStr = "";
    for (const auto& r : rooms) {
        listStr += std::to_string(r.id) + ":" + r.itemName + ":" + std::to_string(r.currentPrice) + ";";
    }
    return listStr;
}

bool RoomManager::joinRoom(int roomId, SocketType clientSocket, std::string& outRoomInfo) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
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
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    for (auto& r : rooms) {
        if (r.id == roomId) {
            // Kiểm tra: Nếu phòng đã đóng thì không cho bid
            if (r.isClosed) return false; 

            if (amount > r.currentPrice) {
                r.currentPrice = amount;
                r.highestBidderSocket = bidderSocket;
                
                // --- LUẬT 30 GIÂY ---
                // Nếu còn dưới 30s mà có người Bid -> Reset về 30s
                if (r.timeLeft < 30) {
                    r.timeLeft = 30; 
                }
                // --------------------

                outBroadcastMsg = "NEW_BID|" + std::to_string(amount) + "|" + std::to_string(bidderSocket) + "\n";
                return true;
            }
            return false;
        }
    }
    return false;
}

std::vector<SocketType> RoomManager::getParticipants(int roomId) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    for (const auto& r : rooms) {
        if (r.id == roomId) {
            return r.participants;
        }
    }
    return {};
}

void RoomManager::updateTimers(BroadcastCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex); // Khóa lại để duyệt an toàn
    
    for (auto& r : rooms) {
        if (r.isClosed) continue; // Phòng đóng rồi thì bỏ qua

        r.timeLeft--; // Trừ 1 giây

        if (r.timeLeft > 0) {
            // Gửi cập nhật thời gian mỗi giây (hoặc tối ưu: chỉ gửi khi % 5 == 0)
            // Format: TIME_UPDATE|<time_left>
            std::string msg = "TIME_UPDATE|" + std::to_string(r.timeLeft) + "\n";
            callback(r.id, msg);
        } 
        else {
            // --- HẾT GIỜ (XỬ LÝ THẮNG CUỘC) ---
            r.isClosed = true;
            r.timeLeft = 0;
            
            std::string msg;
            if (r.highestBidderSocket != -1) {
                // Có người mua
                msg = "SOLD|" + std::to_string(r.currentPrice) + "|" + std::to_string(r.highestBidderSocket) + "\n";
            } else {
                // Không ai mua
                msg = "CLOSED|NO_BID\n";
            }
            
            callback(r.id, msg);
        }
    }
}