# 1. Khai báo trình biên dịch
CXX      = g++        # Trình biên dịch cho C++
CC       = gcc        # Trình biên dịch cho C (Dành riêng cho SQLite)

# 2. Cờ biên dịch (Flags)
# C++17, hiện cảnh báo, hỗ trợ đa luồng
CXXFLAGS = -std=c++17 -Wall -g 
# Cờ cho C (Chỉ cần tối ưu hóa, không cần check lỗi kỹ)
CFLAGS   = -O2 
# Cờ liên kết thư viện (Quan trọng: -ldl cho SQLite, -lpthread cho Thread)
LDFLAGS  = -lpthread -ldl

# 3. Tên file thực thi đầu ra
TARGET   = server

# 4. Danh sách file nguồn
# Các file C++ của bạn
SRCS_CPP = main.cpp AuctionServer.cpp RoomManager.cpp DatabaseManager.cpp
# File C của SQLite
SRCS_C   = sqlite3.c

# 5. Tạo danh sách file Object (.o) tự động
OBJS     = $(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o)

# --- CÁC QUY TẮC BIÊN DỊCH (RULES) ---

# Rule chính: Liên kết tất cả file .o thành file chạy
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule 1: Biên dịch file .cpp thành .o bằng g++
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule 2: Biên dịch file .c thành .o bằng gcc 
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule dọn dẹp: Xóa file rác để build lại từ đầu
clean:
	rm -f $(OBJS) $(TARGET)