#include "Security.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace {
constexpr std::uint32_t ROUND_CONSTANTS[64] = {
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

char hexDigit(unsigned int value) {
    return value < 10U
        ? static_cast<char>('0' + value)
        : static_cast<char>('a' + (value - 10U));
}
}

Sha256::Sha256()
    : state_{}, block_{}, blockLength_(0), totalLength_(0) {
    reset();
}

void Sha256::reset() {
    state_[0] = 0x6a09e667U;
    state_[1] = 0xbb67ae85U;
    state_[2] = 0x3c6ef372U;
    state_[3] = 0xa54ff53aU;
    state_[4] = 0x510e527fU;
    state_[5] = 0x9b05688cU;
    state_[6] = 0x1f83d9abU;
    state_[7] = 0x5be0cd19U;
    blockLength_ = 0;
    totalLength_ = 0;
    for (unsigned char& value : block_) value = 0;
}

std::uint32_t Sha256::rotateRight(std::uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32U - count));
}

std::uint32_t Sha256::choose(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t z
) {
    return (x & y) ^ (~x & z);
}

std::uint32_t Sha256::majority(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t z
) {
    return (x & y) ^ (x & z) ^ (y & z);
}

std::uint32_t Sha256::bigSigma0(std::uint32_t value) {
    return rotateRight(value, 2U) ^ rotateRight(value, 13U) ^ rotateRight(value, 22U);
}

std::uint32_t Sha256::bigSigma1(std::uint32_t value) {
    return rotateRight(value, 6U) ^ rotateRight(value, 11U) ^ rotateRight(value, 25U);
}

std::uint32_t Sha256::smallSigma0(std::uint32_t value) {
    return rotateRight(value, 7U) ^ rotateRight(value, 18U) ^ (value >> 3U);
}

std::uint32_t Sha256::smallSigma1(std::uint32_t value) {
    return rotateRight(value, 17U) ^ rotateRight(value, 19U) ^ (value >> 10U);
}

void Sha256::transform(const unsigned char data[64]) {
    std::uint32_t schedule[64]{};
    for (int index = 0; index < 16; ++index) {
        const int offset = index * 4;
        schedule[index] =
            (static_cast<std::uint32_t>(data[offset]) << 24U) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 16U) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8U) |
            static_cast<std::uint32_t>(data[offset + 3]);
    }
    for (int index = 16; index < 64; ++index) {
        schedule[index] = smallSigma1(schedule[index - 2]) +
                          schedule[index - 7] +
                          smallSigma0(schedule[index - 15]) +
                          schedule[index - 16];
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (int index = 0; index < 64; ++index) {
        const std::uint32_t temporary1 = h + bigSigma1(e) + choose(e, f, g) +
                                         ROUND_CONSTANTS[index] + schedule[index];
        const std::uint32_t temporary2 = bigSigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
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

void Sha256::update(const unsigned char* data, std::size_t length) {
    if (data == nullptr || length == 0) return;
    totalLength_ += static_cast<std::uint64_t>(length);
    for (std::size_t index = 0; index < length; ++index) {
        block_[blockLength_++] = data[index];
        if (blockLength_ == 64) {
            transform(block_);
            blockLength_ = 0;
        }
    }
}

void Sha256::update(const std::string& text) {
    update(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

std::string Sha256::finalHex() {
    const std::uint64_t bitLength = totalLength_ * 8U;
    block_[blockLength_++] = 0x80U;

    if (blockLength_ > 56) {
        while (blockLength_ < 64) block_[blockLength_++] = 0;
        transform(block_);
        blockLength_ = 0;
    }
    while (blockLength_ < 56) block_[blockLength_++] = 0;
    for (int shift = 56; shift >= 0; shift -= 8) {
        block_[blockLength_++] = static_cast<unsigned char>((bitLength >> shift) & 0xffU);
    }
    transform(block_);

    std::string result;
    result.reserve(64);
    for (const std::uint32_t word : state_) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result += hexDigit((word >> shift) & 0x0fU);
        }
    }
    reset();
    return result;
}

std::string Sha256::hash(const std::string& text) {
    Sha256 digest;
    digest.update(text);
    return digest.finalHex();
}

std::string Security::sha256(const std::string& text) {
    return Sha256::hash(text);
}

std::string Security::randomHex(std::size_t byteCount) {
    std::random_device source;
    const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937_64 generator(
        static_cast<std::mt19937_64::result_type>(source()) ^
        static_cast<std::mt19937_64::result_type>(timestamp)
    );
    std::uniform_int_distribution<unsigned int> distribution(0U, 255U);

    std::string result;
    result.reserve(byteCount * 2U);
    for (std::size_t index = 0; index < byteCount; ++index) {
        const unsigned int value = distribution(generator);
        result += hexDigit((value >> 4U) & 0x0fU);
        result += hexDigit(value & 0x0fU);
    }
    return result;
}

bool Security::constantTimeEquals(
    const std::string& left,
    const std::string& right
) {
    const std::size_t maximum = left.size() > right.size() ? left.size() : right.size();
    unsigned int difference = static_cast<unsigned int>(left.size() ^ right.size());
    for (std::size_t index = 0; index < maximum; ++index) {
        const unsigned char leftValue = index < left.size()
            ? static_cast<unsigned char>(left[index])
            : 0U;
        const unsigned char rightValue = index < right.size()
            ? static_cast<unsigned char>(right[index])
            : 0U;
        difference |= static_cast<unsigned int>(leftValue ^ rightValue);
    }
    return difference == 0U;
}

std::string Security::derivePasswordHash(
    const std::string& password,
    const std::string& salt,
    int rounds
) {
    if (rounds < 1) rounds = 1;
    std::string digest = sha256(salt + "|" + password);
    for (int round = 1; round < rounds; ++round) {
        digest = sha256(salt + "|" + digest + "|" + password);
    }
    return digest;
}
