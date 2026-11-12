#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace base {

// 自包含的 SHA-256 / HMAC-SHA256 / PBKDF2 实现。
//
// 不依赖 OpenSSL 是为了让 Windows 上的核心单元测试也能覆盖密码哈希逻辑：
// 项目里原本用 std::hash 存密码，换成 PBKDF2 之后必须能被测试验证。
class Sha256 {
public:
    Sha256();

    void update(const std::uint8_t* data, std::size_t size);
    std::array<std::uint8_t, 32> digest();

private:
    void processBlock(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_{0};
    std::uint64_t total_size_{0};
};

std::array<std::uint8_t, 32> hmacSha256(const std::vector<std::uint8_t>& key,
                                        const std::uint8_t* data,
                                        std::size_t size);

std::vector<std::uint8_t> pbkdf2HmacSha256(const std::string& password,
                                           const std::vector<std::uint8_t>& salt,
                                           std::uint32_t iterations,
                                           std::size_t key_size);

std::string base64Encode(const std::vector<std::uint8_t>& data);
std::optional<std::vector<std::uint8_t>> base64Decode(const std::string& text);

std::vector<std::uint8_t> randomBytes(std::size_t size);

// 比较密码哈希时使用，避免按字节短路比较泄露前缀信息。
bool constantTimeEquals(const std::string& left, const std::string& right);

}  // namespace base
