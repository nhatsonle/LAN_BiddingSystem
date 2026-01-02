#include "DatabaseManager.h"
#include <algorithm>
#include <sstream>

// Callback dùng để lấy userId khi login
static int loginCallback(void *data, int argc, char **argv, char **azColName) {
  int *userId = (int *)data;
  if (argc > 0 && argv[0]) {
    *userId = std::stoi(argv[0]);
  }
  return 0;
}

// Callback helper cho lịch sử
static int historyCallback(void *data, int argc, char **argv,
                           char **azColName) {
  std::string *list = (std::string *)data;

  std::string name = (argv[0] ? argv[0] : "Unknown");
  std::string price = (argv[1] ? argv[1] : "0");
  std::string winner = (argv[2] ? argv[2] : "Unknown");

  *list += name + ":" + price + ":" + winner + ";";
  return 0;
}

DatabaseManager::~DatabaseManager() {
  if (db) {
    sqlite3_close(db);
  }
}

bool DatabaseManager::init(const std::string &dbName) {
  // 1. Mở file DB
  int rc = sqlite3_open(dbName.c_str(), &db);
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  // Bật FK
  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, 0);

  char *zErrMsg = 0;

  // 2. Tạo bảng Users
  const char *sqlUsers = "CREATE TABLE IF NOT EXISTS users ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                         "username TEXT UNIQUE NOT NULL,"
                         "password TEXT NOT NULL,"
                         "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";

  rc = sqlite3_exec(db, sqlUsers, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Users): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 3. Tạo bảng Rooms
  const char *sqlRooms = "CREATE TABLE IF NOT EXISTS rooms ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                         "name TEXT NOT NULL,"
                         "status TEXT DEFAULT 'OPEN',"
                         "created_by INTEGER,"
                         "start_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
                         "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                         "closed_at DATETIME,"
                         "FOREIGN KEY(created_by) REFERENCES users(id));";
  rc = sqlite3_exec(db, sqlRooms, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Rooms): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 4. Tạo bảng Products
  const char *sqlProducts = "CREATE TABLE IF NOT EXISTS products ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "room_id INTEGER NOT NULL,"
                            "name TEXT NOT NULL,"
                            "start_price INTEGER NOT NULL,"
                            "buy_now_price INTEGER,"
                            "duration INTEGER NOT NULL,"
                            "status TEXT DEFAULT 'WAITING',"
                            "FOREIGN KEY(room_id) REFERENCES rooms(id));";
  rc = sqlite3_exec(db, sqlProducts, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Products): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 5. Tạo bảng History
  const char *sqlHistory = "CREATE TABLE IF NOT EXISTS history ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "room_id INTEGER,"
                           "product_id INTEGER,"
                           "item_name TEXT,"
                           "final_price INTEGER,"
                           "winner_user_id INTEGER,"
                           "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
                           "FOREIGN KEY(room_id) REFERENCES rooms(id),"
                           "FOREIGN KEY(product_id) REFERENCES products(id),"
                           "FOREIGN KEY(winner_user_id) REFERENCES users(id));";

  rc = sqlite3_exec(db, sqlHistory, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create History): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 6. Tạo bảng Auction Participants
  const char *sqlParticipants =
      "CREATE TABLE IF NOT EXISTS auction_participants ("
      "history_id INTEGER,"
      "user_id INTEGER,"
      "PRIMARY KEY(history_id, user_id),"
      "FOREIGN KEY(history_id) REFERENCES history(id),"
      "FOREIGN KEY(user_id) REFERENCES users(id));";

  rc = sqlite3_exec(db, sqlParticipants, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Participants): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 7. Tạo bảng Bids
  const char *sqlBids = "CREATE TABLE IF NOT EXISTS bids ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "product_id INTEGER,"
                        "user_id INTEGER,"
                        "amount INTEGER NOT NULL,"
                        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                        "FOREIGN KEY(product_id) REFERENCES products(id),"
                        "FOREIGN KEY(user_id) REFERENCES users(id));";
  rc = sqlite3_exec(db, sqlBids, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Bids): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 8. Tạo bảng Room Members (role)
  const char *sqlRoomMembers = "CREATE TABLE IF NOT EXISTS room_members ("
                               "room_id INTEGER,"
                               "user_id INTEGER,"
                               "role TEXT NOT NULL DEFAULT 'BIDDER',"
                               "joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                               "PRIMARY KEY(room_id, user_id),"
                               "FOREIGN KEY(room_id) REFERENCES rooms(id),"
                               "FOREIGN KEY(user_id) REFERENCES users(id));";
  rc = sqlite3_exec(db, sqlRoomMembers, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Room Members): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  return true;
}

bool DatabaseManager::registerUser(const std::string &username,
                                   const std::string &password) {
  std::string sql = "INSERT INTO users (username, password) VALUES ('" +
                    username + "', '" + password + "');";

  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB REGISTER ERROR]: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

bool DatabaseManager::checkLogin(const std::string &username,
                                 const std::string &password, int &outUserId) {
  std::string sql = "SELECT id FROM users WHERE username='" + username +
                    "' AND password='" + password + "';";

  outUserId = -1;
  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), loginCallback, &outUserId, &zErrMsg);

  if (rc != SQLITE_OK) {
    sqlite3_free(zErrMsg);
    return false;
  }

  return (outUserId > 0);
}

void DatabaseManager::saveAuctionResult(
    int roomId, int productId, const std::string &itemName, int finalPrice,
    int winnerUserId, const std::vector<int> &participantUserIds) {
  char *zErrMsg = 0;

  std::cout << "[DB] Saving: " << itemName << " - Price: " << finalPrice
            << std::endl;

  // 1. Insert into history
  std::string sql = "INSERT INTO history (room_id, product_id, item_name, "
                    "final_price, winner_user_id) VALUES (" +
                    std::to_string(roomId) + ", " + std::to_string(productId) +
                    ", '" + itemName + "', " + std::to_string(finalPrice) +
                    ", " + std::to_string(winnerUserId) + ");";

  int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB ERROR] SQL Error: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return;
  }

  // 2. Insert participants
  long long historyId = sqlite3_last_insert_rowid(db);

  for (const auto &uid : participantUserIds) {
    std::string sqlP =
        "INSERT OR IGNORE INTO auction_participants (history_id, user_id) "
        "VALUES (" +
        std::to_string(historyId) + ", " + std::to_string(uid) + ");";
    sqlite3_exec(db, sqlP.c_str(), 0, 0, 0);
  }

  std::cout << "[DB] Saved successfully with " << participantUserIds.size()
            << " participants." << std::endl;
}

std::string DatabaseManager::getHistoryList(const std::string &username) {
  std::string list = "";
  // JOIN to filter by username
  std::string sql =
      "SELECT h.item_name, h.final_price, COALESCE(w.username, 'Unknown') "
      "FROM history h "
      "JOIN auction_participants ap ON h.id = ap.history_id "
      "JOIN users u ON ap.user_id = u.id "
      "LEFT JOIN users w ON h.winner_user_id = w.id "
      "WHERE u.username = '" +
      username +
      "' "
      "ORDER BY h.id DESC LIMIT 10;";

  char *zErrMsg = 0;
  std::cout << "[DB] Executing History for " << username << std::endl;

  int rc = sqlite3_exec(db, sql.c_str(), historyCallback, &list, &zErrMsg);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB ERROR] Fetch History: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  return list;
}

// --- User Profile ---

bool DatabaseManager::checkPassword(int userId, const std::string &password) {
  int count = 0;
  std::string sql = "SELECT id FROM users WHERE id=" + std::to_string(userId) +
                    " AND password='" + password + "';";
  char *zErrMsg = 0;
  // create a temp callback or reuse loginCallback? loginCallback expects int*
  auto cb = [](void *data, int argc, char **argv, char **col) -> int {
    int *c = (int *)data;
    (*c)++;
    return 0;
  };
  sqlite3_exec(db, sql.c_str(), cb, &count, &zErrMsg);
  if (zErrMsg)
    sqlite3_free(zErrMsg);
  return count > 0;
}

bool DatabaseManager::updatePassword(int userId,
                                     const std::string &newPassword) {
  std::string sql = "UPDATE users SET password='" + newPassword +
                    "' WHERE id=" + std::to_string(userId) + ";";
  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    if (zErrMsg)
      sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

std::string DatabaseManager::getWonItems(const std::string &username) {
  // Format: ItemName:Price:Date;...
  std::string list = "";
  // Query bảng history, filter winner=username (thông qua JOIN users)
  // FIX: winner_user_id is int, need to join users table
  std::string sql = "SELECT h.item_name, h.final_price, h.timestamp "
                    "FROM history h "
                    "JOIN users u ON h.winner_user_id = u.id "
                    "WHERE u.username='" +
                    username + "' ORDER BY h.id DESC;";

  auto cb = [](void *data, int argc, char **argv, char **col) -> int {
    std::string *res = (std::string *)data;
    std::string item = argv[0] ? argv[0] : "";
    std::string price = argv[1] ? argv[1] : "0";
    std::string time = argv[2] ? argv[2] : "";
    *res += item + ":" + price + ":" + time + ";";
    return 0;
  };

  char *zErrMsg = 0;
  sqlite3_exec(db, sql.c_str(), cb, &list, &zErrMsg);
  if (zErrMsg)
    sqlite3_free(zErrMsg);
  return list;
}

int DatabaseManager::createRoom(const std::string &name, int createdByUserId) {
  std::string sql;
  if (createdByUserId > 0) {
    sql = "INSERT INTO rooms (name, created_by) VALUES ('" + name + "', " +
          std::to_string(createdByUserId) + ");";
  } else {
    sql = "INSERT INTO rooms (name) VALUES ('" + name + "');";
  }
  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB ERROR] Create Room: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return -1;
  }
  return (int)sqlite3_last_insert_rowid(db);
}

int DatabaseManager::saveProduct(int roomId, const std::string &name,
                                 int startPrice, int buyNowPrice,
                                 int duration) {
  int cappedDuration = std::min(duration, 1800); // tối đa 30 phút
  std::string sql = "INSERT INTO products (room_id, name, start_price, "
                    "buy_now_price, duration) VALUES (" +
                    std::to_string(roomId) + ", '" + name + "', " +
                    std::to_string(startPrice) + ", " +
                    std::to_string(buyNowPrice) + ", " +
                    std::to_string(cappedDuration) + ");";

  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "[DB ERROR] Save Product: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return -1;
  }
  return (int)sqlite3_last_insert_rowid(db);
}

void DatabaseManager::updateRoomStatus(int roomId, const std::string &status) {
  std::string sql = "UPDATE rooms SET status='" + status +
                    "' WHERE id=" + std::to_string(roomId) + ";";
  char *zErrMsg = 0;
  sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
  if (zErrMsg)
    sqlite3_free(zErrMsg);
}

void DatabaseManager::updateProductStatus(int productId,
                                          const std::string &status) {
  std::string sql = "UPDATE products SET status='" + status +
                    "' WHERE id=" + std::to_string(productId) + ";";
  char *zErrMsg = 0;
  sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
  if (zErrMsg)
    sqlite3_free(zErrMsg);
}

void DatabaseManager::addRoomMember(int roomId, int userId,
                                    const std::string &role) {
  std::string sql = "INSERT OR REPLACE INTO room_members (room_id, user_id, "
                    "role) VALUES (" +
                    std::to_string(roomId) + ", " + std::to_string(userId) +
                    ", '" + role + "');";
  char *zErrMsg = 0;
  sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
  if (zErrMsg)
    sqlite3_free(zErrMsg);
}

// Helper structs for loading
struct RoomData {
  int id;
  std::string name;
  int hostUserId;
  std::string hostName;
};
struct ProdData {
  int id;
  int roomId;
  std::string name;
  int start;
  int buyNow;
  int duration;
};

std::vector<Room> DatabaseManager::loadOpenRooms() {
  std::vector<Room> result;
  char *zErrMsg = 0;

  // 1. Load Rooms
  std::vector<RoomData> roomList;
  auto cbRoom = [](void *data, int argc, char **argv, char **col) -> int {
    auto *list = (std::vector<RoomData> *)data;
    RoomData r;
    r.id = std::stoi(argv[0]);
    r.name = (argv[1] ? argv[1] : "");
    r.hostUserId = argv[2] ? std::stoi(argv[2]) : -1;
    r.hostName = (argv[3] ? argv[3] : "N/A");
    list->push_back(r);
    return 0;
  };

  std::string sqlRooms =
      "SELECT r.id, r.name, COALESCE(r.created_by, -1), "
      "COALESCE(u.username,'N/A') "
      "FROM rooms r LEFT JOIN users u ON r.created_by = u.id "
      "WHERE r.status='OPEN';";
  sqlite3_exec(db, sqlRooms.c_str(), cbRoom, &roomList, &zErrMsg);

  if (zErrMsg) {
    std::cerr << "Load Rooms: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 2. Load Products for each room
  for (auto &rd : roomList) {
    Room room;
    room.id = rd.id;
    room.hostName = rd.hostName;
    room.hostUserId = rd.hostUserId;
    room.isClosed = false;
    room.isWaitingNextItem = false;
    room.currentProductId = -1;
    room.highestBidderSocket = -1;
    room.highestBidderUserId = -1;

    std::vector<ProdData> pList;
    auto cbProd = [](void *data, int argc, char **argv, char **col) -> int {
      auto *list = (std::vector<ProdData> *)data;
      ProdData p;
      p.id = std::stoi(argv[0]);
      p.name = (argv[1] ? argv[1] : "");
      p.start = std::stoi(argv[2]);
      p.buyNow = std::stoi(argv[3]);
      p.duration = std::stoi(argv[4]);
      list->push_back(p);
      return 0;
    };
    std::string sqlP =
        "SELECT id, name, start_price, buy_now_price, duration FROM products "
        "WHERE room_id=" +
        std::to_string(rd.id) + " AND status='WAITING' ORDER BY id ASC;";

    sqlite3_exec(db, sqlP.c_str(), cbProd, &pList, &zErrMsg);
    if (pList.empty())
      continue; // Room hết hàng hoặc lỗi -> Bỏ qua

    // Setup Active Product
    ProdData &first = pList[0];
    room.currentProductId = first.id;
    room.itemName = first.name;
    room.currentPrice = first.start;
    room.buyNowPrice = first.buyNow;
    room.initialDuration = first.duration;
    room.timeLeft = first.duration;

    // Setup Queue
    for (size_t i = 1; i < pList.size(); ++i) {
      Product p;
      p.id = pList[i].id;
      p.name = pList[i].name;
      p.startPrice = pList[i].start;
      p.buyNowPrice = pList[i].buyNow;
      p.duration = pList[i].duration;
      room.productQueue.push(p);
    }

    result.push_back(room);
  }

  return result;
}
