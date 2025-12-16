CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall

TARGET = server
SRC = main.cpp RoomManager.cpp AuctionServer.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
