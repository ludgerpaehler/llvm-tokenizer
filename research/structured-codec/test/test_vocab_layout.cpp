// CTest target: vocab layout offsets are contiguous, monotonic, and round-trip
// through the encode/decode helpers.
#include "vocab.h"
#include <cstdio>

#define EXPECT_EQ(a, b)                                                        \
  do {                                                                         \
    auto _a = (a);                                                             \
    auto _b = (b);                                                             \
    if (!(_a == _b)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a,   \
                   #b);                                                        \
      return 1;                                                                \
    }                                                                          \
  } while (0)

using namespace structured_codec;

int main() {
  const VocabLayout L = VocabLayout::compute();
  // Padding is slot 0. Bytes occupy the next 256 ids. Then tags, opcodes,
  // typekinds. Ranges are contiguous (no holes) and end below 2^32.
  EXPECT_EQ(L.padding, 0u);
  EXPECT_EQ(L.bytes_begin, 1u);
  EXPECT_EQ(L.tags_begin, L.bytes_begin + 256u);
  EXPECT_EQ(L.opcodes_begin, L.tags_begin + tagCount());
  EXPECT_EQ(L.typekinds_begin, L.opcodes_begin + opcodeCount());
  EXPECT_EQ(L.end, L.typekinds_begin + typeKindCount());

  // Encode/decode helpers round-trip for the values used in M0.
  for (uint8_t b = 0; b < 32; ++b) {
    uint32_t t = encodeByte(b);
    EXPECT_EQ(decodeByte(t), b);
  }
  EXPECT_EQ(decodeTag(encodeTag(Tag::MODULE_BEGIN)), Tag::MODULE_BEGIN);
  EXPECT_EQ(decodeTag(encodeTag(Tag::INSTR)), Tag::INSTR);
  EXPECT_EQ(decodeTypeKind(encodeTypeKind(TypeKind::INTEGER)),
            TypeKind::INTEGER);
  // Add (LLVM Instruction::Add == 13) and Ret (1) round-trip as opcode tokens.
  EXPECT_EQ(decodeOpcode(encodeOpcode(13u)), 13u);
  EXPECT_EQ(decodeOpcode(encodeOpcode(1u)), 1u);
  return 0;
}
