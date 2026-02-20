#include "Channel.h"

#include <cerrno>
#include <sys/epoll.h>

Channel::Channel(int fd) : fd_(fd), events_(EPOLLIN | EPOLLET) {}

int Channel::fd() const {
    return fd_;
}

uint32_t Channel::events() const {
    return events_;
}

void Channel::setReadCallback(std::function<void()> cb) {
    readCallback_ = std::move(cb);
}

void Channel::handleEvent(uint32_t revents) const {
    if ((revents & (EPOLLIN | EPOLLPRI | EPOLLHUP | EPOLLERR)) && readCallback_) {
        while (true) {
            errno = 0;
            readCallback_();
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno != 0) {
                break;
            }
        }
    }
}
