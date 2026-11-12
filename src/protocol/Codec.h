#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace protocol {

constexpr std::size_t kHeaderSize = 10;
constexpr std::uint32_t kMaxBodySize = 1024 * 1024;

struct Frame {
    std::uint16_t message_type{};
    std::uint32_t request_id{};
    std::string body;
};

// 增量解码器：处理 TCP 粘包/半包。
//
// 解码过程不抛异常：帧头非法（长度超限等）时置 failed() 并丢弃缓冲，由调用方
// 决定关闭连接。异常一旦从 epoll 事件循环里逃出去会直接 std::terminate，
// 所以这里把所有非法输入都收敛成状态位。
class Codec {
public:
    std::vector<std::uint8_t> encode(const Frame& frame) const;
    void append(const std::uint8_t* data, std::size_t size);
    std::optional<Frame> tryDecode();
    std::size_t bufferedBytes() const;

    bool failed() const;
    const std::string& error() const;
    void reset();

private:
    std::vector<std::uint8_t> buffer_;
    bool failed_{false};
    std::string error_;
};

}  // namespace protocol
