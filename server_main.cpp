#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <sstream>

using namespace std;

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int SocketType;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1


// Cấu hình Server
const int PORT = 8080;
const int BUFFER_SIZE = 1024;

struct Room {
    int id;
    string itemName;
    int currentPrice;
    int highestBidderSocket; // Socket của người đang thắng thế
    vector<SocketType> participants; // Danh sách người đang xem phòng này
};

// Dữ liệu toàn cục (In-memory Database)
vector<Room> rooms;
mutex roomsMutex; // Khóa bảo vệ danh sách phòng
int roomIdCounter = 1; // Tự động tăng ID phòng

// 2. Hàm cắt chuỗi (Split String)
// Ví dụ: "LOGIN|admin|123" -> ["LOGIN", "admin", "123"]
vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
// 3. Hàm xử lý lệnh (Core Logic)
std::string processCommand(SocketType clientSocket, const std::string& msg) {
    auto tokens = split(msg, '|');
    if (tokens.empty()) return "ERR|EMPTY_COMMAND";

    std::string command = tokens[0];

    // --- LỆNH: ĐĂNG NHẬP ---
    if (command == "LOGIN") {
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        // Tạm thời chấp nhận mọi user, sau này sẽ check DB
        return "OK|LOGIN_SUCCESS|Welcome " + tokens[1];
    }

    // --- LỆNH: TẠO PHÒNG ---
    // Cú pháp: CREATE_ROOM|Iphone 15|20000000
    else if (command == "CREATE_ROOM") {
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        
        std::string itemName = tokens[1];
        int startPrice = std::stoi(tokens[2]); // Chuyển chuỗi sang số

        Room newRoom;
        newRoom.id = roomIdCounter++; // Tăng ID lên
        newRoom.itemName = itemName;
        newRoom.currentPrice = startPrice;
        newRoom.highestBidderSocket = -1; // Chưa ai bid

        // Critical Section: Thêm vào danh sách chung
        {
            std::lock_guard<std::mutex> lock(roomsMutex);
            rooms.push_back(newRoom);
        }

        return "OK|ROOM_CREATED|" + std::to_string(newRoom.id);
    }

    // --- LỆNH: LIỆT KÊ PHÒNG ---
    else if (command == "LIST_ROOMS") {
        std::string response = "OK|LIST|";
        
        std::lock_guard<std::mutex> lock(roomsMutex);
        if (rooms.empty()) return "OK|LIST|EMPTY";

        for (const auto& r : rooms) {
            // Định dạng trả về: ID:Name:Price;
            response += std::to_string(r.id) + ":" + r.itemName + ":" + std::to_string(r.currentPrice) + ";";
        }
        return response;
    }

    // --- LỆNH: ĐẤU GIÁ (QUAN TRỌNG) ---
    // Cú pháp: BID|room_id|price
    else if (command == "BID") {
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        
        int rId = std::stoi(tokens[1]);
        int bidPrice = std::stoi(tokens[2]);

        std::lock_guard<std::mutex> lock(roomsMutex);
        
        // Tìm phòng
        Room* targetRoom = nullptr;
        for (auto& r : rooms) {
            if (r.id == rId) {
                targetRoom = &r;
                break;
            }
        }

        if (!targetRoom) return "ERR|ROOM_NOT_FOUND";

        // Logic kiểm tra giá
        if (bidPrice > targetRoom->currentPrice) {
            targetRoom->currentPrice = bidPrice;
            targetRoom->highestBidderSocket = clientSocket;
            
            // TODO: Ở đây cần Broadcast cho mọi người trong phòng biết (Sẽ làm ở bước sau)
            return "OK|BID_SUCCESS|" + std::to_string(bidPrice);
        } else {
            return "ERR|PRICE_TOO_LOW|Current is " + std::to_string(targetRoom->currentPrice);
        }
    }

    return "ERR|UNKNOWN_COMMAND";
}


// Quản lý danh sách Client
vector<SocketType> clients;
mutex clientsMutex; // Khóa bảo vệ danh sách client (Thread-safety)

// --- HÀM HỖ TRỢ ---

// Hàm gửi tin nhắn cho TẤT CẢ client (Broadcast) - Dùng cho thông báo đấu giá
void broadcastMessage(const string& message, SocketType senderSocket) {
    lock_guard<mutex> lock(clientsMutex); // Lock lại để tránh xung đột
    
    for (auto client : clients) {
        if (client != senderSocket) { // (Tùy chọn) Không gửi lại cho người gửi
            send(client, message.c_str(), message.length(), 0);
        }
    }
}

// Hàm xử lý logic cho từNG Client (Chạy trên thread riêng)
void handleClient(SocketType clientSocket) {
    char buffer[BUFFER_SIZE];
    
    cout << "[INFO] Client connected: " << clientSocket << endl;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        
        // Đọc dữ liệu từ Client
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        
        if (bytesReceived <= 0) {
            cout << "[INFO] Client disconnected: " << clientSocket << endl;
            break;
        }

        string msg(buffer);
        // Xóa ký tự xuống dòng thừa (nếu có)
        msg.erase(remove(msg.begin(), msg.end(), '\n'), msg.end());
        msg.erase(remove(msg.begin(), msg.end(), '\r'), msg.end());

        cout << "[RECV from " << clientSocket << "]: " << msg << endl;

        // --- KHU VỰC XỬ LÝ LOGIC ĐẤU GIÁ (Sẽ code sau) ---
        // GỌI HÀM XỬ LÝ
        string response = processCommand(clientSocket, msg);        
        // Demo: Server phản hồi lại Client
        response += "\n";
        send(clientSocket, response.c_str(), response.length(), 0);
        
        // Demo: Broadcast cho các người khác
        broadcastMessage("User " + to_string(clientSocket) + " says: " + msg + "\n", clientSocket);
    }

    // Khi client ngắt kết nối: Đóng socket và xóa khỏi danh sách
#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif

    lock_guard<mutex> lock(clientsMutex);
    auto it = find(clients.begin(), clients.end(), clientSocket);
    if (it != clients.end()) {
        clients.erase(it);
    }
}

// --- MAIN FUNCTION ---
int main() {
    // 1. Khởi tạo Winsock (Chỉ dành cho Windows)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    // 2. Tạo Socket
    SocketType serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Can't create socket" << endl;
        return 1;
    }

    // 3. Bind địa chỉ IP và Port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Chấp nhận mọi IP
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed" << endl;
        return 1;
    }

    // 4. Listen (Lắng nghe kết nối)
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed" << endl;
        return 1;
    }

    cout << "=== AUCTION SERVER STARTED ON PORT " << PORT << " ===" << endl;

    // 5. Vòng lặp chấp nhận Client
    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        
#ifdef _WIN32
        SocketType clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
#else
        SocketType clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, (socklen_t*)&clientAddrSize);
#endif

        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed" << endl;
            continue;
        }

        // Thêm client vào danh sách quản lý
        {
            lock_guard<mutex> lock(clientsMutex);
            clients.push_back(clientSocket);
        }

        // Tạo Thread riêng để xử lý Client này -> Detach để chạy ngầm
        thread t(handleClient, clientSocket);
        t.detach(); 
    }

    // Dọn dẹp (Thực tế code server ít khi chạy đến đây trừ khi có lệnh tắt)
#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#else
    close(serverSocket);
#endif

    return 0;
}