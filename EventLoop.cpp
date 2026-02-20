#include "EventLoop.h"

#include "Channel.h"

#include <cerrno>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

namespace {
constexpr int kMaxEvents = 16;
}

EventLoop::EventLoop() : epollFd_(epoll_create1(0)), running_(false) {
    if (epollFd_ < 0) {
        std::cerr << "epoll_create1 failed\n";
        std::exit(1);
    }
}

EventLoop::~EventLoop() {
    if (epollFd_ >= 0) {
        close(epollFd_);
    }
}

void EventLoop::addChannel(const std::shared_ptr<Channel>& channel) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = channel->fd();

    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, channel->fd(), &ev) < 0) {
        std::cerr << "epoll_ctl add failed for fd " << channel->fd() << '\n';
        return;
    }
    channels_[channel->fd()] = channel;
}

void EventLoop::removeChannel(int fd) {
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
    channels_.erase(fd);
}

void EventLoop::loop() {
    running_ = true;
    epoll_event events[kMaxEvents];

    while (running_) {
        int n = epoll_wait(epollFd_, events, kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "epoll_wait failed\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            auto it = channels_.find(fd);
            if (it != channels_.end()) {
                while (true) {
                    errno = 0;
                    it->second->handleEvent(events[i].events);
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
    }
}
