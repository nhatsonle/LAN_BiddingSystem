#include "DatabaseManager.h"

// Callback dùng để check dữ liệu trả về (dùng cho hàm checkLogin)
// Trả về 0 nếu thành công
static int loginCallback(void *data, int argc, char **argv, char **azColName) {
  int *count = (int *)data;
  *count = 1; // Nếu callback này được gọi nghĩa là có tìm thấy dòng dữ liệu
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

  // 2. Tạo bảng Users
  const char *sqlUsers = "CREATE TABLE IF NOT EXISTS users ("
                         "username TEXT PRIMARY KEY NOT NULL,"
                         "password TEXT NOT NULL);";

  char *zErrMsg = 0;
  rc = sqlite3_exec(db, sqlUsers, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Users): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 3. Tạo bảng History
  const char *sqlHistory = "CREATE TABLE IF NOT EXISTS history ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "room_id INTEGER,"
                           "item_name TEXT,"
                           "final_price INTEGER,"
                           "winner TEXT,"
                           "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

  rc = sqlite3_exec(db, sqlHistory, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create History): " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  }

  // 4. Tạo bảng Auction Participants
  const char *sqlParticipants =
      "CREATE TABLE IF NOT EXISTS auction_participants ("
      "history_id INTEGER,"
      "username TEXT,"
      "FOREIGN KEY(history_id) REFERENCES history(id));";

  rc = sqlite3_exec(db, sqlParticipants, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error (Create Participants): " << zErrMsg << std::endl;
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
                                 const std::string &password) {
  std::string sql = "SELECT username FROM users WHERE username='" + username +
                    "' AND password='" + password + "';";

  int count = 0;
  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), loginCallback, &count, &zErrMsg);

  if (rc != SQLITE_OK) {
    sqlite3_free(zErrMsg);
    return false;
  }

  return (count > 0);
}

void DatabaseManager::saveAuctionResult(
    int roomId, const std::string &itemName, int finalPrice,
    const std::string &winner, const std::vector<std::string> &participants) {
  char *zErrMsg = 0;

  std::cout << "[DB] Saving: " << itemName << " - Price: " << finalPrice
            << std::endl;

  // 1. Insert into history
  std::string sql =
      "INSERT INTO history (room_id, item_name, final_price, winner) VALUES (" +
      std::to_string(roomId) + ", '" + itemName + "', " +
      std::to_string(finalPrice) + ", '" + winner + "');";

  int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB ERROR] SQL Error: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return;
  }

  // 2. Insert participants
  long long historyId = sqlite3_last_insert_rowid(db);

  for (const auto &user : participants) {
    std::string sqlP =
        "INSERT INTO auction_participants (history_id, username) VALUES (" +
        std::to_string(historyId) + ", '" + user + "');";
    sqlite3_exec(db, sqlP.c_str(), 0, 0, 0);
  }

  std::cout << "[DB] Saved successfully with " << participants.size()
            << " participants." << std::endl;
}

// Callback helper
static int historyCallback(void *data, int argc, char **argv,
                           char **azColName) {
  std::string *list = (std::string *)data;

  std::string name = (argv[0] ? argv[0] : "Unknown");
  std::string price = (argv[1] ? argv[1] : "0");
  std::string winner = (argv[2] ? argv[2] : "Unknown");

  *list += name + ":" + price + ":" + winner + ";";
  return 0;
}

std::string DatabaseManager::getHistoryList(const std::string &username) {
  std::string list = "";
  // JOIN to filter by username
  std::string sql = "SELECT h.item_name, h.final_price, h.winner "
                    "FROM history h "
                    "JOIN auction_participants ap ON h.id = ap.history_id "
                    "WHERE ap.username = '" +
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