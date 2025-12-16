#include "AuctionServer.h"

int main() {
    AuctionServer server(8080);
    server.start();
    return 0;
}