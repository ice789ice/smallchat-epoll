#include "Connection.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd), closed_(false) {}

Connection::~Connection() {
    if (!closed_) {
        close(fd_);
    }
}

int Connection::getFd() const {
    return fd_;
}

bool Connection::handleRead(std::string& out) {
    char buf[1024];
    ssize_t n = read(fd_, buf, sizeof(buf));
    if (n > 0) {
        out.assign(buf, static_cast<size_t>(n));
        return true;
    } else if (n == 0) {
        closed_ = true;
        return false; // 客户端关闭
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            closed_ = true;
            return false;
        }
    }
    return true;
}

void Connection::sendMessage(const std::string &msg) {
    ::send(fd_, msg.c_str(), msg.size(), 0);
}

bool Connection::isClosed() const {
    return closed_;
}
