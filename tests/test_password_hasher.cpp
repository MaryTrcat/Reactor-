#include "business/PasswordHasher.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    const std::string password = "s3cret-password";

    const auto stored = business::PasswordHasher::hash(password);
    assert(stored.rfind("pbkdf2-sha256$", 0) == 0);
    // 同一密码两次哈希必须不同（随机盐）
    assert(stored != business::PasswordHasher::hash(password));

    bool needs_rehash = true;
    assert(business::PasswordHasher::verify(password, stored, needs_rehash));
    assert(!needs_rehash);

    assert(!business::PasswordHasher::verify("wrong-password", stored, needs_rehash));
    assert(!business::PasswordHasher::verify(password, "garbage", needs_rehash));
    assert(!business::PasswordHasher::verify(password, "pbkdf2-sha256$abc$def$ghi", needs_rehash));

    // 低迭代次数的历史数据：校验通过但要标记为待升级
    const auto weak = business::PasswordHasher::hashWithIterations(password, 1000);
    needs_rehash = false;
    assert(business::PasswordHasher::verify(password, weak, needs_rehash));
    assert(needs_rehash);

    // 老格式（std::hash）识别与兼容
    const auto legacy = business::PasswordHasher::legacyHash(password);
    assert(business::PasswordHasher::isLegacyHash(legacy));
    assert(!business::PasswordHasher::isLegacyHash(stored));
    needs_rehash = false;
    assert(business::PasswordHasher::verify(password, legacy, needs_rehash));
    assert(needs_rehash);

    // 随机初始密码：长度正确、只含安全字符、两次不同
    const auto generated = business::PasswordHasher::generatePassword(16);
    assert(generated.size() == 16);
    assert(generated != business::PasswordHasher::generatePassword(16));
    for (const char ch : generated) {
        const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        assert(safe);
    }
    // 排除了容易混淆的 0/O/1/l/I 之后仍然至少要有 56 个可用字符
    assert(generated.find('0') == std::string::npos);
    assert(generated.find('O') == std::string::npos);
    assert(generated.find('l') == std::string::npos);

    std::cout << "test_password_hasher passed\n";
    return 0;
}
