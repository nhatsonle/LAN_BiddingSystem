#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

#include <netinet/in.h>
#include <sys/socket.h>
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
  int highestBidderSocket;         // Socket của người đang thắng thế
  vector<SocketType> participants; // Danh sách người đang xem phòng này
};

// Dữ liệu toàn cục (In-memory Database)
vector<Room> rooms;
mutex roomsMutex;      // Khóa bảo vệ danh sách phòng
int roomIdCounter = 1; // Tự động tăng ID phòng

// Hàm này chỉ gửi tin nhắn cho những người đang xem phòng đó
void broadcastToRoom(int roomId, const string &message) {
  // Không cần lock mutex ở đây nếu bên ngoài đã lock,
  // nhưng để an toàn ta cứ lock scope nhỏ
  // Lưu ý: Nếu processCommand đang giữ lock thì ở đây KHÔNG ĐƯỢC lock nữa (sẽ
  // bị Deadlock). TỐT NHẤT LÀ:

  vector<SocketType> targets;

  {
    lock_guard<mutex> lock(roomsMutex);
    for (auto &room : rooms) {
      if (room.id == roomId) {
        targets = room.participants; // Copy danh sách socket ra ngoài
        break;
      }
    }
  }
  // Giải phóng lock rồi mới gửi tin -> Tránh deadlock và tăng tốc độ

  for (auto sock : targets) {
    send(sock, message.c_str(), message.length(), 0);
  }

  cout << "[BROADCAST] Sent to " << targets.size() << " clients: " << message;
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
string processCommand(SocketType clientSocket, const string &msg) {
  auto tokens = split(msg, '|');
  if (tokens.empty())
    return "ERR|EMPTY_COMMAND";

  string command = tokens[0];

  // --- LỆNH: ĐĂNG NHẬP ---
  if (command == "LOGIN") {
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    // Tạm thời chấp nhận mọi user, sau này sẽ check DB
    return "OK|LOGIN_SUCCESS|Welcome " + tokens[1];
  }

  // --- LỆNH: TẠO PHÒNG ---
  // Cú pháp: CREATE_ROOM|Iphone 15|20000000
  else if (command == "CREATE_ROOM") {
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";

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
    if (rooms.empty())
      return "OK|LIST|EMPTY";

    for (const auto &r : rooms) {
      // Định dạng trả về: ID:Name:Price;
      response += to_string(r.id) + ":" + r.itemName + ":" +
                  to_string(r.currentPrice) + ";";
    }
    return response;
  }
  // --- LỆNH: VÀO PHÒNG ---
  // Cú pháp: JOIN_ROOM|<room_id>
  else if (command == "JOIN_ROOM") {
    if (tokens.size() < 2)
      return "ERR|MISSING_ARGS";
    int rId = stoi(tokens[1]);

    lock_guard<mutex> lock(roomsMutex);
    for (auto &r : rooms) {
      if (r.id == rId) {
        // Thêm socket này vào danh sách người tham gia
        r.participants.push_back(clientSocket);

        // Trả về thông tin hiện tại của phòng để Client hiển thị
        // Format: OK|JOINED|<id>|<name>|<current_price>
        return "OK|JOINED|" + to_string(r.id) + "|" + r.itemName + "|" +
               to_string(r.currentPrice);
      }
    }
    return "ERR|ROOM_NOT_FOUND";
  }
  // --- LỆNH: ĐẤU GIÁ ---
  // Cú pháp: BID|<room_id>|<price>
  else if (command == "BID") {
    if (tokens.size() < 3)
      return "ERR|MISSING_ARGS";
    int rId = stoi(tokens[1]);
    int bidPrice = stoi(tokens[2]);

    string broadcastMsg = "";
    bool success = false;

    {
      lock_guard<mutex> lock(roomsMutex);
      for (auto &r : rooms) {
        if (r.id == rId) {
          if (bidPrice > r.currentPrice) {
            r.currentPrice = bidPrice;
            r.highestBidderSocket = clientSocket;
            success = true;

            // Chuẩn bị tin nhắn Broadcast
            broadcastMsg = "NEW_BID|" + to_string(bidPrice) + "|" +
                           to_string(clientSocket) + "\n";
          }
          break;
        }
      }
    } // Lock tự động mở ở đây

    if (success) {
      // GỌI HÀM BROADCAST (Đây là dòng bạn có thể đang thiếu hoặc đặt sai chỗ)
      broadcastToRoom(rId, broadcastMsg);

      return "OK|BID_SUCCESS|" + to_string(bidPrice);
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
void broadcastMessage(const string &message, SocketType senderSocket) {
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
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

    if (bytesReceived <= 0) {
      cout << "[INFO] Client disconnected: " << clientSocket << endl;
      // TODO: Xử lý xóa client khỏi room nếu cần
      break;
    }

    string msg(buffer);
    // Xóa ký tự xuống dòng thừa
    msg.erase(remove(msg.begin(), msg.end(), '\n'), msg.end());
    msg.erase(remove(msg.begin(), msg.end(), '\r'), msg.end());

    cout << "[RECV from " << clientSocket << "]: " << msg << endl;

    // --- QUAN TRỌNG: CHỈ GỌI PROCESS COMMAND ---
    string response = processCommand(clientSocket, msg);

    // Chỉ gửi phản hồi nếu có nội dung (để tránh gửi chuỗi rỗng)
    if (!response.empty()) {
      response += "\n"; // Luôn thêm \n để Client Qt nhận diện
      send(clientSocket, response.c_str(), response.length(), 0);
    }

    // --- XÓA BỎ ĐOẠN CODE "broadcastMessage(...)" CŨ Ở ĐÂY ---
    // Tuyệt đối không để đoạn code: broadcastMessage("User says: " + msg...) ở
    // đây Vì nó sẽ gửi lại lệnh JOIN_ROOM cho tất cả mọi người, gây nhiễu.
  }

  // Đóng socket
#ifdef _WIN32
  closesocket(clientSocket);
#else
  close(clientSocket);
#endif
  // Xóa khỏi danh sách client global...
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

  if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
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

    SocketType clientSocket =
        accept(serverSocket, (struct sockaddr *)&clientAddr,
               (socklen_t *)&clientAddrSize);

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
  shutdown(serverSocket, SHUT_RDWR);
  close(serverSocket);

  return 0;
}