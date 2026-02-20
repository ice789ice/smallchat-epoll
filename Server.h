#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "Channel.h"
#include "Connection.h"
#include "EventLoop.h"

class Server {
public:
    explicit Server(int port);
    ~Server();
    void run();

private:
    struct ClientState {
        std::shared_ptr<Connection> conn;
        bool hasNickname;
        std::string nickname;
        std::string inputBuffer;
    };

    void handleNewConnection();
    void handleClientMessage(int fd);
    void processClientLine(int fd, const std::string& line);
    void removeClient(int fd);
    void broadcastToOthers(int srcFd, const std::string& message);

    int listenFd_;
    EventLoop loop_;
    std::shared_ptr<Channel> listenChannel_;
    std::unordered_map<int, ClientState> connections_;
};
