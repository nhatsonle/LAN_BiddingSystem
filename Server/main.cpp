#include "AuctionServer.h"
#include "DatabaseManager.h"

int main() {
    // Khởi tạo DB trước khi bật Server
    if (!DatabaseManager::getInstance().init("auction_system.db")) {
        std::cerr << "Failed to init DB" << std::endl;
        return -1;
    }

    // Tạo sẵn 1 user mẫu để test
    DatabaseManager::getInstance().registerUser("admin", "123456");

    AuctionServer server(8080);
    server.start();
    return 0;
}