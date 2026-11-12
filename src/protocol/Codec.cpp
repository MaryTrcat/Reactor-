#include "protocol/Codec.h"

#include <algorithm>
#include <stdexcept>

namespace protocol {

namespace {

void writeU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void writeU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
}

std::uint16_t readU16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

std::uint32_t readU32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

}  // namespace

std::vector<std::uint8_t> Codec::encode(const Frame& frame) const {
    if (frame.body.size() > kMaxBodySize) {
        throw std::runtime_error("frame body is too large");
    }

    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + frame.body.size());
    writeU32(out, static_cast<std::uint32_t>(frame.body.size()));
    writeU16(out, frame.message_type);
    writeU32(out, frame.request_id);
    out.insert(out.end(), frame.body.begin(), frame.body.end());
    return out;
}

void Codec::append(const std::uint8_t* data, std::size_t size) {
    if (failed_ || data == nullptr || size == 0) {
        return;
    }
    buffer_.insert(buffer_.end(), data, data + size);
}

std::optional<Frame> Codec::tryDecode() {
    if (failed_) {
        return std::nullopt;
    }
    if (buffer_.size() < kHeaderSize) {
        return std::nullopt;
    }

    const auto* header = buffer_.data();
    const auto body_length = readU32(header);
    if (body_length > kMaxBodySize) {
        // 恶意或损坏的帧头：标记失败并丢弃缓冲，避免把 4GB 的 body_length 当真实长度。
        failed_ = true;
        error_ = "frame body length exceeds limit";
        buffer_.clear();
        return std::nullopt;
    }

    const auto frame_size = kHeaderSize + static_cast<std::size_t>(body_length);
    if (buffer_.size() < frame_size) {
        return std::nullopt;
    }

    Frame frame;
    frame.message_type = readU16(header + 4);
    frame.request_id = readU32(header + 6);
    frame.body.assign(reinterpret_cast<const char*>(header + kHeaderSize),
                      reinterpret_cast<const char*>(header + frame_size));

    // 用 erase 而不是维护读指针，帧体通常不大且这样代码更直观；
    // 一次读事件里解出多帧时的重复搬移由 reserve 的容量增长摊平。
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(frame_size));
    return frame;
}

std::size_t Codec::bufferedBytes() const {
    return buffer_.size();
}

bool Codec::failed() const {
    return failed_;
}

const std::string& Codec::error() const {
    return error_;
}

void Codec::reset() {
    buffer_.clear();
    failed_ = false;
    error_.clear();
}

}  // namespace protocol
