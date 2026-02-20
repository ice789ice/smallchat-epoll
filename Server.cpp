#include "Server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace {
bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}
}

Server::Server(int port) : listenFd_(-1) {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        std::cerr << "socket failed\n";
        exit(1);
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind failed\n";
        exit(1);
    }

    if (!setNonBlocking(listenFd_)) {
        std::cerr << "setNonBlocking failed for listenFd\n";
        exit(1);
    }

    listen(listenFd_, 5);

    listenChannel_ = std::make_shared<Channel>(listenFd_);
    listenChannel_->setReadCallback([this]() { handleNewConnection(); });
    loop_.addChannel(listenChannel_);
}

Server::~Server() {
    close(listenFd_);
}

void Server::run() {
    loop_.loop();
}

void Server::handleNewConnection() {
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int clientFd = accept(listenFd_, (sockaddr*)&clientAddr, &len);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "accept failed\n";
            break;
        }

        if (!setNonBlocking(clientFd)) {
            std::cerr << "setNonBlocking failed for clientFd " << clientFd << '\n';
            close(clientFd);
            continue;
        }

        auto conn = std::make_shared<Connection>(clientFd);
        connections_[clientFd] = {conn, false, "", ""};

        auto channel = std::make_shared<Channel>(clientFd);
        channel->setReadCallback([this, clientFd]() { handleClientMessage(clientFd); });
        loop_.addChannel(channel);

        std::cout << "New client: " << clientFd << std::endl;
        conn->sendMessage("[system]: welcome, please enter nickname\n");
    }
}

void Server::handleClientMessage(int fd) {
    while (true) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            errno = EAGAIN;
            break;
        }

        errno = 0;
        std::string chunk;
        if (!it->second.conn->handleRead(chunk)) {
            removeClient(fd);
            errno = EAGAIN;
            break;
        }

        if (!chunk.empty()) {
            it->second.inputBuffer.append(chunk);
            while (true) {
                size_t pos = it->second.inputBuffer.find('\n');
                if (pos == std::string::npos) {
                    break;
                }

                std::string line = it->second.inputBuffer.substr(0, pos);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                it->second.inputBuffer.erase(0, pos + 1);

                processClientLine(fd, line);
                if (connections_.find(fd) == connections_.end()) {
                    errno = EAGAIN;
                    return;
                }
            }
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
    }
}

void Server::processClientLine(int fd, const std::string& line) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    auto& state = it->second;

    if (!state.hasNickname) {
        if (line.empty()) {
            state.conn->sendMessage("[system]: nickname cannot be empty, retry\n");
            return;
        }
        state.nickname = line;
        state.hasNickname = true;
        state.conn->sendMessage("[system]: nickname set, start chatting\n");
        broadcastToOthers(fd, "[system]: " + state.nickname + " joined\n");
        return;
    }

    if (line == "/quit") {
        std::string left = "[system]: " + state.nickname + " left\n";
        broadcastToOthers(fd, left);
        removeClient(fd);
        return;
    }

    if (!line.empty()) {
        std::string out = "[" + state.nickname + "]: " + line + "\n";
        broadcastToOthers(fd, out);
    }
}

void Server::removeClient(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    loop_.removeChannel(fd);
    close(fd);
    connections_.erase(it);
    std::cout << "Client disconnected: " << fd << std::endl;
}

void Server::broadcastToOthers(int srcFd, const std::string& message) {
    for (auto& kv : connections_) {
        if (kv.first == srcFd) {
            continue;
        }
        if (!kv.second.hasNickname) {
            continue;
        }
        kv.second.conn->sendMessage(message);
    }
}
