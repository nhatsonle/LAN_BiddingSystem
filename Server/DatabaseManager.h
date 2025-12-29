#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

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
  bool checkLogin(const std::string &username, const std::string &password);

  // Lưu lịch sử đấu giá (Thêm danh sách người tham gia)
  void saveAuctionResult(int roomId, const std::string &itemName,
                         int finalPrice, const std::string &winner,
                         const std::vector<std::string> &participants);

  // Hàm lấy chuỗi danh sách lịch sử theo user
  std::string getHistoryList(const std::string &username);

private:
  sqlite3 *db; // Con trỏ quản lý kết nối DB
  DatabaseManager() : db(nullptr) {}
  ~DatabaseManager();
};

#endif