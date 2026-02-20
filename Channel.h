#pragma once

#include <cstdint>
#include <functional>

class Channel {
public:
    explicit Channel(int fd);

    int fd() const;
    uint32_t events() const;

    void setReadCallback(std::function<void()> cb);
    void handleEvent(uint32_t revents) const;

private:
    int fd_;
    uint32_t events_;
    std::function<void()> readCallback_;
};
