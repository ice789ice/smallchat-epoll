#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    int port = 8888;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = std::stoi(argv[2]);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        std::cerr << "invalid address: " << host << '\n';
        close(fd);
        return 1;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << '\n';
        close(fd);
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";
    std::cout << "Please enter nickname first, then chat messages.\n";
    std::cout << "Type /quit to leave, Ctrl+D to quit.\n";

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(fd, &readfds);

        int maxfd = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;
        int ret = select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "select failed: " << std::strerror(errno) << '\n';
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            std::string line;
            if (!std::getline(std::cin, line)) {
                std::cout << "stdin closed, exiting.\n";
                break;
            }
            line.push_back('\n');

            ssize_t sent = send(fd, line.data(), line.size(), 0);
            if (sent <= 0) {
                std::cerr << "send failed: " << std::strerror(errno) << '\n';
                break;
            }
        }

        if (FD_ISSET(fd, &readfds)) {
            char buf[1024];
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n > 0) {
                std::cout << std::string(buf, static_cast<size_t>(n));
            } else if (n == 0) {
                std::cout << "server closed connection.\n";
                break;
            } else {
                std::cerr << "recv failed: " << std::strerror(errno) << '\n';
                break;
            }
        }
    }

    close(fd);
    return 0;
}
