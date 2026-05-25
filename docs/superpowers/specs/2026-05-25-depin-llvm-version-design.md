# Design: De-pin llvm-tokenizer from LLVM 17

**Date:** 2026-05-25
**Status:** Approved

## Goal

Remove the hard pinning of `llvm-tokenizer` to LLVM 17 and make it build, run, and
pass tests against current system LLVM **without Docker isolation**. Both
**LLVM 19** (stable distro package, 19.1.7) and **LLVM 22** (22.1.6) are treated as
first-class, supported targets.

## Background / findings

Established empirically by compiling and running the current source against both
system toolchains (`llvm-config-19`, `llvm-config-22`, both with dev headers +
`FileCheck`):

- **No C++ API drift.** The current source compiles cleanly against both LLVM 19
  and 22 with zero changes. CMake already uses `find_package(LLVM)` with no version
  pin. The actual pinning lives in three hard-coded constants plus the Dockerfile
  and docs.
- **The three counts are derivable from LLVM headers at compile time:**

  | Constant | Derivation | v19 | v22 |
  |---|---|---|---|
  | `OpcodeCount` | `Instruction::OtherOpsEnd - 1` | 67 | 68 |
  | `TypeCount` | `Type::TargetExtTyID + 1` | 22 | 21 |
  | `AttributeCount` | `Attribute::EndAttrKinds` | 93 | 102 |

- **Latent bug:** hard-coded `OpcodeCount = 67` is too small for LLVM 22 (max opcode
  is 68). An opcode-68 instruction serializes into `OpcodeIndex + 68`, which equals
  `TypeIndex` — a silent collision with the type-token range. Deriving the count
  fixes it.
- **Token values are inherently version-specific.** LLVM 22 removed `X86_MMXTyID`,
  shifting every later `TypeID` down by one (e.g. `i64` is type_id 13 on v19, 12 on
  v22). Attribute enum values also shifted (an attribute kind that is 46 on v19 is
  50 on v22). Therefore golden FileCheck values are coupled to the build version;
  this is correct behavior, not a defect.
- The old `AttributeCount = 200` was oversized padding, far above the real
  `EndAttrKinds` (93/102). Tightening it (per decision below) shifts the
  serialization indices even on v19, so the serialization golden test must be
  regenerated regardless of version.

## Decisions

1. **Both LLVM 19 and 22 are first-class**: counts derived in code; FileCheck
   goldens are version-keyed; CI covers both.
2. **Count-derivation robustness = "derive + clamp"** (option B): derive the three
   counts from LLVM enums *and* clamp the opcode/type/attribute offsets in the
   serializer so an out-of-range ID degrades into its last valid slot instead of
   colliding into the next range. No layout/vocab change versus derive-only.
3. **`AttributeCount` derived tightly** from `Attribute::EndAttrKinds` (not kept as
   fixed padding). Cross-version vocab stability is already lost via type/opcode
   shifts, so a magic padding constant buys nothing.
4. **Dockerfile parameterized**, default `LLVM_VERSION=19`; native build is the
   documented primary path.
5. **CI added** as a GitHub Actions matrix over LLVM {19, 22}.

## Design

### 1. Source changes — `llvm-tokenizer.cpp`

Replace the three hard-coded constants with compile-time derivations:

```cpp
static constexpr uint32_t OpcodeCount    = (uint32_t)Instruction::OtherOpsEnd - 1;
static constexpr uint32_t TypeCount      = (uint32_t)Type::TargetExtTyID + 1;
static constexpr uint32_t AttributeCount = (uint32_t)Attribute::EndAttrKinds;
```

- Add explicit includes `<llvm/IR/Instruction.h>`, `<llvm/IR/Type.h>`,
  `<llvm/IR/Attributes.h>` rather than relying on transitive includes.
- `Type::TargetExtTyID + 1` assumes `TargetExtTyID` remains the last `TypeID`
  enumerator (no count sentinel exists in LLVM). Add a comment noting this; the
  clamping below makes a future addition safe rather than corrupting.
- **Defensive clamps (option B)** in `SerializeFunctionFromTokens`: clamp the
  opcode, type, and attribute offsets so they never exceed their range
  (`min(id, Count)` style into the last valid slot), mirroring the existing
  instruction-distance clamp. ~3 lines, no layout change.
- Behavior is otherwise unchanged. Token *meaning* becomes version-specific by
  design; document this as a known property.

### 2. Version-keyed tests

Plumb the LLVM major version into lit:

- `test/CMakeLists.txt` / `test/lit.site.cfg.py.in`: set
  `config.llvm_version_major = "@LLVM_VERSION_MAJOR@"` (CMake already has
  `LLVM_VERSION_MAJOR` from `find_package(LLVM)`).
- `test/lit.cfg.py`: expose it as substitution `%llvm_major`.
- Diverging tests change their RUN line to
  `FileCheck %s --check-prefixes=CHECK,CHECK-%llvm_major`, keeping shared lines on
  `CHECK:` and adding `CHECK-19:` / `CHECK-22:` only where values differ.

Measured divergence map (derived counts, v19 → v22):

| Test | Diverging values |
|---|---|
| `basic-output.ll` | `type_id` 13→12 (standard + JSON check sets) |
| `attributes.ll` | `attribute_id` 46→50; `type_id` 13→12 |
| `instruction-operands.ll` | `type_id` 13→12 (×3) |
| `integer-constants.ll` | `type_id` 13→12 (×2) |
| `function-declarations.ll` | `type_id` 13→12 |
| `global-values.ll` | `type_id` 13→12 |
| `basic-check-serialization.ll` | full config + token block (table below) |
| `float-fp128.ll`, `floating-point-constants.ll`, `input-types.ll` | none — unchanged |

`basic-check-serialization.ll` regenerated values:

| | v19 | v22 |
|---|---|---|
| OpcodeRange End | 114 | 115 |
| TypeRange Begin | 115 | 116 |
| TypeRange End | 137 | 137 |
| AttributeRange Begin | 138 | 138 |
| AttributeRange End | 231 | 240 |
| FunctionStart | 232 | 241 |
| FunctionEnd | 233 | 242 |
| serialized tokens | `[232,60,128,45,45,60,128,2,38,48,122,2,233]` | `[241,60,128,45,45,60,128,2,38,48,123,2,242]` |

The explanatory comments in that file (which narrate the old 338/339/340 math) are
rewritten to match. Note the `i64` type token is coincidentally 128 in both
versions (opcode range grows +1 while `IntegerTyID` drops −1, cancelling); the
divergence in the token stream is the void-type token (122 vs 123) and the
function-start/end tokens.

### 3. Build, Docker, CI, docs

- **CMake:** already version-agnostic. Document that when multiple LLVMs are
  installed, the target is selected with
  `cmake -GNinja -DLLVM_DIR=$(llvm-config-19 --cmakedir) ../` (or `-22`).
- **Dockerfile:** keep `LLVM_VERSION` build-arg, change default to `19`. Native
  build documented as primary.
- **CI:** add `.github/workflows/ci.yml` — matrix `llvm: [19, 22]`, install LLVM via
  apt.llvm.org, configure with the matching `LLVM_DIR`, build, run
  `check-llvm-tokenizer`.
- **README:** add native build/test instructions with version selection;
  de-emphasize Docker. **CLAUDE.md:** update to reflect derived counts (drop the
  LLVM-17 magic-number description), the version-keyed test mechanism, the multiple-
  LLVM selection note, and the version-specific-vocabulary property.

### 4. Verification

Build + `check-llvm-tokenizer` must pass against both LLVM 19 and 22. Pre-verified:
both compile cleanly and produce the values tabulated above; the implementation pass
runs the real lit suite end-to-end under each toolchain.

## Out of scope

- Changing token semantics or making the serialization vocabulary version-stable
  (impossible without abstracting away LLVM's enums; not a goal).
- Supporting LLVM versions other than the two installed (19, 22). The derivation is
  version-agnostic, but only these two are tested/claimed.
- Comparability of ML artifacts trained on tokens from different LLVM versions
  (a downstream concern, noted but not addressed here).
