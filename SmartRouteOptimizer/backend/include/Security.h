#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class Sha256 {
public:
    Sha256();

    void reset();
    void update(const unsigned char* data, std::size_t length);
    void update(const std::string& text);
    std::string finalHex();

    static std::string hash(const std::string& text);

private:
    std::uint32_t state_[8];
    unsigned char block_[64];
    std::size_t blockLength_;
    std::uint64_t totalLength_;

    void transform(const unsigned char block[64]);
    static std::uint32_t rotateRight(std::uint32_t value, unsigned int count);
    static std::uint32_t choose(std::uint32_t x, std::uint32_t y, std::uint32_t z);
    static std::uint32_t majority(std::uint32_t x, std::uint32_t y, std::uint32_t z);
    static std::uint32_t bigSigma0(std::uint32_t value);
    static std::uint32_t bigSigma1(std::uint32_t value);
    static std::uint32_t smallSigma0(std::uint32_t value);
    static std::uint32_t smallSigma1(std::uint32_t value);
};

namespace Security {
std::string sha256(const std::string& text);
std::string randomHex(std::size_t byteCount);
bool constantTimeEquals(const std::string& left, const std::string& right);
std::string derivePasswordHash(
    const std::string& password,
    const std::string& salt,
    int rounds = 2048
);
}
