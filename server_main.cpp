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

//Hàm này chỉ gửi tin nhắn cho những người đang xem phòng đó
void broadcastToRoom(int roomId, const string& message) {
    lock_guard<mutex> lock(roomsMutex); // Lock lại để an toàn
    
    for (auto& room : rooms) {
        if (room.id == roomId) {
            for (auto sock : room.participants) {
                // Gửi tin nhắn đến từng người trong phòng
                send(sock, message.c_str(), message.length(), 0);
            }
            break; // Tìm thấy phòng rồi thì thoát loop
        }
    }
}

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
string processCommand(SocketType clientSocket, const string& msg) {
    auto tokens = split(msg, '|');
    if (tokens.empty()) return "ERR|EMPTY_COMMAND";

    string command = tokens[0];

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
        
        string itemName = tokens[1];
        int startPrice = stoi(tokens[2]); // Chuyển chuỗi sang số

        Room newRoom;
        newRoom.id = roomIdCounter++; // Tăng ID lên
        newRoom.itemName = itemName;
        newRoom.currentPrice = startPrice;
        newRoom.highestBidderSocket = -1; // Chưa ai bid

        // Critical Section: Thêm vào danh sách chung
        {
            lock_guard<mutex> lock(roomsMutex);
            rooms.push_back(newRoom);
        }

        return "OK|ROOM_CREATED|" + to_string(newRoom.id);
    }

    // --- LỆNH: LIỆT KÊ PHÒNG ---
    else if (command == "LIST_ROOMS") {
        string response = "OK|LIST|";
        
        lock_guard<mutex> lock(roomsMutex);
        if (rooms.empty()) return "OK|LIST|EMPTY";

        for (const auto& r : rooms) {
            // Định dạng trả về: ID:Name:Price;
            response += to_string(r.id) + ":" + r.itemName + ":" + to_string(r.currentPrice) + ";";
        }
        return response;
    }

    // --- LỆNH: ĐẤU GIÁ (QUAN TRỌNG) ---
    // Cú pháp: BID|room_id|price
    else if (command == "BID") {
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        
        int rId = stoi(tokens[1]);
        int bidPrice = stoi(tokens[2]);

        lock_guard<mutex> lock(roomsMutex);
        
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
            return "OK|BID_SUCCESS|" + to_string(bidPrice);
        } else {
            return "ERR|PRICE_TOO_LOW|Current is " + to_string(targetRoom->currentPrice);
        }
    }
  // --- LỆNH: VÀO PHÒNG ---
    // Cú pháp: JOIN_ROOM|<room_id>  
    else if (command == "JOIN_ROOM") {
        if (tokens.size() < 2) return "ERR|MISSING_ARGS";
        int rId = stoi(tokens[1]);
        
        lock_guard<mutex> lock(roomsMutex);
        for (auto& r : rooms) {
            if (r.id == rId) {
                // Thêm socket này vào danh sách người tham gia
                r.participants.push_back(clientSocket);
                
                // Trả về thông tin hiện tại của phòng để Client hiển thị
                // Format: OK|JOINED|<id>|<name>|<current_price>
                return "OK|JOINED|" + to_string(r.id) + "|" + r.itemName + "|" + to_string(r.currentPrice);
            }
        }
        return "ERR|ROOM_NOT_FOUND";
    }
    // --- LỆNH: ĐẤU GIÁ ---
    // Cú pháp: BID|<room_id>|<price>
    else if (command == "BID") {
        if (tokens.size() < 3) return "ERR|MISSING_ARGS";
        int rId = stoi(tokens[1]);
        int bidPrice = stoi(tokens[2]);

        // (Lưu ý: Không lock ở đây nữa vì hàm broadcastToRoom sẽ lock, tránh deadlock)
        // Chúng ta sẽ tìm phòng và xử lý cục bộ trước
        bool success = false;
        string broadcastMsg;

        {
            lock_guard<mutex> lock(roomsMutex);
            for (auto& r : rooms) {
                if (r.id == rId) {
                    if (bidPrice > r.currentPrice) {
                        r.currentPrice = bidPrice;
                        r.highestBidderSocket = clientSocket;
                        success = true;
                        
                        // Chuẩn bị tin nhắn để báo cho cả phòng
                        // Format: NEW_BID|<price>|<user_id_tam_thoi>
                        broadcastMsg = "NEW_BID|" + to_string(bidPrice) + "|" + to_string(clientSocket) + "\n";
                    }
                    break;
                }
            }
        }

        if (success) {
            // Gửi thông báo cho TẤT CẢ mọi người trong phòng (bao gồm cả người bid)
            // Lưu ý: broadcastToRoom tự nó sẽ lock mutex, nên ta gọi nó ngoài scope lock ở trên
            // Tuy nhiên, logic broadcastToRoom mình viết ở trên có lock, nên phải cẩn thận.
            // Để đơn giản cho bài tập: Ta sẽ chấp nhận lock 2 lần (recursive) hoặc bỏ lock trong broadcastToRoom.
            // TỐT NHẤT: Copy logic broadcast vào đây để tránh deadlock hoặc sửa broadcastToRoom bỏ lock đi.
            
            // --> Cách an toàn nhất: Gọi broadcast ngoài lock scope
             broadcastToRoom(rId, broadcastMsg);
             return "OK|BID_ACCEPTED"; // Server trả lời riêng cho người gửi để xác nhận
        } else {
            return "ERR|PRICE_TOO_LOW";
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

    // 1. Tạo Socket
    SocketType serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Can't create socket" << endl;
        return 1;
    }

    // 2. Bind địa chỉ IP và Port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Chấp nhận mọi IP
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed" << endl;
        return 1;
    }

    // 3. Listen (Lắng nghe kết nối)
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed" << endl;
        return 1;
    }

    cout << "=== AUCTION SERVER STARTED ON PORT " << PORT << " ===" << endl;

    // 4. Vòng lặp chấp nhận Client
    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        
        SocketType clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, (socklen_t*)&clientAddrSize);

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

    close(serverSocket);

    return 0;
}