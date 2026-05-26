#include "vocab.h"

namespace structured_codec {

void encodeULEB128(std::vector<uint8_t> &out, uint64_t value) {
  do {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if (value != 0) byte |= 0x80; // continuation
    out.push_back(byte);
  } while (value != 0);
}

uint64_t decodeULEB128(const std::vector<uint8_t> &in, size_t &pos) {
  uint64_t result = 0;
  unsigned shift = 0;
  while (true) {
    uint8_t byte = in[pos++];
    result |= uint64_t(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) break;
    shift += 7;
  }
  return result;
}

} // namespace structured_codec

#include <llvm/IR/Instruction.h>

namespace structured_codec {

uint32_t opcodeCount() {
  return static_cast<uint32_t>(llvm::Instruction::OtherOpsEnd) - 1u;
}

VocabLayout VocabLayout::compute() {
  VocabLayout L{};
  L.padding = 0;
  L.bytes_begin = 1;
  L.tags_begin = L.bytes_begin + 256u;
  L.opcodes_begin = L.tags_begin + tagCount();
  L.typekinds_begin = L.opcodes_begin + opcodeCount();
  L.end = L.typekinds_begin + typeKindCount();
  return L;
}

const VocabLayout &layout() {
  static const VocabLayout L = VocabLayout::compute();
  return L;
}

uint32_t encodeByte(uint8_t b) { return layout().bytes_begin + b; }

uint8_t decodeByte(uint32_t token) {
  return static_cast<uint8_t>(token - layout().bytes_begin);
}

uint32_t encodeTag(Tag t) {
  return layout().tags_begin + static_cast<uint32_t>(t);
}

Tag decodeTag(uint32_t token) {
  return static_cast<Tag>(token - layout().tags_begin);
}

// Opcode storage is 0-based (matches the lossy tool's hardening): LLVM
// opcode V in [1, OpcodeCount] -> token id opcodes_begin + (V - 1).
uint32_t encodeOpcode(uint32_t llvm_opcode) {
  return layout().opcodes_begin + (llvm_opcode - 1u);
}

uint32_t decodeOpcode(uint32_t token) {
  return token - layout().opcodes_begin + 1u;
}

uint32_t encodeTypeKind(TypeKind k) {
  return layout().typekinds_begin + static_cast<uint32_t>(k);
}

TypeKind decodeTypeKind(uint32_t token) {
  return static_cast<TypeKind>(token - layout().typekinds_begin);
}

void emitVarint(std::vector<uint32_t> &out, uint64_t value) {
  std::vector<uint8_t> tmp;
  encodeULEB128(tmp, value);
  for (uint8_t b : tmp) out.push_back(encodeByte(b));
}

uint64_t readVarint(const std::vector<uint32_t> &in, size_t &pos) {
  uint64_t result = 0;
  unsigned shift = 0;
  while (true) {
    uint8_t byte = decodeByte(in[pos++]);
    result |= uint64_t(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) break;
    shift += 7;
  }
  return result;
}

} // namespace structured_codec
