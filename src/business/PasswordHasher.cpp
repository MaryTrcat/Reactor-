#include "business/PasswordHasher.h"

#include "base/Crypto.h"

#include <functional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace business {

namespace {

constexpr const char* kPrefix = "pbkdf2-sha256";

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (const char ch : text) {
        if (ch == separator) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

std::string PasswordHasher::hash(const std::string& password) {
    return hashWithIterations(password, kDefaultIterations);
}

std::string PasswordHasher::hashWithIterations(const std::string& password, std::uint32_t iterations) {
    if (iterations == 0) {
        iterations = kDefaultIterations;
    }
    const auto salt = base::randomBytes(kSaltBytes);
    const auto key = base::pbkdf2HmacSha256(password, salt, iterations, kKeyBytes);

    std::ostringstream out;
    out << kPrefix << '$' << iterations << '$' << base::base64Encode(salt) << '$'
        << base::base64Encode(key);
    return out.str();
}

bool PasswordHasher::verify(const std::string& password, const std::string& stored_hash, bool& needs_rehash) {
    needs_rehash = false;

    if (isLegacyHash(stored_hash)) {
        const bool matched = base::constantTimeEquals(legacyHash(password), stored_hash);
        needs_rehash = matched;  // 校验通过就顺手升级成 PBKDF2
        return matched;
    }

    const auto parts = split(stored_hash, '$');
    if (parts.size() != 4 || parts[0] != kPrefix) {
        return false;
    }

    std::uint32_t iterations = 0;
    try {
        iterations = static_cast<std::uint32_t>(std::stoul(parts[1]));
    } catch (const std::exception&) {
        return false;
    }

    const auto salt = base::base64Decode(parts[2]);
    if (!salt.has_value()) {
        return false;
    }

    const auto key = base::pbkdf2HmacSha256(password, *salt, iterations, kKeyBytes);
    const bool matched = base::constantTimeEquals(base::base64Encode(key), parts[3]);
    needs_rehash = matched && iterations < kDefaultIterations;
    return matched;
}

bool PasswordHasher::isLegacyHash(const std::string& stored_hash) {
    return stored_hash.rfind(kPrefix, 0) != 0;
}

std::string PasswordHasher::legacyHash(const std::string& password) {
    // 与改造前的实现保持一致，保证老用户还能登录并触发升级。
    const auto value = std::hash<std::string>{}(std::string("reactor-user-service:") + password);
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string PasswordHasher::generatePassword(std::size_t length) {
    static constexpr char kAlphabet[] =
        "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    constexpr std::size_t kAlphabetSize = sizeof(kAlphabet) - 1;

    if (length == 0) {
        length = 12;
    }
    // 拒绝采样消除取模偏差，保证生成的口令分布均匀。
    const auto random = base::randomBytes(length * 2 + 8);
    std::string password;
    password.reserve(length);
    for (const auto byte : random) {
        if (byte >= 256 - (256 % kAlphabetSize)) {
            continue;
        }
        password.push_back(kAlphabet[byte % kAlphabetSize]);
        if (password.size() == length) {
            break;
        }
    }
    while (password.size() < length) {
        password.push_back(kAlphabet[random[password.size() % random.size()] % kAlphabetSize]);
    }
    return password;
}

}  // namespace business
