// SPDX-License-Identifier: MIT

#include "jr800/formats/sha256.hpp"

#include <bit>
#include <limits>
#include <stdexcept>
#include <vector>

namespace jr800::formats {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
};

}  // namespace

Sha256Digest sha256(std::span<const std::uint8_t> input) {
    static_assert(
        std::numeric_limits<std::size_t>::digits
            <= std::numeric_limits<std::uint64_t>::digits
    );
    const auto input_size = static_cast<std::uint64_t>(input.size());
    if (input_size > std::numeric_limits<std::uint64_t>::max() / 8U) {
        throw std::length_error("SHA-256 input is too large");
    }

    std::vector<std::uint8_t> message(input.begin(), input.end());
    const auto bit_length = input_size * 8U;
    message.push_back(0x80U);
    while (message.size() % 64U != 56U) {
        message.push_back(0U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xFFU));
    }

    std::array<std::uint32_t, 8> hash{
        0x6A09E667U,
        0xBB67AE85U,
        0x3C6EF372U,
        0xA54FF53AU,
        0x510E527FU,
        0x9B05688CU,
        0x1F83D9ABU,
        0x5BE0CD19U,
    };

    for (std::size_t block = 0; block < message.size(); block += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            const auto offset = block + index * 4U;
            words[index] = (static_cast<std::uint32_t>(message[offset]) << 24U)
                | (static_cast<std::uint32_t>(message[offset + 1U]) << 16U)
                | (static_cast<std::uint32_t>(message[offset + 2U]) << 8U)
                | static_cast<std::uint32_t>(message[offset + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto sigma0 = std::rotr(words[index - 15U], 7)
                ^ std::rotr(words[index - 15U], 18)
                ^ (words[index - 15U] >> 3U);
            const auto sigma1 = std::rotr(words[index - 2U], 17)
                ^ std::rotr(words[index - 2U], 19)
                ^ (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
        }

        auto a = hash[0];
        auto b = hash[1];
        auto c = hash[2];
        auto d = hash[3];
        auto e = hash[4];
        auto f = hash[5];
        auto g = hash[6];
        auto h = hash[7];

        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto choose = (e & f) ^ (~e & g);
            const auto temporary1 = h + sum1 + choose + kRoundConstants[index] + words[index];
            const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary2 = sum0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    std::array<std::uint8_t, 32> result{};
    for (std::size_t index = 0; index < hash.size(); ++index) {
        result[index * 4U] = static_cast<std::uint8_t>((hash[index] >> 24U) & 0xFFU);
        result[index * 4U + 1U] = static_cast<std::uint8_t>((hash[index] >> 16U) & 0xFFU);
        result[index * 4U + 2U] = static_cast<std::uint8_t>((hash[index] >> 8U) & 0xFFU);
        result[index * 4U + 3U] = static_cast<std::uint8_t>(hash[index] & 0xFFU);
    }
    return result;
}

}  // namespace jr800::formats
