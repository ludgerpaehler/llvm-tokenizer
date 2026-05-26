# Design: Track B — fidelity-first structured codec

**Date:** 2026-05-25
**Status:** Approved
**Parent spec:** `2026-05-25-lossless-tokenizer-design.md` (Track B is the
"fidelity-first structured codec" of the three research artifacts; see Phase 2
of `docs/superpowers/plans/2026-05-25-lossless-tokenizer.md`).

## Goal

A lossless LLVM IR codec that emits and consumes **a single flat `uint32` token
stream carrying everything needed to rebuild the module**, with exact
references via a definition-order value enumerator and exact constants/types via
inline byte-tokens. The encoder/decoder is a new C++ tool gated by the existing
structural round-trip harness; success = every `corpus/core` module round-trips
losslessly (modulo metadata) and joins `EVAL.md`.

Track B is the **fidelity-first** track. ML-friendliness (small vocabulary, short
sequences) is secondary — that is Track A's territory. Track B exists to solve
the structured pain points (P1–P7 in the parent spec) head-on and to act as the
reconstruction substrate Track A will later reuse.

## Background / findings

- The structural round-trip gate canonicalizes both sides through
  `llvm-as | llvm-dis`. That canonicalization **preserves value names**
  (`%a`, `%sum`). A codec that drops names and renumbers slots would fail the
  gate. Reference encoding therefore has to be paired with name carrying.
- Forward references are real: phi incoming values and incoming blocks may
  reference values/blocks defined later. The decoder needs a two-pass / fixup
  strategy (P7).
- Opaque pointers (LLVM 15+) put the pointee type on the instruction (e.g.,
  alloca's allocated type, GEP's source element type), not the operand. Type
  encoding must capture these per-instruction type slots, not just operand types.
- LLVM exposes `ModuleSlotTracker`/`ValueEnumerator` for stable numbering, but
  ModuleSlotTracker numbers **unnamed** values only — named values have names,
  not slot ids. A codec that wants exact name preservation should not rely on
  ModuleSlotTracker as the reference scheme; it can use it as a sanity oracle.
- Track C exists as the byte-exact control (see Phase 1). Track B is what makes
  the "structured" claim meaningful — without it the research has no
  IR-semantic structured codec to compare.

## Decisions

1. **Token stream form = flat `uint32` (ML-native).** A single space-separated
   integer sequence (same I/O shape as Track C), losslessly carrying all
   structure via tagged records plus byte-token literal escapes for raw bytes.
2. **Value references = definition-order index + name literal.** The encoder
   assigns every value (global, BB, arg, instruction) an index in definition
   order; references use that index; each definition optionally carries a name
   literal. The decoder rebuilds in the same order into a `vector<Value*>` and
   resolves references — including forward refs via a fixup list. This is our
   own value enumerator, validated against (but not implemented as)
   `ModuleSlotTracker`.
3. **First milestone = thin vertical slice**, then widen pain-point by pain
   point. M0 = `identity-smoke.ll` end-to-end round-trip; subsequent milestones
   each add one pain point and the corpus module that exercises it.
4. **Modulo metadata first.** Inherited from the parent spec; `corpus/metadata`
   stays out of scope for this design.
5. **Pin LLVM 22.** Inherited from the parent spec. Cross-version comparability
   is out of scope.
6. **New C++ tool, separate CMake target** under `research/structured-codec/`,
   plugged into the existing harness via the `tracks.py` registry. The existing
   lossy `llvm-tokenizer.cpp` is not touched.

## Design

### 1. Architecture

A new tool, `research/structured-codec/structured-codec`, exposing the harness
contract:

- `structured-codec encode <in.ll> <tokens>` — parses the module with
  `parseIRFile`, walks it, writes a flat `uint32` token stream
  (space-separated decimal ints, identical I/O shape to Track C).
- `structured-codec decode <tokens> <out.ll>` — reads the token stream, rebuilds
  a `Module`, runs `verifyModule`, prints `.ll` to the output path.

Registered as the `structured` track in `research/harness/tracks.py`. Gated by
the existing structural round-trip harness; no new harness machinery is needed.

Links against LLVM `core`, `irreader`, `support`. Does NOT use the bitcode
reader/writer — Track B carries its own structure end-to-end so the pain points
are actually exercised.

### 2. Token vocabulary (flat `uint32`)

A contiguous id space, partitioned into ranges. Range sizes derived from LLVM
enums at compile time (same pattern as the lossy tool's `SerializationConfig`):

- **TAGS** — a fixed enum of record kinds (see §3).
- **BYTES** — 256 ids carrying raw bytes. All arbitrary values (varints, value
  indices, name strings, exact constant bits) ride here via LEB128. This is what
  keeps the vocabulary bounded while the stream stays lossless.
- **OPCODES** — derived from `Instruction::OtherOpsEnd`.
- **TYPEKINDS** — derived from `Type::TargetExtTyID + 1` (used inside
  `TYPEDEF` records to build the type table).
- **PREDICATES** — icmp + fcmp predicates.
- **ATTRIBUTE_KINDS** — derived from `Attribute::EndAttrKinds`.
- **ENUM small ranges** — linkage, visibility, calling convention, atomic
  ordering, syncscope, dll-storage-class. Sizes derived from LLVM enums.

A **literal** = `<NAME>` tag + length varint (bytes) + that many byte tokens.
A **value reference** = `<REF>` tag + value-index varint. Concrete offset math
goes in the implementation plan; the spec only fixes the categories.

Vocabulary size is bounded (tags + enums + 256 bytes). **Sequence length grows
with module size** — exactly the cost the eval will measure against Tracks A
and C.

### 3. Record grammar

The stream is a sequence of tag-introduced records. Each record's tag is
followed by typed arguments: enum tokens (from a fixed range), `REF` value
references, `TYPE_REF` type-table indices (varint), or literals (length + bytes).

- **MODULE / ENDMODULE** — frame the file; carry datalayout and target triple as
  literals.
- **TYPEDEF** — appends one entry to the per-module type table (built in
  encounter order). Type-kind + payload — INT→bitwidth; PTR→addrspace;
  ARRAY→count+elem TYPE_REF; VECTOR→count+scalable+elem TYPE_REF;
  STRUCT→packed-bit+optional name literal+nfields+field TYPE_REFs;
  FUNCTION→vararg-bit+ret TYPE_REF+nparams+param TYPE_REFs. Recursive/named
  structs use a forward TYPE_REF (create opaque-named on first sight, fill body
  when the TYPEDEF arrives — same pattern LLVM uses internally).
- **GLOBAL / ALIAS** — name literal + TYPE_REF + linkage/visibility/section
  enums + flags (const/threadlocal/addrspace) + initializer (a CONST record or
  "none"). Aliases reference an aliasee by REF.
- **FUNC / ENDFUNC** — name literal + function TYPE_REF + linkage/visibility/cc
  + per-slot attribute records (each tagged with slot: ret / param-N / function;
  attribute kind + value when applicable; string attributes carried as
  literal key/value). Declarations have no body. Definitions are followed by a
  sequence of BLOCK/INSTR records and an ENDFUNC.
- **BLOCK** — starts a new basic block; optional name literal. Its index in the
  function's BB sequence is the value index used by `REF`.
- **INSTR** — opcode + result TYPE_REF + optional result name literal +
  modifier flags + operand list. Modifier carriage:
  - **icmp/fcmp:** predicate enum;
  - **add/sub/mul/shl/etc.:** nsw, nuw, exact bits;
  - **fp ops:** fast-math flag bits;
  - **load/store/atomicrmw/cmpxchg:** alignment varint, volatile bit, atomic
    ordering enum, syncscope enum;
  - **GEP:** inbounds bit + source-element TYPE_REF + index operands;
  - **alloca:** allocated-element TYPE_REF + alignment varint + array-size REF
    (or 1) + addrspace varint;
  - **call/invoke:** calling-conv enum + tail/musttail bits + operand bundles +
    attributes + callee REF + arg REFs;
  - **phi:** pairs of (incoming-value REF, incoming-block REF) — phi incoming
    blocks are not in `getOperand(i)` and must be carried explicitly;
  - **switch:** default block REF + case (CONST, block REF) pairs;
  - **landingpad / catchpad / etc.:** their specific operand structures.
- **CONST** operand records (P4): integer = TYPE_REF + raw APInt bits as a
  byte-literal (exact, any width); float = TYPE_REF + raw APFloat bits as a
  byte-literal (every semantics incl. fp128/x86_fp80/ppc_fp128, NaN payloads —
  never `convertToDouble`); aggregate = TYPE_REF + element CONSTs; null / undef
  / poison = distinct tags + TYPE_REF; global-ref = REF to a global; constexpr =
  opcode + operand CONSTs (recursive).

The exact tag enumeration and the LEB128 varint format live in `vocab.h` in the
implementation plan; the spec fixes the categories above.

### 4. Value references and names

The structural gate preserves value names through `llvm-as | llvm-dis`, so the
codec must too. Scheme:

- The **encoder** walks the module and assigns every value a dense
  **definition-order index**: globals/aliases/declarations in module order;
  per function — args in parameter order, then blocks in block order, then
  instructions in block order (across all blocks of that function). Indices are
  module-scoped and unbounded.
- Every value definition (GLOBAL/ALIAS/FUNC arg/BLOCK/INSTR) is the *N*th value
  encountered, gets that index implicitly, and carries an optional **name
  literal** (the value's `getName()`; empty string = unnamed).
- Every use is `<REF> <index-varint>`.
- The **decoder** rebuilds in the same order into a `std::vector<Value*>`,
  applies names from the literals, and resolves `REF`s by index. Phi forward
  references (and any other forward ref the order forces) are handled with a
  **fixup list**: when a `REF` index isn't yet bound, record
  `(instruction*, operand-index, target-value-index)` and resolve after all
  values exist. This is the P7 detokenizer in concrete form.

ModuleSlotTracker is used as a *validation oracle* during testing — round-trip
must hold and we additionally cross-check that our definition order matches
LLVM's slot numbering for unnamed values — but not as the reference scheme.

### 5. Milestone sequence

Each milestone widens the schema by one pain point and ends in a green round-
trip test on the corpus module that exercises it.

- **M0 — Vertical slice.** End-to-end round-trip of `corpus/core/identity-
  smoke.ll`: function definition, integer types only, `add`+`ret`, args and one
  instruction result, definition-order index, name literals, the byte-token
  pipeline, the harness wiring. From M0 onward, the harness reports
  `structured | core | 1/8`.
- **M1 — Types (P2).** Type-table records, full type encoding incl.
  opaque-pointer pointee carriage; `opaque-ptr.ll` and `structs.ll` round-trip.
- **M2 — Modifiers + control flow (P3).** icmp/fcmp predicates, integer/fp
  flags, branches, phi incoming-blocks, switch; `phi-self-ref.ll` and
  `phi-forward-ref.ll` round-trip (forward-ref fixups land here).
- **M3 — Constants (P4).** Exact APInt/APFloat raw bits, aggregates,
  constexpr, null/undef/poison; `wide-int.ll` and `float-semantics.ll`
  round-trip.
- **M4 — Module + function scope (P5/P6).** datalayout/triple, globals (with
  initializers), aliases, declarations, attributes-with-slots (incl. string
  attributes); `globals-decls.ll` round-trip.
- **M5 — Full core green.** All 8 `corpus/core` modules round-trip; `structured`
  joins `eval.py` and `EVAL.md`.

### 6. Testing

- **Primary gate** = the existing structural round-trip harness on the corpus,
  run after each milestone via `pytest` (a new test parameterized over the
  `structured` track and the corpus modules added up to that milestone).
- **Per-pain-point tests** — TDD: a new corpus module (if not already present)
  + a failing round-trip test before each milestone's implementation; green
  after.
- **C++ unit tests** for two pieces with real local logic: the LEB128
  byte-varint encoder/decoder, and the type-table interning (including
  forward-ref struct bodies). These live under `research/structured-codec/test/`
  and run via either gtest-lite or a small `lit` config — choice deferred to the
  plan.
- The `structured` track joins `eval.py` at M5; `EVAL.md` is refreshed.

### 7. File structure

```
research/structured-codec/
  CMakeLists.txt            new target, links LLVM core/irreader/support
  structured_codec.cpp      CLI dispatch (encode/decode)
  vocab.h                   token id ranges, tag enum, LEB128 helpers
  encoder.cpp / encoder.h   Module -> token stream (def-order index, type table)
  decoder.cpp / decoder.h   token stream -> Module (two-pass + fixup list), verify
  README.md                 build/run notes
  test/                     LEB128 + type-table unit tests
```

The harness's `tracks.py` gets one new entry pointing at the built binary; no
other code in `research/harness/` changes.

## Out of scope

- **Metadata/debug info** (`!dbg`, named metadata, distinct nodes). Deferred to
  Phase 5; the structured tracks gate on `corpus/core` only.
- **Byte-exact `.ll` reconstruction.** The structural-round-trip gate
  (canonical form via `llvm-as | llvm-dis`) is the definition. Token byte
  sequences may differ from input bytes.
- **Cross-LLVM-version vocabularies.** Token values are LLVM-version-specific
  (inherited from the lossy tool's documented property). Pinned to LLVM 22.
- **ML training.** The eval measures tokenizer properties (round-trip success,
  vocabulary size, sequence length, compression). No model training in this
  spec.
- **Track A (hybrid).** Reuses Track B's reconstruction logic but adds a
  sidecar split; its design is a separate spec written after Track B is green.
