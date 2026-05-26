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
