#ifndef AUCTIONSERVER_H
#define AUCTIONSERVER_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include "RoomManager.h" // Server cần gọi Manager

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int SocketType;

class AuctionServer {
public:
    AuctionServer(int port);
    ~AuctionServer();
    void start(); // Hàm main loop

private:
    int port;
    SocketType serverSocket;
    
    // Hàm xử lý từng client (chạy trên thread)
    void handleClient(SocketType clientSocket);
    
    // Hàm xử lý lệnh (Parser)
    std::string processCommand(SocketType clientSocket, const std::string& msg);
    
    // Hàm gửi tin nhắn cho phòng
    void broadcastToRoom(int roomId, const std::string& msg);
    
    // Hàm tách chuỗi
    std::vector<std::string> split(const std::string &s, char delimiter);
};

#endif