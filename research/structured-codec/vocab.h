#pragma once
// Track B vocabulary: token id layout + LEB128 byte-varint helpers.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace structured_codec {

// Unsigned LEB128 over raw bytes. The encoder pushes 7-bit groups (low-bit
// first) with the high bit set on all but the last byte. Used as the carrier
// for varints inside the token stream's byte range (see Task 3).
void encodeULEB128(std::vector<uint8_t> &out, uint64_t value);

// Reads one ULEB128 value starting at `pos`, advancing `pos` past it.
// Behavior is undefined on malformed/truncated input (M0 callers always pair
// encode/decode).
uint64_t decodeULEB128(const std::vector<uint8_t> &in, size_t &pos);

} // namespace structured_codec
