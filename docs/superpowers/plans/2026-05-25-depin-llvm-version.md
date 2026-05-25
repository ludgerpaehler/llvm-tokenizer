# De-pin llvm-tokenizer from LLVM 17 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `llvm-tokenizer` build, run, and pass tests against current system LLVM (19 and 22) without Docker, by deriving its opcode/type/attribute counts from LLVM headers and version-keying the test goldens.

**Architecture:** Replace three hard-coded LLVM-17 constants with compile-time derivations from LLVM enums (plus defensive clamps in the serializer). Teach the lit harness to (a) run via the `lit` executable, (b) put the version-matched `FileCheck` on `PATH`, and (c) expose the LLVM major version as a `%llvm_major` substitution so the three version-divergent tests carry `CHECK-19`/`CHECK-22` lines. Parameterize the Dockerfile (default 19) and add a CI matrix.

**Tech Stack:** C++17, LLVM 19/22 (`Support`, `Core`, `IRReader`), CMake + Ninja, lit + FileCheck.

---

## Background facts (verified during planning)

These were established by compiling/running against both system toolchains. Trust them; you do not need to re-derive them.

- The source compiles cleanly against LLVM 19 **and** 22 with **no** C++ API changes.
- Derived counts (replace the hard-coded `67`/`22`/`200`):

  | Constant | Derivation | v19 | v22 |
  |---|---|---|---|
  | `OpcodeCount` | `Instruction::OtherOpsEnd - 1` | 67 | 68 |
  | `TypeCount` | `Type::TargetExtTyID + 1` | 22 | 21 |
  | `AttributeCount` | `Attribute::EndAttrKinds` | 93 | 102 |

- Token *values* are version-specific by design (LLVM 22 dropped `X86_MMXTyID`, shifting type IDs; attribute enum values also shifted). This is correct behavior.
- With `FileCheck` available, the **current** source passes **9/10 on v19** (only `attributes.ll` fails — its goldens are LLVM-17 values) and **7/10 on v22** (`attributes.ll`, `basic-output.ll`, `basic-check-serialization.ll` fail). The other 7 tests assert no version-divergent value and need no changes.
- Native prerequisites discovered: `cmake`, `ninja` (installed), **`libzstd-dev`** (required — LLVM's CMake exports reference `zstd::libzstd_shared`), and a `lit` **executable** on `PATH` (the current `run-tests.py` fails because `lit` is not importable by the system `python3`).

## File map

- `llvm-tokenizer.cpp` — derive the three counts; add defensive clamps. (modify)
- `test/lit.site.cfg.py.in` — pass through `LLVM_TOOLS_BINARY_DIR`, `LLVM_VERSION_MAJOR`. (modify)
- `test/lit.cfg.py` — add LLVM tools dir to `PATH`; add `%llvm_major` substitution. (modify)
- `test/CMakeLists.txt` — invoke the `lit` executable instead of `run-tests.py`. (modify)
- `test/run-tests.py` — delete (no longer used). (delete)
- `test/basic-output.ll`, `test/attributes.ll`, `test/basic-check-serialization.ll` — version-keyed goldens. (modify)
- `Dockerfile` — default `LLVM_VERSION=19`; add `libzstd-dev`. (modify)
- `.github/workflows/ci.yml` — build+test matrix over LLVM {19, 22}. (create)
- `README.md`, `CLAUDE.md` — native build docs. (modify)

Work happens on branch `depin-llvm-version` (already created; the design doc is committed there).

---

## Task 1: Native test enablement (lit executable + FileCheck on PATH + `%llvm_major`)

After this task the suite runs natively. `attributes.ll` will still fail on v19 (pre-existing LLVM-17 goldens, fixed in Task 2); everything else passes on v19.

**Files:**
- Modify: `test/lit.site.cfg.py.in`
- Modify: `test/lit.cfg.py`
- Modify: `test/CMakeLists.txt`
- Delete: `test/run-tests.py`

- [ ] **Step 1: Pass LLVM tools dir and version into the site config**

Edit `test/lit.site.cfg.py.in` to read in full:

```python
import os

config.src_root = r'@CMAKE_SOURCE_DIR@'
config.obj_root = r'@CMAKE_BINARY_DIR@'
config.llvm_tools_dir = r'@LLVM_TOOLS_BINARY_DIR@'
config.llvm_version_major = r'@LLVM_VERSION_MAJOR@'

lit_config.load_config(
        config, os.path.join(config.src_root, "test/lit.cfg.py"))
```

- [ ] **Step 2: Put FileCheck on PATH and add the `%llvm_major` substitution**

Edit `test/lit.cfg.py` to read in full:

```python
import os

import lit.formats

config.name = 'llvm-tokenizer'
config.test_format = lit.formats.ShTest(True)

config.suffixes = ['.ll']

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.obj_root, 'test')

config.substitutions.append(('%llvm-tokenizer',
    os.path.join(config.obj_root, 'llvm-tokenizer')))

# Make the version-matched LLVM tools (e.g. FileCheck) available to RUN lines.
config.environment['PATH'] = os.pathsep.join(
    [config.llvm_tools_dir, config.environment.get('PATH', '')])

# Expose the LLVM major version so tests can select version-specific expected
# values via --check-prefixes=CHECK,CHECK-%llvm_major.
config.substitutions.append(('%llvm_major', config.llvm_version_major))
```

- [ ] **Step 3: Invoke the `lit` executable from CMake instead of the python wrapper**

Edit `test/CMakeLists.txt` to read in full:

```cmake
configure_file(lit.site.cfg.py.in lit.site.cfg.py @ONLY)

find_program(LLVM_LIT NAMES llvm-lit lit
             HINTS ${LLVM_TOOLS_BINARY_DIR}
             DOC "Path to the lit test runner")

if(NOT LLVM_LIT)
  message(WARNING
    "lit not found; the 'check-llvm-tokenizer' target will be unavailable. "
    "Install it (e.g. 'pip install lit' or 'uv tool install lit').")
else()
  add_custom_target(check-llvm-tokenizer
    COMMAND ${LLVM_LIT} -v "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS llvm-tokenizer
    USES_TERMINAL)
endif()
```

- [ ] **Step 4: Delete the obsolete wrapper**

Run: `git rm test/run-tests.py`
Expected: `rm 'test/run-tests.py'`

- [ ] **Step 5: Configure + build + run the suite against LLVM 19**

Run:
```bash
rm -rf build
cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir)
cmake --build build
cmake --build build --target check-llvm-tokenizer
```
Expected: the suite executes; result is **9 passed, 1 failed** with the only failure being `attributes.ll`. (If you instead see a `FileCheck: command not found` error or a `No module named 'lit'` traceback, Steps 1–3 are not yet correct.)

- [ ] **Step 6: Commit**

```bash
git add test/lit.site.cfg.py.in test/lit.cfg.py test/CMakeLists.txt
git commit -m "Run tests via lit executable with version-matched FileCheck

Invoke the lit binary instead of the python import wrapper (lit is not
always importable by the system python3), put the build's LLVM tools dir
on PATH so FileCheck resolves, and expose the LLVM major version as the
%llvm_major substitution for version-keyed goldens. Removes run-tests.py.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Derive counts + clamps in source; regenerate the three version-divergent goldens

TDD order: write the new (failing) goldens first, watch the serialization goldens fail against the still-hard-coded source, then change the source and watch everything pass.

**Files:**
- Modify: `test/basic-output.ll`
- Modify: `test/attributes.ll`
- Modify: `test/basic-check-serialization.ll`
- Modify: `llvm-tokenizer.cpp`

- [ ] **Step 1: Version-key `test/basic-output.ll`**

Replace the two RUN lines and the two divergent `type_id` lines. Write the file in full:

```llvm
; RUN: %llvm-tokenizer %s | FileCheck %s --check-prefixes=CHECK,CHECK-%llvm_major
; RUN: %llvm-tokenizer -output-mode=json -pretty-print %s | FileCheck %s --check-prefixes=CHECK-JSON,CHECK-JSON-%llvm_major

define i64 @f2(i64 %a, i64 %b) {
  %sum = add i64 %a, %b
	ret i64 %sum
}

; CHECK: functions [
; CHECK:   {
; CHECK:     name: f2
; CHECK:     tokens [
; CHECK:       {
; CHECK:         type: opcode
; CHECK:         instruction_index: 0
; CHECK:         opcode: 13
; CHECK:       }
; CHECK:       {
; CHECK:         type: type
; CHECK:         instruction_index: 0
; CHECK-19:         type_id: 13
; CHECK-22:         type_id: 12
; CHECK:       }
; CHECK:       {
; CHECK:         type: argument_operand
; CHECK:         instruction_index: 0
; CHECK:       }
; CHECK:       {
; CHECK:         type: argument_operand
; CHECK:         instruction_index: 0
; CHECK:       }
; CHECK:       {
; CHECK:         type: opcode
; CHECK:         instruction_index: 1
; CHECK:         opcode: 1
; CHECK:       }
; CHECK:       {
; CHECK:         type: type
; CHECK:         instruction_index: 1
; CHECK:         type_id: 7
; CHECK:       }
; CHECK:       {
; CHECK:         type: instruction_operand
; CHECK:         instruction_index: 1
; CHECK:         instruction_reference: 0
; CHECK:       }
; CHECK:     ]
; CHECK:   }
; CHECK: ]

; CHECK-JSON: {
; CHECK-JSON:   "functions": [
; CHECK-JSON:     {
; CHECK-JSON:       "name": "f2",
; CHECK-JSON:       "tokens": [
; CHECK-JSON:         {
; CHECK-JSON:           "type": "opcode",
; CHECK-JSON:           "instruction_index": 0,
; CHECK-JSON:           "opcode": 13
; CHECK-JSON:         },
; CHECK-JSON:         {
; CHECK-JSON:           "type": "type",
; CHECK-JSON:           "instruction_index": 0,
; CHECK-JSON-19:           "type_id": 13
; CHECK-JSON-22:           "type_id": 12
; CHECK-JSON:         },
; CHECK-JSON:         {
; CHECK-JSON:           "type": "argument_operand",
; CHECK-JSON:           "instruction_index": 0
; CHECK-JSON:         },
; CHECK-JSON:         {
; CHECK-JSON:           "type": "argument_operand",
; CHECK-JSON:           "instruction_index": 0
; CHECK-JSON:         },
; CHECK-JSON:         {
; CHECK-JSON:           "type": "opcode",
; CHECK-JSON:           "instruction_index": 1,
; CHECK-JSON:           "opcode": 1
; CHECK-JSON:         },
; CHECK-JSON:         {
; CHECK-JSON:           "type": "type",
; CHECK-JSON:           "instruction_index": 1,
; CHECK-JSON:           "type_id": 7
; CHECK-JSON:         },
; CHECK-JSON:         {
; CHECK-JSON:           "type": "instruction_operand",
; CHECK-JSON:           "instruction_index": 1,
; CHECK-JSON:           "instruction_reference": 0
; CHECK-JSON:         }
; CHECK-JSON:       ]
; CHECK-JSON:     }
; CHECK-JSON:   ]
; CHECK-JSON: }
```

- [ ] **Step 2: Version-key `test/attributes.ll`**

The serialize values below assume the derived counts (applied in Step 4); they will fail until then. Write the file in full:

```llvm
; RUN: %llvm-tokenizer %s | FileCheck %s --check-prefixes=CHECK,CHECK-%llvm_major
; RUN: %llvm-tokenizer %s -mode=serialize | FileCheck %s --check-prefix=CHECK-SERIALIZED-%llvm_major

define i64 @f2(i64 %a, i64 %b) optnone {
  %sum = add i64 %a, %b
  ret i64 %sum
}

; CHECK: type: attribute
; CHECK: instruction_index: 0
; CHECK-19: attribute_id: 46
; CHECK-22: attribute_id: 50

; CHECK-SERIALIZED-19: tokens: [227, 179, 55, 123, 40, 40, 43, 117, 2, 228]
; CHECK-SERIALIZED-22: tokens: [236, 183, 55, 123, 40, 40, 43, 118, 2, 237]
```

- [ ] **Step 3: Regenerate `test/basic-check-serialization.ll`**

Write the file in full (config block uses shared `CHECK` lines with `CHECK-19`/`CHECK-22` only where values diverge):

```llvm
; RUN: %llvm-tokenizer %s -mode=serialize -int-constants-list=%S/data/integers1.csv -print-serialization-config | FileCheck %s --check-prefixes=CHECK,CHECK-%llvm_major

define i64 @f1(i64 %a, i64 %b) {
	%sum = add i64 %a, %b
	%sum2 = add i64 %sum, 5
	ret i64 %sum2
}

; CHECK: config {
; CHECK:   PaddingTokenIndex: 0
; CHECK:   InstructionOperandRange {
; CHECK:     Begin: 1
; CHECK:     End: 33
; CHECK:   }
; CHECK:   ConstantOperandRange {
; CHECK:     Begin: 34
; CHECK:     End: 39
; CHECK:   }
; CHECK:   ConstantFloatOperandIndex: 40
; CHECK:   ConstantGlobalValueIndex: 41
; CHECK:   UnknownConstantOperandIndex: 42
; CHECK:   BasicBlockOperandIndex: 43
; CHECK:   InlineASMOperandIndex: 44
; CHECK:   ArgumentOperandIndex: 45
; CHECK:   UnknownOperandIndex: 46
; CHECK:   OpcodeRange {
; CHECK:     Begin: 47
; CHECK-19:     End: 114
; CHECK-22:     End: 115
; CHECK:   }
; CHECK:   TypeRange {
; CHECK-19:     Begin: 115
; CHECK-22:     Begin: 116
; CHECK:     End: 137
; CHECK:   }
; CHECK:   AttributeRange {
; CHECK:     Begin: 138
; CHECK-19:     End: 231
; CHECK-22:     End: 240
; CHECK:   }
; CHECK-19: FunctionStart: 232
; CHECK-19: FunctionEnd: 233
; CHECK-22: FunctionStart: 241
; CHECK-22: FunctionEnd: 242
; CHECK: }

; The serialized token stream is LLVM-version-specific because LLVM's opcode,
; type, and attribute enum values differ between releases. The expected streams:
;   LLVM 19: [232, 60, 128, 45, 45, 60, 128, 2, 38, 48, 122, 2, 233]
;   LLVM 22: [241, 60, 128, 45, 45, 60, 128, 2, 38, 48, 123, 2, 242]
; Reading the LLVM 19 stream:
;   232 - function start token
;   60  - the first add opcode (OpcodeIndex 47 + opcode 13)
;   128 - the i64 result type of the first instruction (TypeIndex 115 + 13)
;   45  - argument operand (%a)
;   45  - argument operand (%b)
;   60  - the second add opcode
;   128 - i64 result type of the second instruction
;   2   - instruction reference, one instruction back (InstructionOperandIndex 1 + 1)
;   38  - the integer constant 5 (ConstantIntegerOperandIndex 34 + 5 - 1)
;   48  - the ret opcode (47 + 1)
;   122 - the void return type (TypeIndex 115 + 7)
;   2   - instruction reference to the immediately preceding instruction
;   233 - function end token

; CHECK: functions [
; CHECK:   {
; CHECK:     name: f1
; CHECK-19:     tokens: [232, 60, 128, 45, 45, 60, 128, 2, 38, 48, 122, 2, 233]
; CHECK-22:     tokens: [241, 60, 128, 45, 45, 60, 128, 2, 38, 48, 123, 2, 242]
; CHECK:   }
; CHECK: ]
```

- [ ] **Step 4: Run the suite against v19 to confirm the new serialization goldens FAIL**

Run: `cmake --build build --target check-llvm-tokenizer`
Expected: FAIL — `basic-check-serialization.ll` and `attributes.ll` fail (the source still emits the hard-coded `200`-based indices, e.g. `FunctionStart: 339`), while `basic-output.ll` now passes. This is the red state TDD wants.

- [ ] **Step 5: Derive the counts and add defensive clamps in `llvm-tokenizer.cpp`**

In the include block (near the other `llvm/IR/*` includes, e.g. after `#include <llvm/IR/Constants.h>`), add:

```cpp
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Type.h>
```

Replace these three lines:

```cpp
static constexpr const uint32_t OpcodeCount = 67;
static constexpr const uint32_t TypeCount = 22;
static constexpr const uint32_t AttributeCount = 200;
```

with (derived from LLVM enums so they track the LLVM version the tool is built against; `TargetExtTyID` is assumed to remain the last `TypeID` — the clamps below make a future addition safe rather than corrupting):

```cpp
static constexpr const uint32_t OpcodeCount =
    static_cast<uint32_t>(Instruction::OtherOpsEnd) - 1;
static constexpr const uint32_t TypeCount =
    static_cast<uint32_t>(Type::TargetExtTyID) + 1;
static constexpr const uint32_t AttributeCount =
    static_cast<uint32_t>(Attribute::EndAttrKinds);
```

In `SerializeFunctionFromTokens`, replace the opcode/type/attribute branches:

```cpp
    } else if (SingleToken.Type == TokenType::OpcodeToken) {
      SerializedTokens.push_back(SerConfig.OpcodeIndex +
                                 SingleToken.Data.Opcode);
    } else if (SingleToken.Type == TokenType::TypeToken) {
      SerializedTokens.push_back(SerConfig.TypeIndex + SingleToken.Data.TypeID);
    } else if (SingleToken.Type == TokenType::AttributeToken) {
      SerializedTokens.push_back(SerConfig.AttributeIndex +
                                 SingleToken.Data.AttributeID);
    }
```

with (clamp each ID into its range's last slot so an out-of-range value can never collide into the next range; identical output for all in-range values):

```cpp
    } else if (SingleToken.Type == TokenType::OpcodeToken) {
      uint32_t Opcode = SingleToken.Data.Opcode;
      if (Opcode > OpcodeCount)
        Opcode = OpcodeCount;
      SerializedTokens.push_back(SerConfig.OpcodeIndex + Opcode);
    } else if (SingleToken.Type == TokenType::TypeToken) {
      uint32_t TypeID = SingleToken.Data.TypeID;
      if (TypeID > TypeCount)
        TypeID = TypeCount;
      SerializedTokens.push_back(SerConfig.TypeIndex + TypeID);
    } else if (SingleToken.Type == TokenType::AttributeToken) {
      uint32_t AttributeID = SingleToken.Data.AttributeID;
      if (AttributeID > AttributeCount)
        AttributeID = AttributeCount;
      SerializedTokens.push_back(SerConfig.AttributeIndex + AttributeID);
    }
```

Note: the clamps are defensive — no input in the test suite triggers them (the derived ranges already cover every enum value for the build version), so there is no dedicated clamp test.

- [ ] **Step 6: Rebuild and run the suite against v19 — expect all green**

Run:
```bash
cmake --build build
cmake --build build --target check-llvm-tokenizer
```
Expected: **10 passed, 0 failed.**

- [ ] **Step 7: Commit**

```bash
git add llvm-tokenizer.cpp test/basic-output.ll test/attributes.ll test/basic-check-serialization.ll
git commit -m "Derive opcode/type/attribute counts from LLVM enums

Replace the hard-coded LLVM-17 counts (67/22/200) with values derived from
Instruction::OtherOpsEnd, Type::TargetExtTyID, and Attribute::EndAttrKinds so
they track the LLVM version the tool is built against, and clamp serialized
opcode/type/attribute offsets defensively against future enum growth. Fixes a
latent collision under LLVM 22 (max opcode 68 > hard-coded 67). Regenerates the
three version-divergent test goldens with CHECK-19/CHECK-22 prefixes.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Cross-version verification (LLVM 19 and 22)

**Files:** none (verification only).

- [ ] **Step 1: Full clean build + test against LLVM 19**

Run:
```bash
rm -rf build
cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir)
cmake --build build
cmake --build build --target check-llvm-tokenizer
```
Expected: **10 passed, 0 failed.**

- [ ] **Step 2: Full clean build + test against LLVM 22**

Run:
```bash
rm -rf build
cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-22 --cmakedir)
cmake --build build
cmake --build build --target check-llvm-tokenizer
```
Expected: **10 passed, 0 failed.**

- [ ] **Step 3: Restore a v19 build dir for subsequent tasks (optional)**

Run: `rm -rf build && cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir) && cmake --build build`
Expected: builds successfully. (No commit — `build/` is gitignored.)

---

## Task 4: Parameterize the Dockerfile (default LLVM 19) and add libzstd-dev

**Files:**
- Modify: `Dockerfile`

- [ ] **Step 1: Change the default LLVM version to 19**

In `Dockerfile`, change the first line:

```dockerfile
ARG LLVM_VERSION=17
```

to:

```dockerfile
ARG LLVM_VERSION=19
```

(Leave the second, value-less `ARG LLVM_VERSION` after `FROM` as-is — it inherits the default.)

- [ ] **Step 2: Add libzstd-dev to the base package install**

In the first `apt-get install` list, add `libzstd-dev` alongside `zlib1g-dev` (LLVM's CMake config requires the zstd target). Change:

```dockerfile
    zlib1g-dev \
```

to:

```dockerfile
    zlib1g-dev \
    libzstd-dev \
```

- [ ] **Step 3: Build the image to verify**

Run: `docker build -t llvm-tokenizer .`
Expected: image builds successfully (it now installs LLVM 19 and `libzstd-dev`). If Docker is unavailable in your environment, skip execution and rely on CI (Task 5).

- [ ] **Step 4: Commit**

```bash
git add Dockerfile
git commit -m "Default Dockerfile to LLVM 19 and install libzstd-dev

Native builds are now the primary path; bump the container's default
LLVM_VERSION from 17 to 19 (still overridable via --build-arg) and add the
libzstd-dev dependency that find_package(LLVM) requires.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: CI matrix over LLVM 19 and 22

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Create the workflow**

Write `.github/workflows/ci.yml`:

```yaml
name: CI

on:
  push:
  pull_request:

jobs:
  build-test:
    runs-on: ubuntu-24.04
    strategy:
      fail-fast: false
      matrix:
        llvm: [19, 22]
    steps:
      - uses: actions/checkout@v4

      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'

      - name: Install lit and build tools
        run: |
          python -m pip install --upgrade pip
          python -m pip install lit
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build libzstd-dev

      - name: Install LLVM ${{ matrix.llvm }}
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{ matrix.llvm }}
          sudo apt-get install -y llvm-${{ matrix.llvm }} llvm-${{ matrix.llvm }}-dev

      - name: Configure
        run: cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-${{ matrix.llvm }} --cmakedir)

      - name: Build
        run: cmake --build build

      - name: Test
        run: cmake --build build --target check-llvm-tokenizer
```

- [ ] **Step 2: Validate the YAML locally**

Run: `python -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))" && echo OK`
Expected: `OK` (install `pyyaml` first if needed: `pip install pyyaml`).

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "Add CI matrix building and testing against LLVM 19 and 22

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

Note: the LLVM 22 leg depends on apt.llvm.org publishing packages for that version; if the `llvm.sh 22` install fails in CI, adjust the install step (e.g. pin the codename or add the repo manually). Push the branch and confirm both matrix legs are green before merging.

---

## Task 6: Documentation (README + CLAUDE.md)

**Files:**
- Modify: `README.md`
- Modify: `CLAUDE.md`

- [ ] **Step 1: Add a native build section to `README.md`**

Replace the "### Building llvm-tokenizer" and "### Running the llvm-tokenizer tests" sections with the following (native-first; the container sections above them stay):

````markdown
### Building llvm-tokenizer natively

`llvm-tokenizer` builds against any installed LLVM (currently tested with LLVM 19
and 22). You need: a C++17 compiler, `cmake`, `ninja`, `libzstd-dev`, `lit`
(`pip install lit` or `uv tool install lit`), and an LLVM development package
(`llvm-19-dev` / `llvm-22-dev`, which also provides `FileCheck`).

1. Configure, selecting the LLVM you want to build against (required when more
   than one is installed):

```bash
cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir)
```

2. Build:

```bash
cmake --build build
```

The `llvm-tokenizer` binary will be in `build/`.

### Running the tests

```bash
cmake --build build --target check-llvm-tokenizer
```

The tests detect the LLVM major version they were built against and check the
matching expected token values, so the suite passes against each supported LLVM.
````

- [ ] **Step 2: Update `CLAUDE.md`**

Make these edits to `CLAUDE.md`:

In the **Toolchain** section, replace the LLVM-17-pinned description with:

```markdown
## Toolchain

Builds against system LLVM — currently tested with **LLVM 19 and 22**. The
opcode/type/attribute counts are derived from LLVM enums at compile time, so the
tool tracks whatever LLVM it is built against; there is no pinned version. Native
build needs `cmake`, `ninja`, `libzstd-dev`, `lit`, and an LLVM dev package
(`llvm-NN-dev`, which supplies `FileCheck`). With multiple LLVMs installed, select
one via `-DLLVM_DIR=$(llvm-config-19 --cmakedir)`. The Dockerfile remains as an
option (default `LLVM_VERSION=19`, overridable via `--build-arg`).
```

In the **Vocabulary layout** section, replace the bullet about the hard-coded
`OpcodeCount`/`TypeCount`/`AttributeCount` with:

```markdown
- The opcode/type/attribute range sizes are derived from LLVM enums at compile
  time: `OpcodeCount = Instruction::OtherOpsEnd - 1`,
  `TypeCount = Type::TargetExtTyID + 1`, `AttributeCount = Attribute::EndAttrKinds`.
  The serializer clamps each ID into its range, so an unexpected enum value
  degrades safely instead of colliding into the next range. **Token values are
  therefore LLVM-version-specific** (e.g. LLVM 22 dropped `X86_MMXTyID`, shifting
  every later type ID) — tokens from different LLVM versions are not comparable.
```

In the testing section, replace the single-test guidance with:

```markdown
The suite runs via the `lit` executable (CMake finds it with `find_program`);
`lit.cfg.py` puts the build's LLVM tools dir on `PATH` so `FileCheck` resolves and
exposes the LLVM major version as `%llvm_major`. Version-divergent tests use
`FileCheck --check-prefixes=CHECK,CHECK-%llvm_major` with `CHECK-19`/`CHECK-22`
lines. Run one test with a filter, e.g.
`lit -v build/test --filter=basic-output`.
```

- [ ] **Step 3: Commit**

```bash
git add README.md CLAUDE.md
git commit -m "Document native multi-version build and version-keyed tests

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Final verification checklist

- [ ] `rm -rf build && cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir) && cmake --build build && cmake --build build --target check-llvm-tokenizer` → 10/10 pass.
- [ ] `rm -rf build && cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-22 --cmakedir) && cmake --build build && cmake --build build --target check-llvm-tokenizer` → 10/10 pass.
- [ ] `git grep -n "= 67;\|= 22;\|= 200;" llvm-tokenizer.cpp` → no matches (no hard-coded counts remain).
- [ ] `git grep -ni "LLVM_VERSION=17"` → no matches.
- [ ] Branch pushed; both CI matrix legs (19, 22) green.
- [ ] Finish the branch via the `superpowers:finishing-a-development-branch` skill (merge/PR per your preference).
