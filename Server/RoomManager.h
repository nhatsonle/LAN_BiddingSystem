#ifndef ROOMMANAGER_H
#define ROOMMANAGER_H

#include "Room.h"     // Chứa struct Room và Product
#include <functional> // <--- BẮT BUỘC PHẢI CÓ THƯ VIỆN NÀY
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

// --- 1. ĐỊNH NGHĨA CALLBACK & SOCKET ---
// Định nghĩa SocketType là int
using SocketType = int;

// Định nghĩa kiểu hàm Callback: Trả về void, nhận vào (int roomId, string
// message)
using BroadcastCallback = std::function<void(int, std::string)>;

class RoomManager {
private:
  std::vector<Room> rooms;
  std::recursive_mutex roomsMutex;
  int roomIdCounter = 1;
  bool loadNextProduct(Room &r);
  // Singleton: Constructor private
  RoomManager() {}

public:
  // Singleton: Hàm lấy instance duy nhất
  static RoomManager &getInstance() {
    static RoomManager instance;
    return instance;
  }
  std::string getRoomList();

  // Xóa copy constructor
  RoomManager(const RoomManager &) = delete;
  void operator=(const RoomManager &) = delete;

  // Các hàm nghiệp vụ
  int createRoom(std::string roomName, std::vector<Product> products);
  bool joinRoom(int roomId, SocketType clientSocket, std::string &outRoomInfo);
  bool buyNow(int roomId, SocketType buyerSocket, std::string &outMsg,
              BroadcastCallback callback); // Hàm xử lý Bid: Trả về true nếu
                                           // thành công, cập nhật broadcastMsg
  bool placeBid(int roomId, int amount, SocketType bidderSocket,
                std::string &outBroadcastMsg);
  bool leaveRoom(int roomId, SocketType clientSocket);
  // Lấy danh sách socket trong phòng để gửi tin
  std::vector<SocketType> getParticipants(int roomId);

  // Lấy tên sản phẩm tiếp theo trong hàng chờ (để hiển thị)
  std::string getNextItemName(int roomId);
  // Định nghĩa kiểu hàm Callback: Nhận vào roomID và nội dung tin nhắn
  using BroadcastCallback = std::function<void(int roomId, std::string msg)>;

  // Hàm cập nhật thời gian (Được gọi mỗi giây)
  void updateTimers(BroadcastCallback callback);
  void removeClient(SocketType clientSocket);

  // Quản lý User Login
  void loginUser(SocketType sock, std::string name);
  std::string getUsername(SocketType sock);

  // Recovery
  void loadState();

private:
  std::map<SocketType, std::string> userMap;
};

#endif