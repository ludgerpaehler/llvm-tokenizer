# Design: Lossless LLVM IR Tokenizer (comparative research)

**Date:** 2026-05-25
**Status:** Approved

## Goal

Produce **three lossless LLVM IR tokenizer artifacts**, each able to tokenize a
module and de-tokenize it back to an equivalent module, and compare them on a
shared corpus with a shared correctness gate. The current lossy tokenizer is
preserved unchanged as the `v1.0.0` baseline and serves as the comparison
control and fallback.

"Lossless" here is defined precisely (see Decisions): a **structural round-trip**
through canonical IR. The three artifacts deliberately occupy different points on
the structure-vs-fidelity-vs-ML-signal trade-off so we can measure, rather than
guess, what each approach costs.

This is a research effort, not a single product. Its primary output is the three
artifacts plus a comparison; its secondary output is whichever artifact we later
decide to productionize.

## Background / findings

The current tool (`llvm-tokenizer.cpp`) is a **lossy feature extractor by
design**: it projects arbitrary IR onto a small, fixed, LLVM-version-derived
vocabulary, deliberately replacing identity and exact values with category. A
prior introspection pass (this session) catalogued exactly where information is
destroyed; that catalogue is the backbone of the pain points below. Key facts
that shape this design:

- **No detokenizer exists.** "De-tokenize without issue" requires building one
  from scratch for each structured track.
- **LLVM already has a lossless integer serialization of IR** — bitcode — and the
  numbering machinery (`ValueEnumerator`, `ModuleSlotTracker`/`SlotTracker`) that
  the bitcode writer and the assembly printer use to assign stable ids to values,
  blocks, and metadata. The structured tracks should reuse this rather than
  re-derive it.
- **Losslessness and a small fixed ML vocabulary are in tension.** You cannot have
  both in one flat stream without a side channel. This is why three artifacts
  exist instead of one.
- **The build supports LLVM 19 and 22**; token vocabularies are inherently
  version-specific (documented property of the lossy tool, inherited here). The
  research tracks pin to **LLVM 22** initially to remove that variable.
- The four latent-bug fixes for the lossy tool are in open PRs #1–#4 on the
  private remote; the preserved baseline should include them.

## Decisions

1. **Correctness gate = structural round-trip.** `encode(M) -> decode -> M'`,
   then normalize both `M` and `M'` by round-tripping each through
   `llvm-as | llvm-dis` (canonical textual form) and diffing. Pass = identical
   canonical IR. This is robust to LLVM's own canonicalization (value renumbering,
   attribute ordering, float hex) — strict enough to catch real loss, loose enough
   to avoid false negatives. One implementation, used by every track.
2. **Metadata/debug deferred ("modulo metadata first").** Milestone 1 for the
   structured tracks is lossless for everything *except* metadata, `!dbg`, and
   named/distinct nodes. Full metadata is a later milestone (M5). The corpus is
   partitioned so metadata-bearing modules are excluded from the M1 gate.
3. **Layout = subdirectories on one branch.** All work lives under `research/` on
   the `research/lossless` branch, sharing one corpus + harness + eval. Easiest to
   compare side by side and to share fixes. The existing `llvm-tokenizer.cpp`
   stays as the lossy baseline.
4. **Preserve the lossy baseline as `v1.0.0`.** Merge PRs #1–#4 to `master`,
   create branch `release/lossy`, tag `v1.0.0`, and publish a GitHub release on
   the private remote. This is milestone M0 and the comparison control.
5. **Pin research tracks to LLVM 22 initially.** Cross-version comparability is
   out of scope for v1.
6. **Build order C -> B -> A.** Text codec first (fast end-to-end loop, fidelity
   ceiling, exercises the eval pipeline); structured codec second; hybrid third
   because it reuses the structured codec's reconstruction logic.

## Design

### 1. Repository layout

```
research/
  corpus/            shared .ll/.bc modules (+ provenance), chosen to hit every
                     pain point; partitioned into core/ (M1 gate) and
                     metadata/ (M5 gate)
  harness/           round-trip runner + canonical normalizer + eval/metrics
                     scripts  <- the shared correctness gate (Decision 1)
  hybrid/            Track A: structured stream + literal sidecar (encoder+decoder)
  structured-codec/  Track B: one rich structured stream         (encoder+decoder)
  text-codec/        Track C: byte-level BPE / SentencePiece      (trainer+enc+dec)
  README.md          how to build each track, run the harness, regenerate eval
```

All on `research/lossless`. `llvm-tokenizer.cpp` is untouched (the lossy
baseline, also tagged `v1.0.0`).

### 2. Shared correctness gate (`research/harness/`)

A single runner that, for a given track binary and a corpus module:

1. `encode(M)` to the track's token representation (+ sidecar for Track A).
2. `decode` back to a module `M'` and emit it.
3. Normalize: `llvm-as M | llvm-dis` and `llvm-as M' | llvm-dis`.
4. Diff the two canonical forms. Equal = pass; otherwise emit the diff.

`verifyModule` is run on `M'` before emission as a fast-fail. The runner reports
per-module pass/fail and aggregates into the eval metrics. It is wired into `lit`
so it runs in CI.

### 3. The three tracks

All tracks expose the same CLI contract so the harness treats them uniformly:
`<track> encode <in> -> <tokens>` and `<track> decode <tokens> -> <ir>`.

- **Track A — Hybrid (structured + sidecar).** A small structured token stream
  (an extension of the current vocabulary: opcodes, type-table refs, operand
  roles, modifier tokens) that a model trains on, plus a **position-keyed sidecar
  table** holding the exact literals the stream abstracts away (names, full
  constants, full types, value identities). The decoder consumes stream + sidecar.
  Reuses Track B's reconstruction core; the novelty is the stream/sidecar split
  and demonstrating the structured vocabulary stays small while the round-trip
  holds.
- **Track B — Fidelity-first structured codec.** One richer structured token
  stream that itself carries everything needed to rebuild the module, built on
  `ValueEnumerator`/`ModuleSlotTracker` for exact references. ML-friendliness
  (small vocab) is secondary. This is where the hard reconstruction logic lives.
- **Track C — Pure reversible text codec.** A byte-level BPE (or SentencePiece
  with `byte_fallback`) tokenizer trained over textual IR. Round-trip is
  byte-exact by construction, so it passes the structural gate trivially. It is
  the fidelity ceiling and the "what is lost by going generic" control; its costs
  are vocabulary training, sequence length, and absence of structural signal.

### 4. The pain points (core of the work; tracks A & B)

This is the focus of the effort. Each is a milestone unit with its own
round-trip lit tests. Lever names refer to the introspection analysis.

| # | Pain point | Why it is hard | Lever |
|---|---|---|---|
| **P1** | **Operand identity & exact references** | BB targets, argument index, global/callee identity, and instruction defs are all collapsed to category or a clamped distance today; phi nodes carry forward references | Stable ids via `ValueEnumerator`/`ModuleSlotTracker` (the numbering bitcode and the asm printer already use) |
| **P2** | **Type system** (category -> full type) | Integer widths, pointer address space, vectors (incl. scalable), arrays, named/recursive/packed structs, function signatures, and **opaque-pointer types whose pointee lives on the instruction** (alloca/GEP source element type), not on the operand | Per-module interned type table + structural type encoding |
| **P3** | **Instruction modifiers** | icmp/fcmp predicate, nsw/nuw/exact, fast-math flags, atomic ordering/syncscope/alignment, GEP inbounds + source element type, call cc/tail/bundles/varargs, alloca allocated-type/align/arraysize, atomicrmw op, cmpxchg orderings, switch case structure, and **phi incoming blocks (not in the operand list)** | Per-opcode modifier tokens; large surface, the harness catches anything missed |
| **P4** | **Constants, exactly** | >64-bit APInt, full APFloat raw bits for every semantics (fp128/x86_fp80/ppc_fp128, NaN payloads — never `convertToDouble`), aggregates (array/struct/vector/data), recursive ConstantExpr, null vs undef vs poison distinctly, blockaddress | Literal table (Track A) / inline structural encoding (Track B) |
| **P5** | **Module scope** | datalayout, target triple, global variables + initializers + linkage/visibility/section/align/threadlocal/addrspace, aliases, ifuncs, named struct types, comdats, module-level asm, module flags, **function declarations (call targets!)**, and deterministic function order (replace the `unordered_map`) | Module-prologue token stream |
| **P6** | **Function frame** | return + parameter types and names, linkage/visibility/calling-convention, **attributes with their values AND slot association** (return / param-N / function), string attributes, gc/section/personality/prefix/prologue, varargs | Structured function header |
| **P7** | **The detokenizer** (new) | None exists. Requires two-pass construction (materialize all values and basic blocks, then wire operands including forward references and phis), followed by `verifyModule` | New decoder per structured track |
| **P8** | **Equivalence normalization** | LLVM canonicalizes on print; "equal" must be neither too strict (false negatives) nor too loose (hides loss) | The `llvm-as \| llvm-dis` normalizer (Decision 1) |
| **P9** | **Version & determinism** | The token vocabulary is LLVM-version-specific, as the lossy tool already is | Pin LLVM 22; cross-version comparability out of scope |

Track C is unaffected by P1–P7 (it round-trips text), which is precisely the
point of including it as a control.

### 5. Corpus (`research/corpus/`)

Curated `.ll`/`.bc` modules, small and readable, each exercising a specific pain
point so failures localize:

- `core/` — non-metadata modules; the **structured-track gate** (M2/M3): phis
  (incl. self-referential and forward-ref), switches, GEPs (incl. opaque-pointer
  source types), vectors (fixed + scalable), arrays, named/recursive/packed
  structs, integer widths (i1..i128 and beyond), the full float-semantics set,
  globals + initializers, aliases, calls (direct/indirect/varargs/intrinsics),
  inline asm, attributes with values, aggregates, constexprs, null/undef/poison,
  multiple functions (order), declarations.
- `metadata/` — modules with `!dbg`, named metadata, distinct nodes; the
  structured tracks defer these to M5, while Track C round-trips them from M1.

Track C (text) runs against the whole corpus from the start; the `core`/`metadata`
split only gates the structured tracks.

Provenance recorded per file. The corpus starts curated and grows as failures are
found; a later optional extension pulls a small slice of real-world IR.

### 6. Evaluation (`research/harness/eval`)

For each track × corpus partition, emit a comparison table:

- round-trip success rate (the gate);
- fraction of modules fully reconstructed;
- vocabulary size;
- token-sequence length and compression ratio vs input bytes;
- ML-signal retention (qualitative for Track C: none structural; structured for
  A/B), and structured-vocab size for A vs B.

Output is a single Markdown table checked into `research/` plus the raw numbers.

### 7. Milestones

- **M0 — Spine.** Merge PRs #1–#4 -> `release/lossy` + tag `v1.0.0` + GitHub
  release. Scaffold `research/`; build corpus v1; implement the round-trip harness
  + canonical normalizer + eval scaffold.
- **M1 — Track C** end-to-end (encoder/decoder + byte-exact round-trip; validates
  the eval pipeline and sets the fidelity ceiling).
- **M2 — Track B core.** P1–P6 then the P7 decoder; structural round-trip green on
  `corpus/core` (modulo metadata).
- **M3 — Track A.** Reuse B's reconstruction; split into structured stream +
  sidecar; round-trip green on `corpus/core` with a small structured vocabulary.
- **M4 — Comparison.** Run eval across all three; produce the comparison table and
  a short writeup.
- **M5 (stretch) — Metadata/debug** for tracks A & B against `corpus/metadata`.

### 8. Testing strategy

The round-trip harness is the primary test (per-module pass/fail), wired into
`lit`/CI. Per-feature `.ll` round-trip tests are added TDD-style as each pain
point lands (a phi round-trips; a >64-bit integer round-trips; an opaque-pointer
alloca round-trips; …). CI extends the existing LLVM 19/22 matrix; research tracks
are pinned to LLVM 22 initially.

## Out of scope

- **Cross-version comparability** of tokens or trained artifacts (the lossy tool's
  documented version-specificity is inherited; research pins to LLVM 22).
- **Byte-exact `.ll` reconstruction** for the structured tracks (LLVM canonicalizes
  on print; the structural-round-trip gate is the chosen definition instead).
- **Metadata/debug info** until M5.
- **Productionization / choosing a winner.** This effort compares; the decision to
  ship one artifact is a separate follow-up informed by the M4 results.
- **Training actual ML models** on the artifacts (the eval measures tokenizer
  properties, not downstream model quality).
