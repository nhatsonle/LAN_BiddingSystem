#include "RoomManager.h"
#include <iostream>

int RoomManager::createRoom(std::string itemName, int startPrice, int buyNowPrice) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    Room newRoom;
    newRoom.id = roomIdCounter++;
    newRoom.itemName = itemName;
    newRoom.currentPrice = startPrice;
    newRoom.highestBidderSocket = -1;
    newRoom.buyNowPrice = buyNowPrice;

    // --- KHỞI TẠO TIMER ---
    newRoom.initialDuration = 30; // Mặc định 5 phút (300 giây)
    newRoom.timeLeft = 30;
    newRoom.isClosed = false;
    // ---------------------

    rooms.push_back(newRoom);
    return newRoom.id;
}

bool RoomManager::buyNow(int roomId, SocketType buyerSocket, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    
    for (auto& r : rooms) {
        if (r.id == roomId) {
            if (r.isClosed) return false; // Phòng đóng rồi thì thôi

            // 1. Cập nhật trạng thái thắng cuộc
            r.isClosed = true;          // Đóng phòng ngay lập tức
            r.timeLeft = 0;             // Thời gian về 0
            r.currentPrice = r.buyNowPrice; // Giá chốt = Giá mua ngay
            r.highestBidderSocket = buyerSocket;
            
            // 2. Chuẩn bị tin nhắn SOLD (Tái sử dụng logic của Timer)
            // Format: SOLD|<price>|<winner_id>
            outMsg = "SOLD|" + std::to_string(r.buyNowPrice) + "|" + std::to_string(buyerSocket) + "\n";
            
            return true;
        }
    }
    return false;
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

void RoomManager::removeClient(SocketType clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    
    for (auto& room : rooms) {
        // Duyệt tìm và xóa socket khỏi vector participants
        auto it = std::remove(room.participants.begin(), room.participants.end(), clientSocket);
        
        if (it != room.participants.end()) {
            room.participants.erase(it, room.participants.end());
            std::cout << "[INFO] Removed client " << clientSocket << " from Room " << room.id << std::endl;
        }
    }
}

bool RoomManager::leaveRoom(int roomId, SocketType clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex);
    
    for (auto& r : rooms) {
        if (r.id == roomId) {
            // Tìm và xóa socket khỏi danh sách participants
            auto it = std::remove(r.participants.begin(), r.participants.end(), clientSocket);
            
            if (it != r.participants.end()) {
                r.participants.erase(it, r.participants.end());
                std::cout << "[INFO] Client " << clientSocket << " left Room " << roomId << std::endl;
                
                // Nếu phòng trống -> Timer sẽ tự động dừng ở lần updateTimers tiếp theo
                return true;
            }
        }
    }
    return false;
}

void RoomManager::updateTimers(BroadcastCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex); // Khóa lại để duyệt an toàn
    
    for (auto& r : rooms) {
        if (r.isClosed) continue; // Phòng đóng rồi thì bỏ qua

        //KIỂM TRA NGƯỜI DÙNG ---
        if (r.participants.empty()) {
            // Phòng trống -> Không trừ giờ -> Timer đứng yên
            // (Tùy chọn: Có thể reset lại giờ về ban đầu nếu muốn: r.timeLeft = r.initialDuration;)
            continue; 
        }
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