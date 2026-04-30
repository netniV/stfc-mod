#include "sha256.h"

#include <array>
#include <bit>
#include <cstddef>
#include <iomanip>
#include <sstream>

namespace sha256
{
namespace
{
constexpr std::array<uint32_t, 64> k{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

constexpr uint32_t choose(uint32_t x, uint32_t y, uint32_t z)
{
  return (x & y) ^ (~x & z);
}

constexpr uint32_t majority(uint32_t x, uint32_t y, uint32_t z)
{
  return (x & y) ^ (x & z) ^ (y & z);
}

constexpr uint32_t big_sigma0(uint32_t x)
{
  return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

constexpr uint32_t big_sigma1(uint32_t x)
{
  return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}

constexpr uint32_t small_sigma0(uint32_t x)
{
  return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
}

constexpr uint32_t small_sigma1(uint32_t x)
{
  return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
}

uint32_t read_be32(const uint8_t* p)
{
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void write_be32(uint8_t* p, uint32_t value)
{
  p[0] = static_cast<uint8_t>(value >> 24);
  p[1] = static_cast<uint8_t>(value >> 16);
  p[2] = static_cast<uint8_t>(value >> 8);
  p[3] = static_cast<uint8_t>(value);
}

void write_be64(uint8_t* p, uint64_t value)
{
  for (int i = 0; i < 8; ++i) {
    p[7 - i] = static_cast<uint8_t>(value >> (i * 8));
  }
}

void transform(std::array<uint32_t, 8>& h, const uint8_t* block)
{
  std::array<uint32_t, 64> w{};
  for (int i = 0; i < 16; ++i) {
    w[i] = read_be32(block + i * 4);
  }
  for (int i = 16; i < 64; ++i) {
    w[i] = small_sigma1(w[i - 2]) + w[i - 7] + small_sigma0(w[i - 15]) + w[i - 16];
  }

  uint32_t a = h[0];
  uint32_t b = h[1];
  uint32_t c = h[2];
  uint32_t d = h[3];
  uint32_t e = h[4];
  uint32_t f = h[5];
  uint32_t g = h[6];
  uint32_t hh = h[7];

  for (int i = 0; i < 64; ++i) {
    const uint32_t t1 = hh + big_sigma1(e) + choose(e, f, g) + k[i] + w[i];
    const uint32_t t2 = big_sigma0(a) + majority(a, b, c);
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  h[0] += a;
  h[1] += b;
  h[2] += c;
  h[3] += d;
  h[4] += e;
  h[5] += f;
  h[6] += g;
  h[7] += hh;
}
} // namespace

digest_t digest(std::string_view data)
{
  std::array<uint32_t, 8> h{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

  const uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8U;
  const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
  size_t offset = 0;

  for (; offset + 64 <= data.size(); offset += 64) {
    transform(h, bytes + offset);
  }

  std::array<uint8_t, 128> tail{};
  size_t tail_size = data.size() - offset;
  for (size_t i = 0; i < tail_size; ++i) {
    tail[i] = bytes[offset + i];
  }
  tail[tail_size++] = 0x80U;

  if (tail_size > 56) {
    transform(h, tail.data());
    tail.fill(0);
  }

  write_be64(tail.data() + 56, bit_len);
  transform(h, tail.data());

  digest_t out{};
  for (int i = 0; i < 8; ++i) {
    write_be32(out.data() + i * 4, h[i]);
  }
  return out;
}

std::string hex(const digest_t& digest)
{
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    out << std::setw(2) << static_cast<int>(byte);
  }
  return out.str();
}

std::string hex(std::string_view data)
{
  return hex(digest(data));
}
} // namespace sha256
