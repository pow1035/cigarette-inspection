#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace cigvision {
namespace detail {

inline std::uint32_t sha256RotateRight(std::uint32_t value, std::uint32_t count)
{
    return (value >> count) | (value << (32U - count));
}

inline void sha256Transform(const std::uint8_t* block, std::uint32_t state[8])
{
    static const std::uint32_t constants[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    std::uint32_t words[64] = {};
    for (std::size_t index = 0; index < 16U; ++index) {
        const std::size_t offset = index * 4U;
        words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
            (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
            (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
            static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < 64U; ++index) {
        const std::uint32_t s0 = sha256RotateRight(words[index - 15U], 7U) ^
            sha256RotateRight(words[index - 15U], 18U) ^
            (words[index - 15U] >> 3U);
        const std::uint32_t s1 = sha256RotateRight(words[index - 2U], 17U) ^
            sha256RotateRight(words[index - 2U], 19U) ^
            (words[index - 2U] >> 10U);
        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];
    for (std::size_t index = 0; index < 64U; ++index) {
        const std::uint32_t sum1 = sha256RotateRight(e, 6U) ^
            sha256RotateRight(e, 11U) ^ sha256RotateRight(e, 25U);
        const std::uint32_t choose = (e & f) ^ ((~e) & g);
        const std::uint32_t temporary1 = h + sum1 + choose +
            constants[index] + words[index];
        const std::uint32_t sum0 = sha256RotateRight(a, 2U) ^
            sha256RotateRight(a, 13U) ^ sha256RotateRight(a, 22U);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temporary2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

} // namespace detail

inline bool isSha256Hex(const std::string& value)
{
    if (value.size() != 64U) {
        return false;
    }
    for (const char character : value) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        const bool upper = character >= 'A' && character <= 'F';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return true;
}

inline std::string sha256Hex(const std::string& input)
{
    if (input.size() > (std::numeric_limits<std::uint64_t>::max)() / 8U) {
        return std::string();
    }
    std::vector<std::uint8_t> message(input.begin(), input.end());
    message.push_back(0x80U);
    while (message.size() % 64U != 56U) {
        message.push_back(0U);
    }
    const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>(bitLength >>
            static_cast<std::uint32_t>(shift)));
    }

    std::uint32_t state[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    for (std::size_t offset = 0; offset < message.size(); offset += 64U) {
        detail::sha256Transform(&message[offset], state);
    }

    static const char digits[] = "0123456789abcdef";
    std::string result(64U, '0');
    for (std::size_t index = 0; index < 8U; ++index) {
        for (std::size_t byte = 0; byte < 4U; ++byte) {
            const std::uint32_t shift = static_cast<std::uint32_t>((3U - byte) * 8U);
            const std::uint8_t value = static_cast<std::uint8_t>(state[index] >> shift);
            const std::size_t output = index * 8U + byte * 2U;
            result[output] = digits[value >> 4U];
            result[output + 1U] = digits[value & 0x0fU];
        }
    }
    return result;
}

} // namespace cigvision
