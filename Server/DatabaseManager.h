#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "Room.h"
#include "sqlite3.h" // Include thư viện vừa tải
#include <iostream>
#include <string>
#include <vector>

class DatabaseManager {
public:
  static DatabaseManager &getInstance() {
    static DatabaseManager instance;
    return instance;
  }

  // Khởi tạo và tạo bảng
  bool init(const std::string &dbName);

  // Xử lý User
  bool registerUser(const std::string &username, const std::string &password);
  bool checkLogin(const std::string &username, const std::string &password,
                  int &outUserId);

  // Lưu lịch sử đấu giá (Thêm danh sách người tham gia)
  void saveAuctionResult(int roomId, int productId, const std::string &itemName,
                         int finalPrice, int winnerUserId,
                         const std::vector<int> &participantUserIds);

  // Hàm lấy chuỗi danh sách lịch sử theo user
  std::string getHistoryList(const std::string &username);

  // --- User Profile ---
  bool checkPassword(int userId, const std::string &password);
  bool updatePassword(int userId, const std::string &newPassword);
  std::string getWonItems(const std::string &username);

  // --- Room & Product Management ---
  // --- Room & Product Management ---
  int createRoom(const std::string &name, int createdByUserId = -1);
  int saveProduct(int roomId, const std::string &name, int startPrice,
                  int buyNowPrice, int duration, const std::string &description);
  void updateRoomStatus(int roomId, const std::string &status);
  void updateProductStatus(int productId, const std::string &status);
  void addRoomMember(int roomId, int userId,
                     const std::string &role = "BIDDER");

  // Product list for UI
  std::string getProductList(int roomId);

  // Recovery
  std::vector<Room> loadOpenRooms();

private:
  sqlite3 *db; // Con trỏ quản lý kết nối DB
  DatabaseManager() : db(nullptr) {}
  ~DatabaseManager();
};

#endif
