#include "net/Buffer.h"

#include <algorithm>

namespace net {

void Buffer::append(const std::uint8_t* data, std::size_t size) {
    data_.insert(data_.end(), data, data + size);
}

void Buffer::append(const std::vector<std::uint8_t>& data) {
    data_.insert(data_.end(), data.begin(), data.end());
}

void Buffer::append(const std::string& data) {
    data_.insert(data_.end(), data.begin(), data.end());
}

const std::uint8_t* Buffer::data() const {
    return data_.data();
}

std::vector<std::uint8_t> Buffer::takeAll() {
    std::vector<std::uint8_t> taken;
    taken.swap(data_);
    return taken;
}

void Buffer::prepend(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return;
    }
    data_.insert(data_.begin(), data, data + size);
}

std::size_t Buffer::size() const {
    return data_.size();
}

bool Buffer::empty() const {
    return data_.empty();
}

void Buffer::consume(std::size_t size) {
    const auto count = std::min(size, data_.size());
    data_.erase(data_.begin(), data_.begin() + static_cast<std::ptrdiff_t>(count));
}

}  // namespace net
