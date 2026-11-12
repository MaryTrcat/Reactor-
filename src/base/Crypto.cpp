#include "base/Crypto.h"

#include <algorithm>
#include <cstring>
#include <random>

namespace base {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline std::uint32_t rotateRight(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

}  // namespace

Sha256::Sha256() {
    state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
}

void Sha256::processBlock(const std::uint8_t* block) {
    std::array<std::uint32_t, 64> schedule{};
    for (int i = 0; i < 16; ++i) {
        schedule[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                      (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                      (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                      static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const auto s0 = rotateRight(schedule[i - 15], 7) ^ rotateRight(schedule[i - 15], 18) ^ (schedule[i - 15] >> 3);
        const auto s1 = rotateRight(schedule[i - 2], 17) ^ rotateRight(schedule[i - 2], 19) ^ (schedule[i - 2] >> 10);
        schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];

    for (int i = 0; i < 64; ++i) {
        const auto s1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        const auto ch = (e & f) ^ ((~e) & g);
        const auto temp1 = h + s1 + ch + kRoundConstants[i] + schedule[i];
        const auto s0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        const auto maj = (a & b) ^ (a & c) ^ (b & c);
        const auto temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return;
    }
    total_size_ += size;

    // 先把上次残留的不足 64 字节拼满
    if (buffer_size_ > 0) {
        const auto need = 64 - buffer_size_;
        const auto take = std::min(need, size);
        std::memcpy(buffer_.data() + buffer_size_, data, take);
        buffer_size_ += take;
        data += take;
        size -= take;
        if (buffer_size_ == 64) {
            processBlock(buffer_.data());
            buffer_size_ = 0;
        }
    }

    while (size >= 64) {
        processBlock(data);
        data += 64;
        size -= 64;
    }

    if (size > 0) {
        std::memcpy(buffer_.data(), data, size);
        buffer_size_ = size;
    }
}

std::array<std::uint8_t, 32> Sha256::digest() {
    const std::uint64_t bit_length = total_size_ * 8;

    std::uint8_t padding[8];
    for (int i = 0; i < 8; ++i) {
        padding[i] = static_cast<std::uint8_t>((bit_length >> (56 - i * 8)) & 0xff);
    }

    const std::uint8_t marker = 0x80;
    update(&marker, 1);

    const std::uint8_t zero = 0x00;
    while (buffer_size_ != 56) {
        update(&zero, 1);
    }
    update(padding, 8);

    std::array<std::uint8_t, 32> output{};
    for (int i = 0; i < 8; ++i) {
        output[i * 4] = static_cast<std::uint8_t>((state_[i] >> 24) & 0xff);
        output[i * 4 + 1] = static_cast<std::uint8_t>((state_[i] >> 16) & 0xff);
        output[i * 4 + 2] = static_cast<std::uint8_t>((state_[i] >> 8) & 0xff);
        output[i * 4 + 3] = static_cast<std::uint8_t>(state_[i] & 0xff);
    }
    return output;
}

std::array<std::uint8_t, 32> hmacSha256(const std::vector<std::uint8_t>& key,
                                        const std::uint8_t* data,
                                        std::size_t size) {
    std::array<std::uint8_t, 64> normalized{};
    if (key.size() > 64) {
        Sha256 hasher;
        hasher.update(key.data(), key.size());
        const auto hashed = hasher.digest();
        std::copy(hashed.begin(), hashed.end(), normalized.begin());
    } else {
        std::copy(key.begin(), key.end(), normalized.begin());
    }

    std::array<std::uint8_t, 64> inner_pad{};
    std::array<std::uint8_t, 64> outer_pad{};
    for (std::size_t i = 0; i < 64; ++i) {
        inner_pad[i] = static_cast<std::uint8_t>(normalized[i] ^ 0x36);
        outer_pad[i] = static_cast<std::uint8_t>(normalized[i] ^ 0x5c);
    }

    Sha256 inner;
    inner.update(inner_pad.data(), inner_pad.size());
    inner.update(data, size);
    const auto inner_digest = inner.digest();

    Sha256 outer;
    outer.update(outer_pad.data(), outer_pad.size());
    outer.update(inner_digest.data(), inner_digest.size());
    return outer.digest();
}

std::vector<std::uint8_t> pbkdf2HmacSha256(const std::string& password,
                                           const std::vector<std::uint8_t>& salt,
                                           std::uint32_t iterations,
                                           std::size_t key_size) {
    if (iterations == 0) {
        iterations = 1;
    }
    const std::vector<std::uint8_t> key(password.begin(), password.end());

    std::vector<std::uint8_t> output;
    output.reserve(key_size);

    std::uint32_t block_index = 1;
    while (output.size() < key_size) {
        std::vector<std::uint8_t> input(salt);
        input.push_back(static_cast<std::uint8_t>((block_index >> 24) & 0xff));
        input.push_back(static_cast<std::uint8_t>((block_index >> 16) & 0xff));
        input.push_back(static_cast<std::uint8_t>((block_index >> 8) & 0xff));
        input.push_back(static_cast<std::uint8_t>(block_index & 0xff));

        auto u = hmacSha256(key, input.data(), input.size());
        auto block = u;
        for (std::uint32_t round = 1; round < iterations; ++round) {
            u = hmacSha256(key, u.data(), u.size());
            for (std::size_t i = 0; i < block.size(); ++i) {
                block[i] ^= u[i];
            }
        }

        const auto take = std::min(block.size(), key_size - output.size());
        output.insert(output.end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(take));
        ++block_index;
    }
    return output;
}

std::string base64Encode(const std::vector<std::uint8_t>& data) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index + 2 < data.size()) {
        const std::uint32_t chunk = (static_cast<std::uint32_t>(data[index]) << 16) |
                                    (static_cast<std::uint32_t>(data[index + 1]) << 8) |
                                    static_cast<std::uint32_t>(data[index + 2]);
        out.push_back(kAlphabet[(chunk >> 18) & 0x3f]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3f]);
        out.push_back(kAlphabet[(chunk >> 6) & 0x3f]);
        out.push_back(kAlphabet[chunk & 0x3f]);
        index += 3;
    }

    const auto remaining = data.size() - index;
    if (remaining == 1) {
        const std::uint32_t chunk = static_cast<std::uint32_t>(data[index]) << 16;
        out.push_back(kAlphabet[(chunk >> 18) & 0x3f]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3f]);
        out.append("==");
    } else if (remaining == 2) {
        const std::uint32_t chunk = (static_cast<std::uint32_t>(data[index]) << 16) |
                                    (static_cast<std::uint32_t>(data[index + 1]) << 8);
        out.push_back(kAlphabet[(chunk >> 18) & 0x3f]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3f]);
        out.push_back(kAlphabet[(chunk >> 6) & 0x3f]);
        out.push_back('=');
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> base64Decode(const std::string& text) {
    auto value_of = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9') return ch - '0' + 52;
        if (ch == '+') return 62;
        if (ch == '/') return 63;
        return -1;
    };

    std::vector<std::uint8_t> out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (const char ch : text) {
        if (ch == '=') {
            break;
        }
        const int value = value_of(ch);
        if (value < 0) {
            if (ch == '\n' || ch == '\r' || ch == ' ') {
                continue;
            }
            return std::nullopt;
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xff));
        }
    }
    return out;
}

std::vector<std::uint8_t> randomBytes(std::size_t size) {
    std::vector<std::uint8_t> out(size);
    // random_device 在 Linux 上读 /dev/urandom，Windows 上用 BCryptGenRandom 的封装。
    std::random_device device;
    std::mt19937_64 engine((static_cast<std::uint64_t>(device()) << 32) ^ device());
    std::uniform_int_distribution<int> distribution(0, 255);
    for (auto& byte : out) {
        byte = static_cast<std::uint8_t>(distribution(engine));
    }
    return out;
}

bool constantTimeEquals(const std::string& left, const std::string& right) {
    const std::size_t max_size = std::max(left.size(), right.size());
    unsigned char diff = static_cast<unsigned char>(left.size() ^ right.size());
    for (std::size_t i = 0; i < max_size; ++i) {
        const auto l = i < left.size() ? static_cast<unsigned char>(left[i]) : 0;
        const auto r = i < right.size() ? static_cast<unsigned char>(right[i]) : 0;
        diff |= static_cast<unsigned char>(l ^ r);
    }
    return diff == 0;
}

}  // namespace base
