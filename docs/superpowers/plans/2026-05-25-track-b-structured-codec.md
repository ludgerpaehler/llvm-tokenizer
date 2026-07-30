# Track B (Structured Codec) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build M0 of Track B — a new C++ tool under `research/structured-codec/` that losslessly round-trips `corpus/core/identity-smoke.ll` through the existing harness via a flat `uint32` token stream, with the vocabulary layout and reconstruction scaffolding sized to grow into M1–M5.

**Architecture:** New C++ binary (`structured-codec`) built by its own CMake (LLVM 22 `core/irreader/support`), exposing the harness `encode`/`decode` contract. Token stream is a flat `uint32` sequence in space-separated decimal (same I/O shape as Track C). Vocabulary is layered (`PADDING | BYTES | TAGS | OPCODES | TYPEKINDS`) with arbitrary values (varints, names) riding in the byte range via LEB128. Encoder uses a **definition-order value index** (its own value enumerator) and carries names as literals; decoder rebuilds in the same order into a `vector<Value*>` and resolves refs (forward refs via a fixup list — not exercised in M0).

**Tech Stack:** C++17, CMake (Ninja), LLVM 22 C++ API (`parseIRFile`, `Module`, `IRBuilder`, `verifyModule`), CTest for C++ unit tests, the existing Python harness (`pytest`) for end-to-end round-trip.

**Conventions:** Commit trailer `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`. `private` is the default remote. Format C++ with `clang-format` (LLVM style, `.clang-format`).

**Scope of THIS plan:** **M0 only** (vertical-slice round-trip of `identity-smoke.ll`). M1–M5 are roadmapped at task-group level at the end; each gets its own short follow-on plan once M0 is green.

**Reference:** Spec at `docs/superpowers/specs/2026-05-25-track-b-structured-codec-design.md`. Parent spec at `docs/superpowers/specs/2026-05-25-lossless-tokenizer-design.md`.

---

## File structure (M0)

```
research/structured-codec/
  CMakeLists.txt          new top-level CMake; standalone build under .build/
  .clang-format            symlink to repo root .clang-format (or copy)
  structured_codec.cpp    CLI dispatch (encode/decode)
  vocab.h                 token-id ranges, Tag/TypeKind enums, range helpers
  vocab.cpp               VocabLayout + LEB128 byte-varint helpers
  encoder.h               Encoder class interface
  encoder.cpp             Module -> token stream
  decoder.h               Decoder class interface
  decoder.cpp             token stream -> Module
  README.md               build/run notes
  test/
    CMakeLists.txt
    test_leb128.cpp       LEB128 round-trip unit tests (CTest)
    test_vocab_layout.cpp Vocab layout offset arithmetic unit tests (CTest)
```

Plus one harness change:
- `research/harness/tracks.py` — add `"structured"` entry pointing at the built binary.
- `research/harness/test_roundtrip.py` — add a parameterized round-trip test for `corpus/core/identity-smoke.ll` (only the M0 corpus member) through the `structured` track.

The existing lossy tool's `CMakeLists.txt` is **not** touched; Track B has its own build.

---

## M0 task list

### Task 1: Scaffold the structured-codec project and confirm it builds

**Files:**
- Create: `research/structured-codec/CMakeLists.txt`
- Create: `research/structured-codec/structured_codec.cpp`
- Create: `research/structured-codec/README.md`

- [ ] **Step 1: Create `research/structured-codec/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(structured-codec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "structured-codec: found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif()

# Library that encoder/decoder/tests share.
add_library(structured_codec_lib STATIC
  vocab.cpp
  encoder.cpp
  decoder.cpp
)
llvm_map_components_to_libnames(STRUCTURED_CODEC_LLVM_LIBS support core irreader)
target_link_libraries(structured_codec_lib PUBLIC ${STRUCTURED_CODEC_LLVM_LIBS})

# CLI binary.
add_executable(structured-codec structured_codec.cpp)
target_link_libraries(structured-codec PRIVATE structured_codec_lib)

enable_testing()
add_subdirectory(test)
```

- [ ] **Step 2: Create a minimal CLI stub** — `research/structured-codec/structured_codec.cpp`

```cpp
// Track B CLI. encode|decode subcommand contract matches the harness:
//   structured-codec encode <input.ll> <tokens>
//   structured-codec decode <tokens> <output.ll>
// M0: only `encode` and `decode` are wired (no flags). Subcommand bodies are
// added in later tasks; this file just dispatches.
#include <cstdio>
#include <cstring>
#include <string>

#include "encoder.h"
#include "decoder.h"

static int usage() {
  std::fprintf(stderr,
               "usage: structured-codec encode <in.ll> <tokens>\n"
               "       structured-codec decode <tokens> <out.ll>\n");
  return 2;
}

int main(int argc, char **argv) {
  if (argc != 4) return usage();
  const std::string cmd = argv[1], in = argv[2], out = argv[3];
  if (cmd == "encode") return structured_codec::encodeFile(in, out);
  if (cmd == "decode") return structured_codec::decodeFile(in, out);
  return usage();
}
```

- [ ] **Step 3: Create a placeholder `encoder.h`, `decoder.h`, and empty `encoder.cpp`, `decoder.cpp`, `vocab.h`, `vocab.cpp`** so the project links

`encoder.h`:
```cpp
#pragma once
#include <string>
namespace structured_codec {
// Encode the IR at `in_path` into a token-stream file at `out_path`.
// Returns 0 on success, non-zero on error.
int encodeFile(const std::string &in_path, const std::string &out_path);
} // namespace structured_codec
```

`decoder.h`:
```cpp
#pragma once
#include <string>
namespace structured_codec {
// Decode the token-stream file at `in_path` and write an .ll file to `out_path`.
// Returns 0 on success, non-zero on error.
int decodeFile(const std::string &in_path, const std::string &out_path);
} // namespace structured_codec
```

`encoder.cpp` (stub, just so the symbol exists):
```cpp
#include "encoder.h"
#include <cstdio>
namespace structured_codec {
int encodeFile(const std::string &, const std::string &) {
  std::fprintf(stderr, "encode: not yet implemented\n");
  return 1;
}
} // namespace structured_codec
```

`decoder.cpp` (stub):
```cpp
#include "decoder.h"
#include <cstdio>
namespace structured_codec {
int decodeFile(const std::string &, const std::string &) {
  std::fprintf(stderr, "decode: not yet implemented\n");
  return 1;
}
} // namespace structured_codec
```

`vocab.h` (empty header placeholder for now; filled in Task 2):
```cpp
#pragma once
// Track B vocabulary: token id layout + LEB128 byte-varint helpers.
// Populated in Task 2.
```

`vocab.cpp` (empty):
```cpp
#include "vocab.h"
```

- [ ] **Step 4: Create `research/structured-codec/test/CMakeLists.txt`** — a stub that registers no tests yet (so the parent `add_subdirectory(test)` resolves):

```cmake
# Unit tests are added in later tasks via add_executable + add_test.
```

- [ ] **Step 5: Create `research/structured-codec/README.md`**

```markdown
# structured-codec (Track B)

A lossless LLVM IR codec emitting a flat `uint32` token stream. Plugs into the
research harness as the `structured` track. See
`../../docs/superpowers/specs/2026-05-25-track-b-structured-codec-design.md`.

## Build (out-of-tree)

    cd research/structured-codec
    cmake -G Ninja -S . -B .build -DLLVM_DIR=$(llvm-config-22 --cmakedir)
    cmake --build .build

The `structured-codec` binary lands at `.build/structured-codec`.

## Unit tests

    ctest --test-dir .build --output-on-failure

## End-to-end (via the research harness)

    cd ../
    PYTHONPATH="$PWD/harness:$PWD/text-codec" LLVM_BIN=/usr/lib/llvm-22/bin \
      python3 -m pytest harness -k structured
```

- [ ] **Step 6: Configure and build; verify the binary exists and exits 2 on no args**

Run:
```bash
cd research/structured-codec
cmake -G Ninja -S . -B .build -DLLVM_DIR=$(llvm-config-22 --cmakedir)
cmake --build .build
.build/structured-codec ; echo "exit: $?"
```
Expected: build succeeds, binary at `.build/structured-codec`, running it without args prints usage and exits 2.

Also gitignore the build dir — append to the repo root `.gitignore`:
```
research/structured-codec/.build/
```

- [ ] **Step 7: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec .gitignore
git commit -m "Scaffold Track B structured-codec project (CMake + CLI stub)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 2: LEB128 byte-varint helpers (TDD)

**Files:**
- Modify: `research/structured-codec/vocab.h`
- Modify: `research/structured-codec/vocab.cpp`
- Create: `research/structured-codec/test/test_leb128.cpp`
- Modify: `research/structured-codec/test/CMakeLists.txt`

LEB128 emits an unsigned integer as a sequence of 7-bit groups, low bits first, with the high bit (continuation) set on all but the last byte. This is how arbitrary integers (slot indices, name lengths, constant bit widths) ride in the bounded 256-id byte range.

- [ ] **Step 1: Write the failing unit test** — `research/structured-codec/test/test_leb128.cpp`

```cpp
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
```

- [ ] **Step 2: Register the test in `research/structured-codec/test/CMakeLists.txt`**

```cmake
add_executable(test_leb128 test_leb128.cpp)
target_link_libraries(test_leb128 PRIVATE structured_codec_lib)
add_test(NAME test_leb128 COMMAND test_leb128)
```

- [ ] **Step 3: Build, verify the test FAILS** (linker error: `encodeULEB128`/`decodeULEB128` not defined)

```bash
cd research/structured-codec
cmake --build .build 2>&1 | tail -5
```
Expected: build fails with undefined references to `structured_codec::encodeULEB128` / `decodeULEB128`. (If you previously built the project, you may need `cmake -G Ninja -S . -B .build -DLLVM_DIR=...` to re-pick up the new test files; on most setups Ninja picks up new sources after `cmake` reconfigure.)

- [ ] **Step 4: Implement LEB128 in `vocab.h`** (declarations) and `vocab.cpp` (definitions)

Add to `vocab.h`:
```cpp
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
```

`vocab.cpp`:
```cpp
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
```

- [ ] **Step 5: Build and run the test, verify it PASSES**

```bash
cd research/structured-codec
cmake --build .build
ctest --test-dir .build --output-on-failure
```
Expected: `1/1 Test #1: test_leb128 ... Passed`.

- [ ] **Step 6: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec
git commit -m "Add LEB128 byte-varint helpers with CTest unit tests

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 3: Vocabulary layout — token id ranges, Tag/TypeKind enums (TDD)

**Files:**
- Modify: `research/structured-codec/vocab.h`, `vocab.cpp`
- Create: `research/structured-codec/test/test_vocab_layout.cpp`
- Modify: `research/structured-codec/test/CMakeLists.txt`

The token id space is partitioned into contiguous ranges (same pattern as the lossy tool's `SerializationConfig`). M0 fixes a layout sized for the full design but uses only a subset (`MODULE_*`, `TYPEDEF`, `FUNC_*`, `ARG`, `BLOCK_BEGIN`, `INSTR`, `REF`, `NAME`; one TypeKind, `INTEGER`; opcodes via LLVM enum).

- [ ] **Step 1: Write the failing unit test** — `research/structured-codec/test/test_vocab_layout.cpp`

```cpp
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
```

- [ ] **Step 2: Register the test in `research/structured-codec/test/CMakeLists.txt`**

Append:
```cmake
add_executable(test_vocab_layout test_vocab_layout.cpp)
target_link_libraries(test_vocab_layout PRIVATE structured_codec_lib)
add_test(NAME test_vocab_layout COMMAND test_vocab_layout)
```

- [ ] **Step 3: Build, verify the test FAILS** (missing types: `VocabLayout`, `Tag`, etc.)

```bash
cd research/structured-codec && cmake --build .build 2>&1 | tail -5
```
Expected: compile errors — `VocabLayout`, `Tag`, `TypeKind`, `tagCount`, `opcodeCount`, `typeKindCount`, `encodeByte`, etc. are not declared.

- [ ] **Step 4: Implement the layout** in `vocab.h` (declarations + small inline helpers) and `vocab.cpp` (compute())

Replace the contents of `vocab.h` with:
```cpp
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
  // M1+: HALF, FLOAT, DOUBLE, FP128, X86_FP80, PPC_FP128, POINTER, ARRAY,
  // VECTOR, STRUCT, FUNCTION, ...
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
```

Append to `vocab.cpp` (keep the existing LEB128 functions above; add):
```cpp
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
```

- [ ] **Step 5: Build, run tests, verify both PASS**

```bash
cd research/structured-codec
cmake --build .build
ctest --test-dir .build --output-on-failure
```
Expected: 2/2 tests pass.

- [ ] **Step 6: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec
git commit -m "Add Track B vocabulary layout, tag/typekind enums, token helpers

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 4: Encoder skeleton + CLI wiring (no IR walk yet)

**Files:**
- Modify: `research/structured-codec/encoder.cpp`, `decoder.cpp`

Wire up the file I/O so `structured-codec encode <in.ll> <out>` parses the IR and writes an EMPTY token stream (one byte-token, MODULE_BEGIN, MODULE_END) to disk. `decode` writes an empty module. This is the smallest end-to-end shell before the real IR walk; we'll fail the harness round-trip in Task 9 and TDD our way to green.

- [ ] **Step 1: Implement `encoder.cpp`**

```cpp
#include "encoder.h"
#include "vocab.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace structured_codec {

// Walks a Module and emits the token stream. Filled out across Tasks 5-7.
class Encoder {
public:
  std::vector<uint32_t> encode(llvm::Module &M);
};

std::vector<uint32_t> Encoder::encode(llvm::Module &M) {
  std::vector<uint32_t> tokens;
  tokens.push_back(encodeTag(Tag::MODULE_BEGIN));
  // TODO(M0 task 5): TYPEDEF table, function walk.
  tokens.push_back(encodeTag(Tag::MODULE_END));
  (void)M;
  return tokens;
}

static int writeTokens(const std::vector<uint32_t> &tokens,
                       const std::string &path) {
  std::ofstream f(path);
  if (!f) {
    std::fprintf(stderr, "encode: cannot open output '%s'\n", path.c_str());
    return 1;
  }
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i) f << ' ';
    f << tokens[i];
  }
  // Track C convention: no trailing newline required, but harmless.
  return 0;
}

int encodeFile(const std::string &in_path, const std::string &out_path) {
  llvm::LLVMContext ctx;
  llvm::SMDiagnostic err;
  auto M = llvm::parseIRFile(in_path, err, ctx);
  if (!M) {
    err.print("structured-codec encode", llvm::errs());
    return 1;
  }
  Encoder enc;
  auto tokens = enc.encode(*M);
  return writeTokens(tokens, out_path);
}

} // namespace structured_codec
```

- [ ] **Step 2: Implement `decoder.cpp`** with the same minimal shape — read the token stream, materialize the smallest possible empty `Module`, print it. Real reconstruction lands in Tasks 8–10.

```cpp
#include "decoder.h"
#include "vocab.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace structured_codec {

class Decoder {
public:
  std::unique_ptr<llvm::Module> decode(const std::vector<uint32_t> &tokens,
                                       llvm::LLVMContext &ctx);
};

std::unique_ptr<llvm::Module>
Decoder::decode(const std::vector<uint32_t> &tokens, llvm::LLVMContext &ctx) {
  auto M = std::make_unique<llvm::Module>("structured-codec", ctx);
  size_t pos = 0;
  if (pos >= tokens.size() || decodeTag(tokens[pos++]) != Tag::MODULE_BEGIN) {
    std::fprintf(stderr, "decode: expected MODULE_BEGIN at start\n");
    return nullptr;
  }
  // TODO(M0 task 8+): consume TYPEDEFs, functions, until MODULE_END.
  if (pos >= tokens.size() || decodeTag(tokens[pos++]) != Tag::MODULE_END) {
    std::fprintf(stderr, "decode: expected MODULE_END\n");
    return nullptr;
  }
  return M;
}

static std::vector<uint32_t> readTokens(const std::string &path) {
  std::ifstream f(path);
  std::vector<uint32_t> tokens;
  uint32_t t;
  while (f >> t) tokens.push_back(t);
  return tokens;
}

int decodeFile(const std::string &in_path, const std::string &out_path) {
  llvm::LLVMContext ctx;
  auto tokens = readTokens(in_path);
  Decoder dec;
  auto M = dec.decode(tokens, ctx);
  if (!M) return 1;
  if (llvm::verifyModule(*M, &llvm::errs())) {
    std::fprintf(stderr, "decode: verifyModule failed\n");
    return 1;
  }
  std::error_code ec;
  llvm::raw_fd_ostream out(out_path, ec);
  if (ec) {
    std::fprintf(stderr, "decode: cannot open '%s': %s\n", out_path.c_str(),
                 ec.message().c_str());
    return 1;
  }
  M->print(out, /*AAW*/ nullptr);
  return 0;
}

} // namespace structured_codec
```

- [ ] **Step 3: Build and smoke-run on identity-smoke.ll**

```bash
cd research/structured-codec
cmake --build .build
.build/structured-codec encode ../corpus/core/identity-smoke.ll /tmp/m0.tok
cat /tmp/m0.tok ; echo
.build/structured-codec decode /tmp/m0.tok /tmp/m0.ll
cat /tmp/m0.ll
```
Expected: encode produces two integers (MODULE_BEGIN and MODULE_END token ids). decode produces an empty `.ll` (just the module header lines). Both exit 0. (This does NOT round-trip yet — that's Task 9.)

- [ ] **Step 4: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec
git commit -m "Wire encode/decode end-to-end (empty MODULE frame)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 5: Encoder — minimal TYPEDEF table (integer + void)

**Files:**
- Modify: `research/structured-codec/encoder.cpp`

The TYPEDEF table grows in encounter order. For M0 we only need `INTEGER 64` and `VOID` (for ret's "result type"). Encoder walks the module, interning every `Type *` it encounters, and emits a TYPEDEF record per new entry.

- [ ] **Step 1: Add a type-table member to `Encoder` and an `internType` method**

In `encoder.cpp`, replace the `Encoder` class with:

```cpp
class Encoder {
public:
  std::vector<uint32_t> encode(llvm::Module &M);

private:
  std::vector<uint32_t> tokens_;
  // type index in encounter order
  std::vector<llvm::Type *> type_table_;
  // map for fast lookup
  llvm::DenseMap<llvm::Type *, uint32_t> type_index_;

  uint32_t internType(llvm::Type *T);
  void emitTypeDef(llvm::Type *T); // appends a TYPEDEF record for T
};
```

Add the include at the top:
```cpp
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
```

Implement the methods below `Encoder` (still inside `namespace structured_codec`):

```cpp
uint32_t Encoder::internType(llvm::Type *T) {
  auto it = type_index_.find(T);
  if (it != type_index_.end()) return it->second;
  // Recursively intern subtypes before defining this one (so its TYPEDEF can
  // reference them by index). M0 only sees integer and void, so this is a
  // formality; it matters from M1 onward.
  if (auto *IT = llvm::dyn_cast<llvm::IntegerType>(T)) {
    (void)IT;
  }
  uint32_t idx = static_cast<uint32_t>(type_table_.size());
  type_table_.push_back(T);
  type_index_[T] = idx;
  emitTypeDef(T);
  return idx;
}

void Encoder::emitTypeDef(llvm::Type *T) {
  tokens_.push_back(encodeTag(Tag::TYPEDEF));
  if (auto *IT = llvm::dyn_cast<llvm::IntegerType>(T)) {
    tokens_.push_back(encodeTypeKind(TypeKind::INTEGER));
    emitVarint(tokens_, IT->getBitWidth());
    return;
  }
  if (T->isVoidTy()) {
    tokens_.push_back(encodeTypeKind(TypeKind::VOID));
    return;
  }
  // M0 only handles the two type kinds above. Other kinds land in M1.
  std::fprintf(stderr,
               "encode: M0 cannot yet handle type kind %u\n",
               static_cast<unsigned>(T->getTypeID()));
  std::exit(2);
}
```

- [ ] **Step 2: Update `Encoder::encode` to use the new fields and intern types lazily**

```cpp
std::vector<uint32_t> Encoder::encode(llvm::Module &M) {
  tokens_.clear();
  type_table_.clear();
  type_index_.clear();
  tokens_.push_back(encodeTag(Tag::MODULE_BEGIN));
  // (Functions walked in Task 6.)
  (void)M;
  tokens_.push_back(encodeTag(Tag::MODULE_END));
  return std::move(tokens_);
}
```

- [ ] **Step 3: Build, verify it still compiles and the smoke run from Task 4 still works (empty module frame)**

```bash
cd research/structured-codec && cmake --build .build
.build/structured-codec encode ../corpus/core/identity-smoke.ll /tmp/m0.tok
cat /tmp/m0.tok ; echo
```
Expected: still just two tokens (no TYPEDEFs because no function is walked yet). The type-table infrastructure exists; it's exercised in Task 6.

- [ ] **Step 4: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec/encoder.cpp
git commit -m "Add type-table interning (integer + void) to encoder

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 6: Encoder — FUNC_BEGIN/ARG/BLOCK_BEGIN/FUNC_END with value index + names

**Files:**
- Modify: `research/structured-codec/encoder.cpp`

Walk each function definition: intern its function type (which lazily interns ret/param types via TYPEDEFs), emit `FUNC_BEGIN <NAME literal> <type-ref varint>`, then one `ARG <NAME literal>` per parameter, then one `BLOCK_BEGIN <NAME literal>` per basic block (instructions go in Task 7), then `FUNC_END`. Each emitted definition (FUNC, ARG, BLOCK, INSTR) advances the definition-order value index by one — but in M0 we don't yet *use* the index (refs come in Task 7).

M0 ignores: linkage/visibility/cc/attributes (identity-smoke uses defaults); declarations (none in identity-smoke). These are M4 work.

- [ ] **Step 1: Add a `NAME` literal helper and a value-index counter**

In `encoder.cpp`, inside `Encoder` add:
```cpp
private:
  uint64_t next_value_index_ = 0;
  void emitNameLiteral(llvm::StringRef name);
  void emitFunction(llvm::Function &F);
  // Returns the index this call assigned.
  uint64_t defineValue();
```

Implement (still inside `namespace structured_codec`):
```cpp
void Encoder::emitNameLiteral(llvm::StringRef name) {
  tokens_.push_back(encodeTag(Tag::NAME));
  emitVarint(tokens_, name.size());
  for (char c : name) tokens_.push_back(encodeByte(static_cast<uint8_t>(c)));
}

uint64_t Encoder::defineValue() { return next_value_index_++; }
```

- [ ] **Step 2: Implement `Encoder::emitFunction`**

```cpp
void Encoder::emitFunction(llvm::Function &F) {
  // Function type goes through the type table (lazily interns ret + params).
  uint32_t fty_index = internType(F.getFunctionType());

  tokens_.push_back(encodeTag(Tag::FUNC_BEGIN));
  emitNameLiteral(F.getName());
  emitVarint(tokens_, fty_index);
  (void)defineValue(); // the function itself takes a definition-order index

  // ARG records, one per parameter, in order.
  for (llvm::Argument &A : F.args()) {
    tokens_.push_back(encodeTag(Tag::ARG));
    emitNameLiteral(A.getName());
    (void)defineValue();
  }

  // BLOCK_BEGIN per basic block (instructions in Task 7).
  for (llvm::BasicBlock &BB : F) {
    tokens_.push_back(encodeTag(Tag::BLOCK_BEGIN));
    emitNameLiteral(BB.getName());
    (void)defineValue();
    // Instructions emitted in Task 7.
  }

  tokens_.push_back(encodeTag(Tag::FUNC_END));
}
```

Note: `F.getFunctionType()` is a `FunctionType *`. Our current `emitTypeDef` only handles integer/void. Extend it minimally to handle `FunctionType` so identity-smoke (and any future function-typed value) interns cleanly. Add to `emitTypeDef`, BEFORE the integer/void cases — and recursively intern ret/params first:

```cpp
void Encoder::emitTypeDef(llvm::Type *T) {
  if (auto *FT = llvm::dyn_cast<llvm::FunctionType>(T)) {
    // Recursively intern subtypes first so their indices precede ours.
    uint32_t ret_idx = internType(FT->getReturnType());
    std::vector<uint32_t> param_idxs;
    for (llvm::Type *PT : FT->params()) param_idxs.push_back(internType(PT));
    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::FUNCTION));
    emitVarint(tokens_, ret_idx);
    emitVarint(tokens_, param_idxs.size());
    for (uint32_t pi : param_idxs) emitVarint(tokens_, pi);
    emitVarint(tokens_, FT->isVarArg() ? 1 : 0);
    return;
  }
  // ...existing integer/void cases unchanged below...
  tokens_.push_back(encodeTag(Tag::TYPEDEF));
  if (auto *IT = llvm::dyn_cast<llvm::IntegerType>(T)) {
    tokens_.push_back(encodeTypeKind(TypeKind::INTEGER));
    emitVarint(tokens_, IT->getBitWidth());
    return;
  }
  if (T->isVoidTy()) {
    tokens_.push_back(encodeTypeKind(TypeKind::VOID));
    return;
  }
  std::fprintf(stderr,
               "encode: M0 cannot yet handle type kind %u\n",
               static_cast<unsigned>(T->getTypeID()));
  std::exit(2);
}
```

WAIT — the corrected `emitTypeDef` above must NOT push the TYPEDEF tag twice for FunctionType. The function-type branch pushes its own tag and returns. Make sure the integer/void branches each push the tag too. Cleanest rewrite:

```cpp
void Encoder::emitTypeDef(llvm::Type *T) {
  if (auto *FT = llvm::dyn_cast<llvm::FunctionType>(T)) {
    uint32_t ret_idx = internType(FT->getReturnType());
    std::vector<uint32_t> param_idxs;
    for (llvm::Type *PT : FT->params()) param_idxs.push_back(internType(PT));
    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::FUNCTION));
    emitVarint(tokens_, ret_idx);
    emitVarint(tokens_, param_idxs.size());
    for (uint32_t pi : param_idxs) emitVarint(tokens_, pi);
    emitVarint(tokens_, FT->isVarArg() ? 1 : 0);
    return;
  }
  if (auto *IT = llvm::dyn_cast<llvm::IntegerType>(T)) {
    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::INTEGER));
    emitVarint(tokens_, IT->getBitWidth());
    return;
  }
  if (T->isVoidTy()) {
    tokens_.push_back(encodeTag(Tag::TYPEDEF));
    tokens_.push_back(encodeTypeKind(TypeKind::VOID));
    return;
  }
  std::fprintf(stderr,
               "encode: M0 cannot yet handle type kind %u\n",
               static_cast<unsigned>(T->getTypeID()));
  std::exit(2);
}
```

Also extend `vocab.h`'s `TypeKind` enum to include `FUNCTION` (and keep `kCount` last):

```cpp
enum class TypeKind : uint32_t {
  INTEGER = 0,
  VOID,
  FUNCTION,
  // M1+: HALF, FLOAT, DOUBLE, FP128, X86_FP80, PPC_FP128, POINTER, ARRAY,
  // VECTOR, STRUCT, ...
  kCount,
};
```

- [ ] **Step 3: Call `emitFunction` from `Encoder::encode`**

```cpp
std::vector<uint32_t> Encoder::encode(llvm::Module &M) {
  tokens_.clear();
  type_table_.clear();
  type_index_.clear();
  next_value_index_ = 0;
  tokens_.push_back(encodeTag(Tag::MODULE_BEGIN));
  for (llvm::Function &F : M) {
    if (F.isDeclaration()) continue; // M0 ignores declarations
    emitFunction(F);
  }
  tokens_.push_back(encodeTag(Tag::MODULE_END));
  return std::move(tokens_);
}
```

- [ ] **Step 4: Build and inspect the encoded stream for identity-smoke**

```bash
cd research/structured-codec && cmake --build .build
.build/structured-codec encode ../corpus/core/identity-smoke.ll /tmp/m0.tok
wc -w /tmp/m0.tok ; echo "----" ; cat /tmp/m0.tok ; echo
```
Expected: ~30–40 tokens, structured roughly as: `MODULE_BEGIN, TYPEDEF INTEGER 64, TYPEDEF FUNCTION ret=0 nparams=2 [0,0] varargs=0, FUNC_BEGIN NAME("add") fty=1, ARG NAME("a"), ARG NAME("b"), BLOCK_BEGIN NAME(""), FUNC_END, MODULE_END`. (Instructions still missing — Task 7.)

- [ ] **Step 5: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec
git commit -m "Encode function header, args, blocks (no instructions yet)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 7: Encoder — INSTR for add and ret with REF operands

**Files:**
- Modify: `research/structured-codec/encoder.cpp`

INSTR layout in M0: `INSTR <opcode> <result-TYPE_REF> <result-NAME literal> <operand list>`. Each operand is `REF <value-index>` for a value-typed operand. M0 sees only `add` (two REF operands) and `ret` (one REF operand). Modifier flags (nsw/nuw/exact, fast-math) are M2.

We need a value→index map so REF can look up an Instruction's operand.

- [ ] **Step 1: Add a value-index map and an `emitInstr` method**

In `encoder.cpp`'s `Encoder` add:
```cpp
private:
  llvm::DenseMap<llvm::Value *, uint64_t> value_index_;
  void emitInstr(llvm::Instruction &I);
  uint64_t indexOf(llvm::Value *V);
```

Implement:
```cpp
uint64_t Encoder::indexOf(llvm::Value *V) {
  auto it = value_index_.find(V);
  if (it == value_index_.end()) {
    std::fprintf(stderr, "encode: value not in index (M0 does not yet handle "
                          "globals/forward refs)\n");
    std::exit(2);
  }
  return it->second;
}
```

Replace `defineValue()` so it records the value:
```cpp
private:
  uint64_t recordValue(llvm::Value *V) {
    uint64_t idx = next_value_index_++;
    value_index_[V] = idx;
    return idx;
  }
```
And remove the old `defineValue()` declaration.

Update call sites in `emitFunction`:
```cpp
void Encoder::emitFunction(llvm::Function &F) {
  uint32_t fty_index = internType(F.getFunctionType());

  tokens_.push_back(encodeTag(Tag::FUNC_BEGIN));
  emitNameLiteral(F.getName());
  emitVarint(tokens_, fty_index);
  recordValue(&F);

  for (llvm::Argument &A : F.args()) {
    tokens_.push_back(encodeTag(Tag::ARG));
    emitNameLiteral(A.getName());
    recordValue(&A);
  }

  for (llvm::BasicBlock &BB : F) {
    tokens_.push_back(encodeTag(Tag::BLOCK_BEGIN));
    emitNameLiteral(BB.getName());
    recordValue(&BB);
    for (llvm::Instruction &I : BB) emitInstr(I);
  }

  tokens_.push_back(encodeTag(Tag::FUNC_END));
}
```

- [ ] **Step 2: Implement `emitInstr`**

```cpp
void Encoder::emitInstr(llvm::Instruction &I) {
  tokens_.push_back(encodeTag(Tag::INSTR));
  tokens_.push_back(encodeOpcode(static_cast<uint32_t>(I.getOpcode())));
  emitVarint(tokens_, internType(I.getType()));
  emitNameLiteral(I.getName());
  // Operands: M0 handles value-typed operands only (no constants, no BB
  // labels, no metadata). For identity-smoke that's exactly what we have.
  for (unsigned i = 0, e = I.getNumOperands(); i != e; ++i) {
    llvm::Value *V = I.getOperand(i);
    tokens_.push_back(encodeTag(Tag::REF));
    emitVarint(tokens_, indexOf(V));
  }
  recordValue(&I); // even void instructions get an index (never referenced)
}
```

- [ ] **Step 3: Build and inspect the full encoded stream**

```bash
cd research/structured-codec && cmake --build .build
.build/structured-codec encode ../corpus/core/identity-smoke.ll /tmp/m0.tok
cat /tmp/m0.tok ; echo
```
Expected: a complete stream covering `MODULE_BEGIN, ..., MODULE_END`, with two INSTR records (add, ret) each followed by their REF operands. No crash on the `indexOf` exit. (Quick sanity: `grep -c .` matches one line; word count is in the 50-80 range.)

- [ ] **Step 4: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec
git commit -m "Encode instructions and REF operands (M0 add+ret)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 8: Decoder — TYPEDEF table + FUNC/ARG/BLOCK with value vector

**Files:**
- Modify: `research/structured-codec/decoder.cpp`

Decoder reads tokens one at a time, dispatching on tag. It maintains: a type table (`vector<Type*>`) built from TYPEDEF records, a value table (`vector<Value*>`) appended in definition order, and a small parser-state machine (which BB/Function we're currently in). Instructions land in Task 9; this task gets us as far as an empty-bodied function with named args and an empty BB.

- [ ] **Step 1: Replace `Decoder` with the real implementation**

`decoder.cpp` (full replacement of the class + `decodeFile`):

```cpp
#include "decoder.h"
#include "vocab.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

namespace structured_codec {

class Decoder {
public:
  std::unique_ptr<llvm::Module> decode(const std::vector<uint32_t> &tokens,
                                       llvm::LLVMContext &ctx);

private:
  const std::vector<uint32_t> *tokens_ = nullptr;
  size_t pos_ = 0;
  llvm::LLVMContext *ctx_ = nullptr;
  llvm::Module *module_ = nullptr;

  std::vector<llvm::Type *> type_table_;
  std::vector<llvm::Value *> values_;

  Tag peekTag() const { return decodeTag((*tokens_)[pos_]); }
  Tag readTag() { return decodeTag((*tokens_)[pos_++]); }
  uint64_t readVar() { return readVarint(*tokens_, pos_); }
  std::string readNameLiteral();

  void readTypeDef();
  void readFunc();
};

std::string Decoder::readNameLiteral() {
  Tag t = readTag();
  if (t != Tag::NAME) {
    std::fprintf(stderr, "decode: expected NAME tag\n");
    std::exit(2);
  }
  uint64_t n = readVar();
  std::string s;
  s.resize(static_cast<size_t>(n));
  for (size_t i = 0; i < n; ++i) s[i] = static_cast<char>(decodeByte((*tokens_)[pos_++]));
  return s;
}

void Decoder::readTypeDef() {
  // TYPEDEF tag already consumed.
  TypeKind kind = decodeTypeKind((*tokens_)[pos_++]);
  switch (kind) {
  case TypeKind::INTEGER: {
    uint64_t bits = readVar();
    type_table_.push_back(llvm::IntegerType::get(*ctx_, static_cast<unsigned>(bits)));
    return;
  }
  case TypeKind::VOID:
    type_table_.push_back(llvm::Type::getVoidTy(*ctx_));
    return;
  case TypeKind::FUNCTION: {
    uint64_t ret_idx = readVar();
    uint64_t nparams = readVar();
    std::vector<llvm::Type *> params;
    params.reserve(static_cast<size_t>(nparams));
    for (uint64_t i = 0; i < nparams; ++i)
      params.push_back(type_table_[static_cast<size_t>(readVar())]);
    uint64_t vararg = readVar();
    type_table_.push_back(llvm::FunctionType::get(
        type_table_[static_cast<size_t>(ret_idx)], params, vararg != 0));
    return;
  }
  default:
    std::fprintf(stderr, "decode: unknown TypeKind %u\n",
                 static_cast<unsigned>(kind));
    std::exit(2);
  }
}

void Decoder::readFunc() {
  // FUNC_BEGIN tag already consumed.
  std::string name = readNameLiteral();
  uint64_t fty_idx = readVar();
  auto *FTy = llvm::cast<llvm::FunctionType>(type_table_[static_cast<size_t>(fty_idx)]);
  llvm::Function *F = llvm::Function::Create(
      FTy, llvm::GlobalValue::ExternalLinkage, name, module_);
  values_.push_back(F);

  // Now consume ARG / BLOCK_BEGIN / INSTR / FUNC_END records.
  llvm::Function::arg_iterator arg_it = F->arg_begin();
  llvm::BasicBlock *current_bb = nullptr;
  while (true) {
    Tag t = readTag();
    switch (t) {
    case Tag::ARG: {
      std::string an = readNameLiteral();
      if (arg_it == F->arg_end()) {
        std::fprintf(stderr, "decode: more ARG records than parameters\n");
        std::exit(2);
      }
      arg_it->setName(an);
      values_.push_back(&*arg_it);
      ++arg_it;
      break;
    }
    case Tag::BLOCK_BEGIN: {
      std::string bn = readNameLiteral();
      current_bb = llvm::BasicBlock::Create(*ctx_, bn, F);
      values_.push_back(current_bb);
      break;
    }
    case Tag::INSTR:
      // Filled in Task 9. For now, fail loudly.
      std::fprintf(stderr, "decode: INSTR not yet implemented (Task 9)\n");
      std::exit(2);
    case Tag::FUNC_END:
      return;
    default:
      std::fprintf(stderr, "decode: unexpected tag %u inside function\n",
                   static_cast<unsigned>(t));
      std::exit(2);
    }
  }
}

std::unique_ptr<llvm::Module>
Decoder::decode(const std::vector<uint32_t> &tokens, llvm::LLVMContext &ctx) {
  tokens_ = &tokens;
  pos_ = 0;
  ctx_ = &ctx;
  auto M = std::make_unique<llvm::Module>("structured-codec", ctx);
  module_ = M.get();
  type_table_.clear();
  values_.clear();

  if (readTag() != Tag::MODULE_BEGIN) {
    std::fprintf(stderr, "decode: expected MODULE_BEGIN\n");
    return nullptr;
  }
  while (true) {
    Tag t = readTag();
    if (t == Tag::MODULE_END) break;
    if (t == Tag::TYPEDEF) { readTypeDef(); continue; }
    if (t == Tag::FUNC_BEGIN) { readFunc(); continue; }
    std::fprintf(stderr, "decode: unexpected top-level tag %u\n",
                 static_cast<unsigned>(t));
    return nullptr;
  }
  return M;
}

// (writeTokens/readTokens and decodeFile from Task 4 remain below.)

} // namespace structured_codec
```

Keep the existing `readTokens` and `decodeFile` functions intact (they were already correct in Task 4).

- [ ] **Step 2: Build and run end-to-end (decode will exit 2 at the first INSTR — expected for this task)**

```bash
cd research/structured-codec && cmake --build .build
.build/structured-codec encode ../corpus/core/identity-smoke.ll /tmp/m0.tok
.build/structured-codec decode /tmp/m0.tok /tmp/m0.ll
echo "decode exit: $?"
```
Expected: decode prints `INSTR not yet implemented (Task 9)` and exits 2. The function header / args / BB were materialized correctly in memory but never written (we exit before printing).

- [ ] **Step 3: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec
git commit -m "Decode type table, function, args, and blocks

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 9: Decoder — INSTR with REF resolution; round-trip

**Files:**
- Modify: `research/structured-codec/decoder.cpp`
- Modify: `research/harness/tracks.py`
- Modify: `research/harness/test_roundtrip.py`

Add INSTR handling: read opcode + result-TYPE_REF + result-NAME + N operand REFs. Build the instruction with `IRBuilder` at the end of `current_bb` and set its name. The opcodes M0 needs are `Add` (BinaryOperator) and `Ret`. **TDD the end-to-end here:** add the harness round-trip test for `identity-smoke.ll` first, watch it fail (decode errors), then make it green.

- [ ] **Step 1: Register the `structured` track** in `research/harness/tracks.py`. Append to `TRACKS`:

```python
import os as _os
_REPO = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), "..", ".."))
_STRUCTURED_BIN = _os.path.join(_REPO, "research", "structured-codec", ".build", "structured-codec")

TRACKS["structured"] = {
    "encode": [_STRUCTURED_BIN, "encode"],
    "decode": [_STRUCTURED_BIN, "decode"],
}
```

- [ ] **Step 2: Write the failing harness test** — append to `research/harness/test_roundtrip.py`:

```python
def test_structured_track_round_trips_identity_smoke():
    module = os.path.join(CORPUS, "identity-smoke.ll")
    ok, diff = roundtrip("structured", module, LLVM_BIN)
    assert ok, f"identity-smoke failed structured round-trip:\n{diff}"
```

- [ ] **Step 3: Run the test, verify it FAILS**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer/research
LLVM_BIN=/usr/lib/llvm-22/bin python3 -m pytest harness -k structured -v
```
Expected: FAIL — the decode subprocess prints `INSTR not yet implemented` and exits 2, so `subprocess.run(..., check=True)` raises and the test fails with `CalledProcessError`.

- [ ] **Step 4: Implement INSTR handling in the decoder.**

In `decoder.cpp`, add includes near the top:
```cpp
#include <llvm/IR/Instructions.h>
```

Add a private helper to look up an operand value by varint-index:
```cpp
private:
  llvm::Value *readValueRef();
```

Implement:
```cpp
llvm::Value *Decoder::readValueRef() {
  Tag t = readTag();
  if (t != Tag::REF) {
    std::fprintf(stderr, "decode: expected REF, got tag %u\n",
                 static_cast<unsigned>(t));
    std::exit(2);
  }
  uint64_t idx = readVar();
  if (idx >= values_.size()) {
    std::fprintf(stderr, "decode: REF %llu out of range (size %zu) — forward "
                          "refs not yet supported in M0\n",
                 static_cast<unsigned long long>(idx), values_.size());
    std::exit(2);
  }
  return values_[static_cast<size_t>(idx)];
}
```

Replace the `case Tag::INSTR:` body inside `readFunc` with the real implementation:
```cpp
    case Tag::INSTR: {
      uint32_t opcode = decodeOpcode((*tokens_)[pos_++]);
      uint64_t ty_idx = readVar();
      llvm::Type *resTy = type_table_[static_cast<size_t>(ty_idx)];
      std::string in = readNameLiteral();

      // Read operands. The number of REF operands depends on the opcode;
      // we keep reading until the next tag isn't REF.
      std::vector<llvm::Value *> ops;
      while (pos_ < tokens_->size() && peekTag() == Tag::REF)
        ops.push_back(readValueRef());

      if (!current_bb) {
        std::fprintf(stderr, "decode: INSTR before any BLOCK_BEGIN\n");
        std::exit(2);
      }
      llvm::IRBuilder<> B(current_bb);
      llvm::Instruction *new_inst = nullptr;
      switch (opcode) {
      case llvm::Instruction::Add: {
        if (ops.size() != 2) {
          std::fprintf(stderr, "decode: Add expects 2 ops, got %zu\n", ops.size());
          std::exit(2);
        }
        new_inst = llvm::cast<llvm::Instruction>(
            B.CreateAdd(ops[0], ops[1], in));
        break;
      }
      case llvm::Instruction::Ret: {
        if (ops.size() == 0) new_inst = B.CreateRetVoid();
        else if (ops.size() == 1) new_inst = B.CreateRet(ops[0]);
        else {
          std::fprintf(stderr, "decode: Ret expects 0 or 1 ops\n");
          std::exit(2);
        }
        (void)resTy; // ret's "result" is its return type; not used by IRBuilder
        break;
      }
      default:
        std::fprintf(stderr,
                     "decode: M0 cannot yet build opcode %u\n", opcode);
        std::exit(2);
      }
      values_.push_back(new_inst);
      break;
    }
```

- [ ] **Step 5: Build and run the harness test, verify it PASSES**

```bash
cd research/structured-codec && cmake --build .build
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer/research
LLVM_BIN=/usr/lib/llvm-22/bin python3 -m pytest harness -k structured -v
```
Expected: PASS — `test_structured_track_round_trips_identity_smoke PASSED`.

If it FAILS with a canonical-IR diff, read the diff and check: are the value names preserved? Is the function type identical? Is there an extra `source_filename` or `ModuleID` line slipping through? Fix the encoder/decoder, not the test.

- [ ] **Step 6: Run the full suite to ensure no regression**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer/research
LLVM_BIN=/usr/lib/llvm-22/bin python3 -m pytest harness text-codec -q
```
Expected: all green (previous tests + the new structured round-trip).

- [ ] **Step 7: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/structured-codec research/harness/tracks.py research/harness/test_roundtrip.py
git commit -m "Round-trip identity-smoke.ll through Track B (M0 green)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 10: Refresh the eval snapshot (Track B joins the table for M0 corpus)

**Files:**
- Modify: `research/EVAL.md`
- Modify: `research/harness/eval.py` (a tiny change so eval can filter the corpus partition to a single module list per track — see step 1)

For M0, only `identity-smoke.ll` round-trips through `structured`; the other 7 core modules would fail. To produce a meaningful snapshot without "0/8 + 7 crashes," eval is taught to accept a per-track corpus subset (an optional positional list of module basenames). The default behavior is unchanged.

- [ ] **Step 1: Extend `research/harness/eval.py`** — add a `--modules-for-track <track>=<csv>` flag that limits which corpus modules a given track is run against. Default: all `.ll` files in the partition.

Replace the `evaluate` and `main` functions in `research/harness/eval.py` with:

```python
def evaluate(tracks, partitions, llvm_bin, per_track_modules):
    rows = []
    for track in tracks:
        for part in partitions:
            all_mods = _modules(part)
            subset = per_track_modules.get(track)
            mods = [m for m in all_mods if subset is None or os.path.basename(m) in subset]
            passes, lengths, ratios = 0, [], []
            for m in mods:
                ok, _ = roundtrip(track, m, llvm_bin)
                passes += int(ok)
                n = _token_count(track, m, llvm_bin)
                if n is not None:
                    lengths.append(n)
                    ratios.append(os.path.getsize(m) / max(n, 1))
            rows.append((track, part, passes, len(mods), lengths, ratios))
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracks", default="identity,text")
    ap.add_argument("--partitions", default="core")
    ap.add_argument("--llvm-bin", default=os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin"))
    ap.add_argument(
        "--modules-for-track",
        action="append",
        default=[],
        help="Limit one track to a CSV of module basenames: e.g. structured=identity-smoke.ll",
    )
    args = ap.parse_args(argv)
    per_track_modules = {}
    for spec in args.modules_for_track:
        track, csv = spec.split("=", 1)
        per_track_modules[track] = set(csv.split(","))
    rows = evaluate(
        args.tracks.split(","), args.partitions.split(","), args.llvm_bin,
        per_track_modules,
    )
    print(render(rows))
    return 0
```

- [ ] **Step 2: Generate the new EVAL.md** (Track B restricted to identity-smoke for M0):

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer/research
LLVM_BIN=/usr/lib/llvm-22/bin PYTHONPATH="$PWD/harness:$PWD/text-codec" \
  python3 harness/eval.py \
    --tracks identity,text,structured \
    --partitions core \
    --modules-for-track structured=identity-smoke.ll \
    > EVAL.md
cat EVAL.md
```
Expected: three rows — `identity | core | 8/8`, `text | core | 8/8`, `structured | core | 1/1`.

- [ ] **Step 3: Commit**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git add research/EVAL.md research/harness/eval.py
git commit -m "Eval supports per-track corpus subsets; Track B M0 in the table

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Self-review

**Spec coverage (M0 portion of spec §5):** Decision 1 (flat uint32 stream) → Tasks 2–9. Decision 2 (def-order index + name literals) → Tasks 6–9 (`recordValue`, `emitNameLiteral`, `values_` vector in decoder). Decision 3 (thin vertical slice first) → M0 ends at `identity-smoke.ll` round-trip (Task 9). Decision 6 (new C++ tool) → Task 1. Pain points exercised by M0: P1 references (Tasks 7, 9 `readValueRef`), P2 a sliver of the type system (integer + void + function — Tasks 5–6), P7 the detokenizer skeleton (Task 8). P2 (full types), P3, P4, P5, P6 are M1–M4 (roadmap below).

**Placeholder scan:** No `TBD`/`TODO`/"add appropriate error handling". `std::exit(2)` is used for not-yet-implemented schema in M0 — these are deliberate failure points that later milestones replace with real implementations; each has a specific message naming the construct. Each M0 task contains complete code for what it adds.

**Type consistency:** `Tag`, `TypeKind`, `VocabLayout`, `encodeByte/Tag/Opcode/TypeKind`, `emitVarint`/`readVarint` (Task 3) used identically in Tasks 5–9. `recordValue`/`indexOf`/`readValueRef` and the `values_` vector use consistent signatures across encoder and decoder. The corpus path `corpus/core/identity-smoke.ll` matches the spec.

---

## Follow-on milestones (each gets its own short plan after M0 is green)

Each milestone ends in a green round-trip test on the named corpus module(s) and refreshes `EVAL.md`. The vocabulary `Tag`/`TypeKind` enums get new entries at the end (never renumber existing ones).

### M1 — Types (P2)
- Extend `TypeKind` with `POINTER, ARRAY, VECTOR, STRUCT, HALF, FLOAT, DOUBLE, FP128, X86_FP80, PPC_FP128`.
- Recursive/named-struct interning via opaque-name-then-body pattern.
- Tasks add `TYPEDEF` payloads per kind; INSTR `alloca`/`getelementptr` carry their pointee/source-element-type ref.
- Corpus targets: `opaque-ptr.ll`, `structs.ll`.

### M2 — Modifiers + control flow (P3)
- INSTR modifier flags (nsw/nuw/exact, fast-math, atomic ordering, syncscope, alignment, GEP inbounds, alloca alignment).
- icmp/fcmp predicate enum range added to the vocab layout.
- Branches (cond/uncond), `phi` carrying `(incoming-value REF, incoming-block REF)` pairs, `switch` carrying default+cases.
- Forward-ref fixup list lands here (phi back-edges in `phi-forward-ref.ll`).
- Corpus: `phi-self-ref.ll`, `phi-forward-ref.ll`.

### M3 — Constants (P4)
- `CONST_INT`/`CONST_FP`/`CONST_AGG`/`NULL`/`UNDEF`/`POISON`/`CONSTEXPR` tags.
- Raw APInt bits as a byte literal; raw APFloat bits via `bitcastToAPInt()` (never `convertToDouble`) covering every semantics, including fp128/x86_fp80/ppc_fp128 with NaN payloads.
- Aggregates recurse via nested CONST records.
- Corpus: `wide-int.ll`, `float-semantics.ll`.

### M4 — Module + function scope (P5/P6)
- datalayout + target triple as literals.
- `GLOBAL`/`ALIAS` (initializer via CONST), `FUNC_DECL` for declarations.
- Linkage/visibility/section/dll-storage/cc enum ranges added to vocab layout.
- Attribute records tagged by slot (ret / param-N / function); string attributes as key/value literals.
- Corpus: `globals-decls.ll`.

### M5 — Full core green + eval
- All 8 `corpus/core` modules round-trip via `structured`; drop the per-track subset and refresh `EVAL.md` so `structured | core | 8/8`.
- Final code review pass over encoder/decoder; document the schema in `research/structured-codec/README.md`.
