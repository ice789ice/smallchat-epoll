#pragma once

#include <memory>
#include <unordered_map>

class Channel;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void loop();
    void addChannel(const std::shared_ptr<Channel>& channel);
    void removeChannel(int fd);

private:
    int epollFd_;
    bool running_;
    std::unordered_map<int, std::shared_ptr<Channel>> channels_;
};
