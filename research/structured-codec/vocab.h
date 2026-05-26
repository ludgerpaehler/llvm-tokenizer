#pragma once
// Track B vocabulary: token id layout, Tag/TypeKind enums, encode/decode
// helpers, and LEB128 byte-varint helpers.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace structured_codec {

// ------- Record-kind tags (M0 subset) -------
//
// Tags are added as later milestones widen the schema; their numeric order is
// the on-wire order, so DO NOT renumber existing entries.
enum class Tag : uint32_t {
  MODULE_BEGIN = 0,
  MODULE_END,
  TYPEDEF,
  FUNC_BEGIN,
  ARG,
  BLOCK_BEGIN,
  INSTR,
  FUNC_END,
  REF,
  NAME,
  kCount, // sentinel, not on the wire
};
inline constexpr uint32_t tagCount() {
  return static_cast<uint32_t>(Tag::kCount);
}

// ------- Type kinds (M0 subset; widens in M1) -------
enum class TypeKind : uint32_t {
  INTEGER = 0,
  VOID,
  FUNCTION,
  // M1+: HALF, FLOAT, DOUBLE, FP128, X86_FP80, PPC_FP128, POINTER, ARRAY,
  // VECTOR, STRUCT, ...
  kCount,
};
inline constexpr uint32_t typeKindCount() {
  return static_cast<uint32_t>(TypeKind::kCount);
}

// Opcode count comes from LLVM enums; defined in vocab.cpp.
uint32_t opcodeCount();

// ------- Token id layout -------
//
// Contiguous ranges, computed once via VocabLayout::compute():
//   0                : PADDING (reserved, currently unused)
//   1..256           : BYTES (raw byte b -> id (1 + b))
//   tags_begin..     : TAGS
//   opcodes_begin..  : OPCODES (LLVM Instruction opcodes, 1-based; stored
//                     0-based per the lossy tool's hardening)
//   typekinds_begin..: TYPEKINDS
//   end              : one past the last id used
struct VocabLayout {
  uint32_t padding;
  uint32_t bytes_begin;
  uint32_t tags_begin;
  uint32_t opcodes_begin;
  uint32_t typekinds_begin;
  uint32_t end;
  static VocabLayout compute();
};

// Singleton accessor — computed once, then cheap to copy/read.
const VocabLayout &layout();

// ------- Token encode/decode helpers -------
uint32_t encodeByte(uint8_t b);
uint8_t  decodeByte(uint32_t token);
uint32_t encodeTag(Tag t);
Tag      decodeTag(uint32_t token);
uint32_t encodeOpcode(uint32_t llvm_opcode); // 1..opcodeCount
uint32_t decodeOpcode(uint32_t token);
uint32_t encodeTypeKind(TypeKind k);
TypeKind decodeTypeKind(uint32_t token);

// ------- ULEB128 over raw bytes (see Task 2) -------
void encodeULEB128(std::vector<uint8_t> &out, uint64_t value);
uint64_t decodeULEB128(const std::vector<uint8_t> &in, size_t &pos);

// Helpers that pack/unpack ULEB128 directly into a token stream (appends byte
// tokens). Used by encoder/decoder for varints (value indices, name lengths).
void emitVarint(std::vector<uint32_t> &out, uint64_t value);
uint64_t readVarint(const std::vector<uint32_t> &in, size_t &pos);

} // namespace structured_codec
