#include "DatabaseManager.h"

// Callback dùng để check dữ liệu trả về (dùng cho hàm checkLogin)
// Trả về 0 nếu thành công
static int loginCallback(void* data, int argc, char** argv, char** azColName) {
    int* count = (int*)data;
    *count = 1; // Nếu callback này được gọi nghĩa là có tìm thấy dòng dữ liệu
    return 0;
}

DatabaseManager::~DatabaseManager() {
    if (db) {
        sqlite3_close(db);
    }
}

bool DatabaseManager::init(const std::string& dbName) {
    // 1. Mở file DB
    int rc = sqlite3_open(dbName.c_str(), &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 2. Tạo bảng Users
    const char* sqlUsers = "CREATE TABLE IF NOT EXISTS users (" \
                           "username TEXT PRIMARY KEY NOT NULL," \
                           "password TEXT NOT NULL);";
    
    char* zErrMsg = 0;
    rc = sqlite3_exec(db, sqlUsers, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (Create Users): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    }

    // 3. Tạo bảng History
    const char* sqlHistory = "CREATE TABLE IF NOT EXISTS history (" \
                             "id INTEGER PRIMARY KEY AUTOINCREMENT," \
                             "room_id INTEGER," \
                             "item_name TEXT," \
                             "final_price INTEGER," \
                             "winner TEXT," \
                             "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    
    rc = sqlite3_exec(db, sqlHistory, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (Create History): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    }
    
    return true;
}

bool DatabaseManager::registerUser(const std::string& username, const std::string& password) {
    std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password + "');";
    
    char* zErrMsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Register Error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false; // Có thể do trùng username
    }
    return true;
}

bool DatabaseManager::checkLogin(const std::string& username, const std::string& password) {
    std::string sql = "SELECT username FROM users WHERE username='" + username + "' AND password='" + password + "';";
    
    int count = 0;
    char* zErrMsg = 0;
    // Hàm exec sẽ gọi 'loginCallback' cho mỗi dòng tìm thấy
    int rc = sqlite3_exec(db, sql.c_str(), loginCallback, &count, &zErrMsg);

    if (rc != SQLITE_OK) {
        sqlite3_free(zErrMsg);
        return false;
    }
    
    return (count > 0); // Nếu count = 1 nghĩa là login đúng
}

void DatabaseManager::saveAuctionResult(int roomId, const std::string& itemName, int finalPrice, const std::string& winner) {
    // Xây dựng câu lệnh SQL
    std::string sql = "INSERT INTO history (room_id, item_name, final_price, winner) VALUES (" + 
                      std::to_string(roomId) + ", '" + 
                      itemName + "', " + 
                      std::to_string(finalPrice) + ", '" + 
                      winner + "');";

    char* zErrMsg = 0;
    sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
}