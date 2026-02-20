#include "Server.h"
#include <iostream>

int main() {
    Server server(8888);
    std::cout << "Server running on port 8888\n";
    server.run();
    return 0;
}
