#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace net {

class Buffer {
public:
    void append(const std::uint8_t* data, std::size_t size);
    void append(const std::vector<std::uint8_t>& data);
    void append(const std::string& data);
    // 把待发数据整体取出，让 send() 可以在锁外执行，避免阻塞生产者线程。
    std::vector<std::uint8_t> takeAll();
    // 发送不完整时把剩余部分放回队首，保证字节序不被新数据插队。
    void prepend(const std::uint8_t* data, std::size_t size);
    const std::uint8_t* data() const;
    std::size_t size() const;
    bool empty() const;
    void consume(std::size_t size);

private:
    std::vector<std::uint8_t> data_;
};

}  // namespace net
