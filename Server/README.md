Hệ Thống Đấu Giá Trực Tuyến - Server Side

Đây là phần backend (Server) của dự án Đấu giá trực tuyến. Server được viết bằng C++ thuần, sử dụng kiến trúc Đa luồng (Multithreading) để xử lý nhiều kết nối đồng thời và SQLite để lưu trữ dữ liệu bền vững.

🛠 Công nghệ sử dụng

Ngôn ngữ: C++ (Standard C++11/17).

Giao tiếp mạng: Raw TCP Sockets (Sys-socket trên Linux).

Cơ sở dữ liệu: SQLite (Embedded).

Mô hình: Singleton Pattern (cho các Manager), Mutex Locking (xử lý đồng bộ luồng).

📂 Cấu trúc thư mục

Server/
├── main.cpp                # Điểm khởi chạy chương trình, khởi tạo Database và Server.
├── AuctionServer.h/.cpp    # [Network Layer] Quản lý kết nối Socket, luồng (Thread) cho từng Client.
├── RoomManager.h/.cpp      # [Business Logic] Quản lý phòng đấu giá, timer, xử lý Bid/BuyNow/Queue.
├── DatabaseManager.h/.cpp  # [Data Layer] Xử lý mọi thao tác SQL (Login, Register, History).
├── Room.h                  # [Model] Định nghĩa cấu trúc dữ liệu: Room, Product, SoldItem.
├── sqlite3.c / .h          # Thư viện SQLite (Amalgamation code).
├── auction_system.db       # File cơ sở dữ liệu (Tự động sinh ra khi chạy).
└── Makefile                # File cấu hình biên dịch.



🚀 Hướng dẫn Cài đặt & Chạy

Yêu cầu

Trình biên dịch G++ hoặc MSVC.

Thư viện pthread (thường có sẵn trên Linux).

Thư viện libdl (cho SQLite trên Linux).

Cách biên dịch (Sử dụng Makefile)

Mở terminal tại thư mục Server.

Chạy lệnh để biên dịch:

make



Lệnh này sẽ tự động biên dịch chéo C (cho SQLite) và C++ (cho Server).

Chạy Server:

Linux/WSL: ./server

Windows: server.exe

Server sẽ lắng nghe tại Port mặc định 8080.

📡 Giao thức giao tiếp (Protocol)

Hệ thống sử dụng giao thức dạng chuỗi văn bản, ngăn cách bởi ký tự |. Mỗi lệnh kết thúc bằng ký tự xuống dòng \n.

Chức năng

Client gửi (Request)

Server trả về (Response)

Đăng nhập

`LOGIN

user

Đăng ký

`REGISTER

user

Tạo phòng

`CREATE_ROOM

Name

Vào phòng

`JOIN_ROOM

ID`

Đấu giá

`BID

ID

Lấy DS SP

`GET_PRODUCTS

ID`

👨‍💻 Hướng dẫn Thêm/Sửa Tính năng mới

Quy trình chuẩn để thêm một tính năng (Ví dụ: Tính năng "Kick User") gồm 3 bước:

Bước 1: Định nghĩa Protocol

Quyết định định dạng lệnh. Ví dụ Client sẽ gửi: KICK|RoomID|UserID.

Bước 2: Xử lý lệnh tại AuctionServer.cpp

Tìm hàm processCommand, thêm nhánh else if mới:

else if (cmd == "KICK") {
    int rId = std::stoi(tokens[1]);
    int uId = std::stoi(tokens[2]);
    
    // Gọi Logic xử lý bên Manager
    if (RoomManager::getInstance().kickUser(rId, uId)) {
        return "OK|KICK_SUCCESS";
    }
    return "ERR|CANNOT_KICK";
}



Bước 3: Cài đặt Logic nghiệp vụ

Nếu tính năng liên quan đến phòng, mở RoomManager.cpp:

bool RoomManager::kickUser(int roomId, int userId) {
    std::lock_guard<std::recursive_mutex> lock(roomsMutex); // Quan trọng: Phải khóa Mutex
    // 1. Tìm phòng
    // 2. Tìm User trong phòng -> Xóa khỏi vector participants
    // 3. Đóng socket của user đó (nếu cần)
    return true;
}

