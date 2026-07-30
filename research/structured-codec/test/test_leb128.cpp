// CTest target for LEB128 round-trip. Exits 0 on success, non-zero on failure.
#include "vocab.h"
#include <cstdint>
#include <cstdio>
#include <vector>

#define EXPECT_EQ(a, b)                                                        \
  do {                                                                         \
    auto _a = (a);                                                             \
    auto _b = (b);                                                             \
    if (!(_a == _b)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s != %s (%llu vs %llu)\n", __FILE__,  \
                   __LINE__, #a, #b, (unsigned long long)_a,                   \
                   (unsigned long long)_b);                                    \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int roundTrip(uint64_t v) {
  std::vector<uint8_t> bytes;
  structured_codec::encodeULEB128(bytes, v);
  size_t pos = 0;
  uint64_t got = structured_codec::decodeULEB128(bytes, pos);
  EXPECT_EQ(got, v);
  EXPECT_EQ(pos, bytes.size());
  return 0;
}

int main() {
  for (uint64_t v : {0ull, 1ull, 63ull, 64ull, 127ull, 128ull, 255ull, 256ull,
                     16383ull, 16384ull, 1ull << 32, ~0ull}) {
    if (roundTrip(v)) return 1;
  }
  // Multiple values in one buffer decode at increasing positions.
  std::vector<uint8_t> buf;
  structured_codec::encodeULEB128(buf, 0);
  structured_codec::encodeULEB128(buf, 1234567);
  structured_codec::encodeULEB128(buf, 0xDEADBEEF);
  size_t pos = 0;
  EXPECT_EQ(structured_codec::decodeULEB128(buf, pos), 0ull);
  EXPECT_EQ(structured_codec::decodeULEB128(buf, pos), 1234567ull);
  EXPECT_EQ(structured_codec::decodeULEB128(buf, pos), 0xDEADBEEFull);
  EXPECT_EQ(pos, buf.size());
  return 0;
}
