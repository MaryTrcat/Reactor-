#include "base/Crypto.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string toHex(const std::vector<std::uint8_t>& data) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (const auto byte : data) {
        out.push_back(kDigits[(byte >> 4) & 0x0f]);
        out.push_back(kDigits[byte & 0x0f]);
    }
    return out;
}

void testSha256Vectors() {
    base::Sha256 hasher;
    const std::string input = "abc";
    hasher.update(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    const auto digest = hasher.digest();
    assert(toHex(std::vector<std::uint8_t>(digest.begin(), digest.end())) ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    base::Sha256 empty;
    const auto empty_digest = empty.digest();
    assert(toHex(std::vector<std::uint8_t>(empty_digest.begin(), empty_digest.end())) ==
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

void testPbkdf2Vectors() {
    // RFC 7914 / 常见 PBKDF2-HMAC-SHA256 测试向量
    const std::vector<std::uint8_t> salt{'s', 'a', 'l', 't'};

    const auto one = base::pbkdf2HmacSha256("password", salt, 1, 32);
    assert(toHex(one) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");

    const auto two = base::pbkdf2HmacSha256("password", salt, 2, 32);
    assert(toHex(two) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
}

void testBase64RoundTrip() {
    const std::vector<std::uint8_t> data{1, 2, 3, 250, 251, 252, 0, 42};
    const auto encoded = base::base64Encode(data);
    const auto decoded = base::base64Decode(encoded);
    assert(decoded.has_value());
    assert(*decoded == data);

    // 覆盖 padding 的三种情况
    assert(base::base64Encode({0x66}) == "Zg==");
    assert(base::base64Encode({0x66, 0x6f}) == "Zm8=");
    assert(base::base64Encode({0x66, 0x6f, 0x6f}) == "Zm9v");
}

void testConstantTimeEquals() {
    assert(base::constantTimeEquals("abc", "abc"));
    assert(!base::constantTimeEquals("abc", "abd"));
    assert(!base::constantTimeEquals("abc", "ab"));
    assert(base::constantTimeEquals("", ""));
}

void testRandomBytes() {
    const auto first = base::randomBytes(16);
    const auto second = base::randomBytes(16);
    assert(first.size() == 16);
    assert(first != second);
}

}  // namespace

int main() {
    testSha256Vectors();
    testPbkdf2Vectors();
    testBase64RoundTrip();
    testConstantTimeEquals();
    testRandomBytes();
    std::cout << "test_crypto passed\n";
    return 0;
}
