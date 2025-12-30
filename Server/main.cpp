#include "AuctionServer.h"
#include "DatabaseManager.h"

int main() {
  // Khởi tạo DB trước khi bật Server
  const std::string dbFile = "auction_system.db";
  if (!DatabaseManager::getInstance().init(dbFile)) {
    std::cerr << "Failed to init DB" << std::endl;
    return -1;
  }

  // Load sample data nếu DB vừa tạo (chưa có bảng rooms)
  // NOTE: simple heuristic: nếu file DB vừa tạo và chưa có room nào -> load sample
  // (Không ném lỗi nếu thất bại)
  {
    sqlite3 *db = nullptr;
    if (sqlite3_open(dbFile.c_str(), &db) == SQLITE_OK) {
      int roomCount = 0;
      auto cbCount = [](void *data, int argc, char **argv, char **cols) -> int {
        int *cnt = static_cast<int *>(data);
        if (argc > 0 && argv[0])
          *cnt = std::stoi(argv[0]);
        return 0;
      };
      sqlite3_exec(db, "SELECT COUNT(1) FROM rooms;", cbCount, &roomCount, 0);

      if (roomCount == 0) {
        char *err = nullptr;
        std::string sqlPath = "sample_data.sql";
        std::string cmd = ".read " + sqlPath; // used with sqlite3 CLI
        // execute file via sqlite3 shell-like command
        std::string script;
        {
          FILE *f = fopen(sqlPath.c_str(), "rb");
          if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            script.resize(len);
            fread(&script[0], 1, len, f);
            fclose(f);
          }
        }
        if (!script.empty()) {
          if (sqlite3_exec(db, script.c_str(), 0, 0, &err) != SQLITE_OK) {
            std::cerr << "Warning: load sample data failed: " << (err ? err : "")
                      << std::endl;
            sqlite3_free(err);
          } else {
            std::cout << "[DB] Sample data loaded." << std::endl;
          }
        }
      }
      sqlite3_close(db);
    }
  }

  // RECOVERY STATE
  RoomManager::getInstance().loadState();

  AuctionServer server(8080);
  server.start();
  return 0;
}
