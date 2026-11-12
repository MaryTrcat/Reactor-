#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace business {

// 密码哈希：PBKDF2-HMAC-SHA256 + 每用户随机盐。
//
// 存储格式：pbkdf2-sha256$<iterations>$<salt_base64>$<hash_base64>
// 老数据是 std::hash 拼出来的十进制串，登录成功后会按 needRehash 自动升级，
// 这样不用停机洗数据也能平滑迁移。
class PasswordHasher {
public:
    // 12000 次迭代在本机约 20~40ms，够用来做演示；
    // 生产环境建议按 OWASP 当前建议提到 600000 次或改用 Argon2id。
    static constexpr std::uint32_t kDefaultIterations = 12000;
    static constexpr std::size_t kSaltBytes = 16;
    static constexpr std::size_t kKeyBytes = 32;

    static std::string hash(const std::string& password);
    static std::string hashWithIterations(const std::string& password, std::uint32_t iterations);

    // 校验密码；needs_rehash 用于把老哈希或低迭代次数哈希标记为待升级。
    static bool verify(const std::string& password, const std::string& stored_hash, bool& needs_rehash);

    static bool isLegacyHash(const std::string& stored_hash);

    // 兼容旧数据：与改造前 UserService::hashPassword 的计算方式一致。
    static std::string legacyHash(const std::string& password);

    // 管理员重置密码时生成一次性随机密码。
    static std::string generatePassword(std::size_t length = 12);
};

}  // namespace business
