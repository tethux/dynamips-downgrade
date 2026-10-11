#include <zlib.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

#include <dynamips/macros.h>

cfn crc_init() -> void;
cfn crc_test_legacy(std::uint32_t accumulator, std::uint8_t *data, int length)
    -> std::uint32_t;

#ifdef CRC_TEST_ZLIB
cfn crc32_compute(std::uint32_t accumulator, std::uint8_t *data, int length)
    -> std::uint32_t;
#endif

namespace {

fn compute(std::uint32_t accumulator, std::uint8_t *data, int length)
    -> std::uint32_t {
#ifdef CRC_TEST_ZLIB
  return crc32_compute(accumulator, data, length);
#else
  return crc_test_legacy(accumulator, data, length);
#endif
}

fn check(std::uint32_t accumulator, std::uint8_t *data, int length) -> bool {
  const let original = crc_test_legacy(accumulator, data, length);
  // Empty input complements the raw accumulator, including legacy len < 0.
  const let expected =
      length <= 0 ? ~accumulator
                  : static_cast<std::uint32_t>(crc32_z(
                        ~accumulator, data, static_cast<z_size_t>(length)));
  const let actual = compute(accumulator, data, length);
  if (actual == original && actual == expected)
    return true;

  std::fprintf(
      stderr, "FAIL: seed=%08x length=%d actual=%08x legacy=%08x zlib=%08x\n",
      static_cast<unsigned>(accumulator), length, static_cast<unsigned>(actual),
      static_cast<unsigned>(original), static_cast<unsigned>(expected));
  return false;
}

} // namespace

fn main() -> int {
  crc_init();
  bool ok = true;
  unsigned checks = 0;

  std::array<std::uint8_t, 9> known{'1', '2', '3', '4', '5',
                                    '6', '7', '8', '9'};
  if (compute(0xffffffffU, known.data(), known.size()) != 0xcbf43926U) {
    std::fputs("FAIL: standard 123456789 CRC-32 vector\n", stderr);
    ok = false;
  }

  // A minimum-sized Ethernet ARP request, excluding its FCS.
  std::array<std::uint8_t, 60> ethernet{
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x02, 0x01, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x02, 0x02};
  ok &= check(0xffffffffU, ethernet.data(), ethernet.size());
  ++checks;

  std::array<std::uint8_t, 65536> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>((index * 37 + index / 256) & 0xff);

  constexpr std::array<std::uint32_t, 4> seeds{0, 0xffffffffU, 0x12345678U,
                                               0x80000000U};
  constexpr std::array lengths{0,    1,    2,    3,    4,     7,    8,    9,
                               15,   16,   17,   31,   32,    33,   44,   48,
                               63,   64,   65,   255,  256,   257,  1499, 1500,
                               1501, 4095, 4096, 4097, 65535, 65536};
  for (const let seed : seeds) {
    ok &= check(seed, nullptr, 0);
    ok &= check(seed, nullptr, -1);
    checks += 2;
    for (const let length : lengths) {
      ok &= check(seed, bytes.data(), length);
      ++checks;

      const let whole = compute(seed, bytes.data(), length);
      for (const let split : std::array{0, length / 2, length}) {
        const let first = compute(seed, bytes.data(), split);
        const let joined =
            compute(~first, bytes.data() + split, length - split);
        if (joined != whole) {
          std::fprintf(stderr, "FAIL: incremental length=%d split=%d\n", length,
                       split);
          ok = false;
        }
        ++checks;
      }
    }
  }

  constexpr int largeLength = 1 << 20;
  static std::array<std::uint8_t, largeLength + 8> large{};
  std::uint32_t randomState = 0x6d2b79f5U;
  for (unsigned pattern = 0; pattern < 4; ++pattern) {
    for (std::size_t index = 0; index < large.size(); ++index) {
      randomState ^= randomState << 13;
      randomState ^= randomState >> 17;
      randomState ^= randomState << 5;
      switch (pattern) {
      case 0:
        large[index] = 0;
        break;
      case 1:
        large[index] = 0xff;
        break;
      case 2:
        large[index] = index % 2 == 0 ? 0xaa : 0x55;
        break;
      case 3:
        large[index] = static_cast<std::uint8_t>(randomState);
        break;
      }
    }

    for (const let offset : std::array{0, 1, 3, 7}) {
      for (const let seed : seeds) {
        for (const let length :
             std::array{1, 2, 3, 31, 32, 33, 1500, 65537, largeLength - 1,
                        largeLength, largeLength + 1}) {
          ok &= check(seed, large.data() + offset, length);
          ++checks;
        }
      }
    }

    // Uneven chunks exercise repeated conversion between raw and completed
    // CRCs.
    std::uint32_t chained = 0;
    for (int offset = 0; offset < largeLength;) {
      const let remaining = largeLength - offset;
      const let length = remaining < 4093 ? remaining : 4093;
      chained = compute(~chained, large.data() + offset, length);
      offset += length;
    }
    if (chained != crc_test_legacy(0xffffffffU, large.data(), largeLength)) {
      std::fprintf(stderr, "FAIL: multi-chunk pattern=%u length=%d\n", pattern,
                   largeLength);
      ok = false;
    }
    ++checks;
  }

  if (!ok)
    return 1;

  std::printf("%s: %u compatibility checks passed\n", CRC_IMPLEMENTATION,
              checks + 1);

  using clock = std::chrono::steady_clock;
  constexpr unsigned iterations = 20000;
  std::uint32_t checksum = 0;
  const let start = clock::now();
  for (unsigned iteration = 0; iteration < iterations; ++iteration)
    checksum = compute(~checksum, bytes.data(), 1500);
  const let elapsed = clock::now() - start;
  const let nanoseconds =
      std::chrono::duration<double, std::nano>(elapsed).count() / iterations;
  std::printf("%s: 1500-byte chained blocks %.1f ns/call checksum=%08x\n",
              CRC_IMPLEMENTATION, nanoseconds, static_cast<unsigned>(checksum));
  std::puts("PASS");
  return 0;
}
