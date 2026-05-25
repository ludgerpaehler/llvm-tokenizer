# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`llvm-tokenizer` converts LLVM IR (textual `.ll` or bitcode, from a file or stdin) into a token stream intended for machine-learning consumption. The entire program is a single translation unit, `llvm-tokenizer.cpp`; everything below describes its internal pipeline.

## Toolchain

Builds against system LLVM — currently tested with **LLVM 19 and 22**. The
opcode/type/attribute counts are derived from LLVM enums at compile time, so the
tool tracks whatever LLVM it is built against; there is no pinned version. Native
build needs `cmake`, `ninja`, `libzstd-dev`, `lit`, and an LLVM dev package
(`llvm-NN-dev`, which supplies `FileCheck`). With multiple LLVMs installed, select
one via `-DLLVM_DIR=$(llvm-config-19 --cmakedir)`. The Dockerfile remains as an
option (default `LLVM_VERSION=19`, overridable via `--build-arg`).

## Build & test

```bash
cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir)
cmake --build build                          # produces build/llvm-tokenizer
cmake --build build --target check-llvm-tokenizer   # full lit test suite
```

The suite runs via the `lit` executable (CMake finds it with `find_program`);
`lit.cfg.py` puts the build's LLVM tools dir on `PATH` so `FileCheck` resolves and
exposes the LLVM major version as `%llvm_major`. Version-divergent tests use
`FileCheck --check-prefixes=CHECK,CHECK-%llvm_major` with `CHECK-19`/`CHECK-22`
lines. Run one test with a filter, e.g.
`lit -v build/test --filter=basic-output`.

Tests are `.ll` files driven by `; RUN:` lines that pipe `%llvm-tokenizer` into `FileCheck` (`%llvm-tokenizer` is substituted with the built binary in `test/lit.cfg.py`). Format C++ with `clang-format` (LLVM style, `.clang-format`).

## Pipeline (main → output)

1. `parseIRFile` loads the module (auto-detects textual vs. bitcode). Function **declarations are skipped**; only definitions are tokenized.
2. `processFunction` emits, per function: a `FunctionStartToken`, then function **attribute** tokens (only enum/int/type attributes), then for each instruction in BB order: an **opcode** token, a **type** token (the instruction's result type), and one token per operand; finally a `FunctionEndToken`.
3. `processOperand` classifies each operand by `dyn_cast` into instruction-reference / constant (int, float, global, unknown) / basic-block / inline-asm / argument / unknown.
4. Output is selected by `-mode`: `tokenize` (default) prints structured tokens via `printTokenizedFunction`; `serialize` flattens each token to a single integer via `SerializeFunctionFromTokens`. Both render through an LLVM `ScopedPrinter` chosen by `WriterFactory` — Standard or `-output-mode=json` (`-pretty-print` to indent).

## Token model

`Token` = a `TokenType` enum + a `TokenData` **union** + the owning `InstructionIndex`. Because `TokenData` is a union, which member is valid depends entirely on `TokenType`. Any change must stay consistent across all four places that switch on `TokenType`: `GetTokenTypeName`, `processOperand`, `printTokenizedFunction`, and `SerializeFunctionFromTokens`. Adding a token type also requires extending `SerializationConfig` (below).

## Vocabulary layout (serialize mode)

`SerializationConfig` assigns every possible token a slot in one contiguous integer space, computed as a running offset in its constructor: padding (0), an instruction-operand range, an integer-constant range, then single slots for float / global / unknown-constant / basic-block / inline-asm / argument / unknown operands, then opcode, type, and attribute ranges, then function-start / function-end. `print-serialization-config` dumps the exact boundaries.

Things that drive the layout and are easy to get wrong:

- The opcode/type/attribute range sizes are derived from LLVM enums at compile
  time: `OpcodeCount = Instruction::OtherOpsEnd - 1`,
  `TypeCount = Type::TargetExtTyID + 1`, `AttributeCount = Attribute::EndAttrKinds`.
  The serializer clamps each ID into its range, so an unexpected enum value
  degrades safely instead of colliding into the next range. **Token values are
  therefore LLVM-version-specific** (e.g. LLVM 22 dropped `X86_MMXTyID`, shifting
  every later type ID) — tokens from different LLVM versions are not comparable.
- **Instruction references are encoded as a distance**, `InstructionIndex - ReferencedInstructionIndex`, clamped to `-max-instruction-operand-reference-diff` (default 32). This assumes SSA so the distance is positive; an operand whose definition wasn't recorded falls back to index 0 (known limitation, see TODO in `processOperand`).
- **Integer constants** only get unique tokens if listed in the `-int-constants-list` file (newline-separated); every other integer maps to a single shared "out of range" slot at the end of the range.
- **Floats wider than 64-bit** (IEEEquad, x87 extended, PPC double-double) are deliberately left unconverted to avoid a crash, so their `ConstantFloatValue` is unset.
