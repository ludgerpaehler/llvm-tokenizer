# Lossless LLVM IR Tokenizer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the shared spine (preserved lossy baseline + corpus + structural round-trip harness + eval) and the first lossless artifact (Track C, a reversible text codec), then proceed via follow-on plans to the structured tracks.

**Architecture:** All research work lives under `research/` on the `research/lossless` branch, sharing one corpus and one round-trip harness. The harness defines "lossless" operationally: `encode -> decode`, normalize both sides through `llvm-as | llvm-dis`, and diff. Each track exposes the same `encode`/`decode` CLI so the harness treats them uniformly.

**Tech Stack:** C++17/LLVM 19+22 (existing tool + future structured tracks), Python 3 (harness + Track C), HuggingFace `tokenizers` (Track C BPE), `lit`/FileCheck (existing test runner), `gh` (releases). Research tracks pin to **LLVM 22** (`/usr/lib/llvm-22/bin`).

**Conventions:** Commit messages end with the repo trailer `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`. `private` is the default remote.

**Scope of THIS plan:** Phase 0 (spine) and Phase 1 (Track C) are fully detailed. Phase 2 (Track B), Phase 3 (Track A), Phase 4 (comparison), Phase 5 (metadata) are a roadmap — each gets its own spec + plan once the structured token schema is designed. See "Follow-on plans" at the end.

**Reference:** Spec at `docs/superpowers/specs/2026-05-25-lossless-tokenizer-design.md`. Pain-point taxonomy P1–P9 is in spec §4.

---

## File structure (this plan)

```
research/
  README.md                     how to build/run each track, run the harness, regenerate eval
  corpus/
    PROVENANCE.md               where each module came from / what it exercises
    core/                       non-metadata modules; the structured-track gate
      identity-smoke.ll         minimal module to self-test the harness
      phi-self-ref.ll           P1/P3: self-referential phi
      phi-forward-ref.ll        P1/P3: phi referencing a later-defined value
      opaque-ptr.ll             P2: alloca/GEP source element type off the operand
      wide-int.ll               P4: i128 constant
      float-semantics.ll        P4: half/float/double/fp128/x86_fp80/ppc_fp128
      structs.ll                P2: named/recursive/packed structs
      globals-decls.ll          P5: globals, initializers, declarations, call targets
    metadata/                   deferred to Phase 5 for structured tracks
      dbg-basic.ll              P-meta: a !dbg location + named metadata
  harness/
    requirements.txt            pytest
    normalize.py                canonical(ll) via llvm-as|llvm-dis (+ strip volatile lines)
    tracks.py                   registry: track name -> encode/decode argv
    roundtrip.py                encode->decode->normalize->diff for one (track, module)
    eval.py                     run roundtrip over corpus x tracks -> Markdown table
    test_normalize.py           tests for normalize.py
    test_roundtrip.py           tests for roundtrip.py (uses the "identity" track)
  text-codec/                   Track C
    requirements.txt            tokenizers
    text_codec/__init__.py
    text_codec/__main__.py      encode/decode CLI (raw byte-level)
    text_codec/train.py         train a ByteLevel BPE over the corpus
    test_text_codec.py          round-trip + losslessness tests
```

---

## Phase 0 — Spine (milestone M0)

### Task 0.1: Preserve the lossy tool as the v1.0.0 release

**Files:** none in-tree (git/GitHub state only).

This merges the four open bugfix PRs into `master`, then snapshots that as the baseline. Do this on `master`, not `research/lossless`.

- [ ] **Step 1: Update master and merge the four fix branches in order**

```bash
cd /home/lpaehler/Work/LLVM-ML/llvm-tokenizer
git checkout master
git pull private master
for b in fix/dead-return-in-processoperand fix/instruction-distance-assert \
         fix/int-constants-list-parsing fix/serialize-dedicated-oob-slots; do
  git merge --no-ff "$b" -m "Merge $b" || { echo "RESOLVE CONFLICT in $b, then: git merge --continue"; break; }
done
```

Expected: four merge commits. Only #2 and #4 touch `SerializeFunctionFromTokens` (different sub-blocks) so conflicts, if any, are small; resolve by keeping both changes.

- [ ] **Step 2: Verify the merged baseline builds and passes on both LLVMs**

```bash
cmake --build build && /usr/lib/llvm-22/bin/lit build/test          # expect: Passed: 12
cmake -G Ninja -S . -B build-19 -DLLVM_DIR=$(llvm-config-19 --cmakedir) \
  && cmake --build build-19 && lit build-19/test                    # expect: Passed: 12
rm -rf build-19
```

Expected: 12/12 on each (10 original + the 2 new bugfix tests). If a serialize golden fails, the #2/#4 merge dropped a change — re-resolve.

- [ ] **Step 3: Push master; the PRs auto-close as merged**

```bash
git push private master
gh pr list --repo ludgerpaehler/llvm-tokenizer --state merged   # expect #1-#4 listed
```

- [ ] **Step 4: Create the release branch and tag**

```bash
git branch release/lossy master
git tag -a v1.0.0 -m "Lossy llvm-tokenizer baseline (pre-lossless-rewrite)"
git push private release/lossy
git push private v1.0.0
```

- [ ] **Step 5: Publish the GitHub release**

```bash
gh release create v1.0.0 --repo ludgerpaehler/llvm-tokenizer \
  --target release/lossy --title "v1.0.0 — lossy baseline" \
  --notes "Frozen lossy tokenizer (single-TU llvm-tokenizer.cpp) including bugfixes #1-#4. Baseline/control for the lossless tokenizer research; see docs/superpowers/specs/2026-05-25-lossless-tokenizer-design.md."
gh release view v1.0.0 --repo ludgerpaehler/llvm-tokenizer   # confirm
```

- [ ] **Step 6: Return to the research branch and rebase it on the updated master**

```bash
git checkout research/lossless
git rebase master      # picks up the merged bugfixes + spec under the new master
```

Expected: clean rebase (research/lossless only added the spec doc).

### Task 0.2: Scaffold the research tree

**Files:** Create `research/README.md`, `research/corpus/PROVENANCE.md`, `research/harness/requirements.txt`, `research/text-codec/requirements.txt`.

- [ ] **Step 1: Create the directories and README**

```bash
mkdir -p research/corpus/core research/corpus/metadata research/harness research/text-codec/text_codec
```

`research/README.md`:

```markdown
# Lossless tokenizer research

Three lossless LLVM IR tokenizer artifacts compared on a shared corpus and a
shared structural round-trip gate. See
`../docs/superpowers/specs/2026-05-25-lossless-tokenizer-design.md`.

## Layout
- `corpus/`  shared `.ll` modules (`core/` gates the structured tracks; `metadata/` is Phase 5)
- `harness/` the round-trip correctness gate + eval (Python)
- `text-codec/` Track C: reversible byte-level codec

## Quick start
    python -m venv .venv && . .venv/bin/activate
    pip install -r harness/requirements.txt -r text-codec/requirements.txt
    export LLVM_BIN=/usr/lib/llvm-22/bin
    pytest harness text-codec                 # unit + round-trip tests
    python harness/eval.py --llvm-bin $LLVM_BIN   # comparison table
```

`research/corpus/PROVENANCE.md`:

```markdown
# Corpus provenance

All modules are hand-written for this research to exercise a specific pain point
(see spec §4). LLVM 22 textual IR.

| File | Pain point | Notes |
|------|-----------|-------|
| core/identity-smoke.ll | — | minimal module, harness self-test |
| core/phi-self-ref.ll | P1/P3 | self-referential phi |
| core/phi-forward-ref.ll | P1/P3 | phi referencing later-defined value |
| core/opaque-ptr.ll | P2 | alloca/GEP source element type off-operand |
| core/wide-int.ll | P4 | i128 constant |
| core/float-semantics.ll | P4 | half/float/double/fp128/x86_fp80/ppc_fp128 |
| core/structs.ll | P2 | named/recursive/packed structs |
| core/globals-decls.ll | P5 | globals, initializers, declarations, call targets |
| metadata/dbg-basic.ll | P-meta | !dbg + named metadata (Phase 5) |
```

`research/harness/requirements.txt`:

```
pytest>=8
```

`research/text-codec/requirements.txt`:

```
tokenizers>=0.20
```

- [ ] **Step 2: Commit**

```bash
git add research/README.md research/corpus/PROVENANCE.md research/harness/requirements.txt research/text-codec/requirements.txt
git commit -m "Scaffold research/ tree for lossless tokenizer work

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 0.3: Add the starter corpus

**Files:** Create the eight `research/corpus/core/*.ll` files and `research/corpus/metadata/dbg-basic.ll`.

- [ ] **Step 1: Write the corpus modules**

`research/corpus/core/identity-smoke.ll`:

```llvm
define i64 @add(i64 %a, i64 %b) {
  %s = add i64 %a, %b
  ret i64 %s
}
```

`research/corpus/core/phi-self-ref.ll`:

```llvm
define i64 @loop(i64 %n) {
entry:
  br label %loop
loop:
  %x = phi i64 [ 0, %entry ], [ %x, %loop ]
  %c = icmp slt i64 %x, %n
  br i1 %c, label %loop, label %done
done:
  ret i64 %x
}
```

`research/corpus/core/phi-forward-ref.ll`:

```llvm
define i64 @count(i64 %n) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop ]
  %i.next = add nsw i64 %i, 1
  %c = icmp slt i64 %i.next, %n
  br i1 %c, label %loop, label %done
done:
  ret i64 %i
}
```

`research/corpus/core/opaque-ptr.ll`:

```llvm
define i32 @load_elt(ptr %p, i64 %idx) {
  %slot = alloca [4 x i32], align 16
  %gep = getelementptr inbounds [4 x i32], ptr %slot, i64 0, i64 %idx
  store i32 7, ptr %gep, align 4
  %v = load i32, ptr %gep, align 4
  ret i32 %v
}
```

`research/corpus/core/wide-int.ll`:

```llvm
define i128 @big() {
  ret i128 170141183460469231731687303715884105727
}
```

`research/corpus/core/float-semantics.ll`:

```llvm
define void @floats(ptr %h, ptr %f, ptr %d, ptr %q, ptr %x, ptr %p) {
  store half      0xH3C00,                      ptr %h
  store float     1.5,                          ptr %f
  store double    3.14159265358979,             ptr %d
  store fp128     0xL00000000000000004000900000000000, ptr %q
  store x86_fp80  0xK4000C90FDAA22168C000,      ptr %x
  store ppc_fp128 0xM40090000000000000000000000000000, ptr %p
  ret void
}
```

`research/corpus/core/structs.ll`:

```llvm
%pair = type { i32, i64 }
%node = type { i32, ptr }
%packed = type <{ i8, i32 }>

define %pair @mk(i32 %a, i64 %b) {
  %0 = insertvalue %pair undef, i32 %a, 0
  %1 = insertvalue %pair %0, i64 %b, 1
  ret %pair %1
}

define i32 @first(ptr %n) {
  %f = getelementptr %node, ptr %n, i64 0, i32 0
  %v = load i32, ptr %f
  ret i32 %v
}
```

`research/corpus/core/globals-decls.ll`:

```llvm
@g = global i32 42, align 4
@ro = constant [3 x i8] c"hi\00"
@alias = alias i32, ptr @g

declare i32 @puts(ptr)

define i32 @use() {
  %p = getelementptr [3 x i8], ptr @ro, i64 0, i64 0
  %r = call i32 @puts(ptr %p)
  %v = load i32, ptr @g, align 4
  ret i32 %v
}
```

`research/corpus/metadata/dbg-basic.ll`:

```llvm
define i32 @f() !dbg !4 {
  ret i32 0, !dbg !7
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, emissionKind: FullDebug)
!1 = !DIFile(filename: "f.c", directory: "/tmp")
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, unit: !0)
!7 = !DILocation(line: 1, column: 1, scope: !4)
```

- [ ] **Step 2: Verify every corpus module parses under LLVM 22**

Run:

```bash
for f in research/corpus/core/*.ll research/corpus/metadata/*.ll; do
  /usr/lib/llvm-22/bin/llvm-as "$f" -o /dev/null && echo "OK $f" || echo "BAD $f"
done
```

Expected: `OK` for all nine. Fix any `BAD` (syntax) before committing.

- [ ] **Step 3: Commit**

```bash
git add research/corpus
git commit -m "Add starter corpus covering lossless pain points

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 0.4: Canonical normalizer

**Files:** Create `research/harness/normalize.py`, `research/harness/test_normalize.py`.

- [ ] **Step 1: Write the failing test**

`research/harness/test_normalize.py`:

```python
import os
import textwrap
import pytest
from normalize import canonical

LLVM_BIN = os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin")


def _write(tmp_path, name, text):
    p = tmp_path / name
    p.write_text(textwrap.dedent(text))
    return str(p)


def test_whitespace_differences_canonicalize_equal(tmp_path):
    a = _write(tmp_path, "a.ll", """
        define i64 @f(i64 %a) {
          ret i64 %a
        }
    """)
    b = _write(tmp_path, "b.ll", "define i64 @f(i64 %a){ret i64 %a}\n")
    assert canonical(a, LLVM_BIN) == canonical(b, LLVM_BIN)


def test_semantically_different_modules_differ(tmp_path):
    a = _write(tmp_path, "a.ll", "define i64 @f(i64 %a){ret i64 %a}\n")
    b = _write(tmp_path, "b.ll", "define i64 @f(i64 %a){ret i64 0}\n")
    assert canonical(a, LLVM_BIN) != canonical(b, LLVM_BIN)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd research/harness && LLVM_BIN=/usr/lib/llvm-22/bin python -m pytest test_normalize.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'normalize'`.

- [ ] **Step 3: Write minimal implementation**

`research/harness/normalize.py`:

```python
"""Canonicalize LLVM IR for structural round-trip comparison.

A module is canonicalized by assembling then disassembling it
(`llvm-as | llvm-dis`), which applies LLVM's own normalization (value
renumbering, attribute ordering, float printing). Two volatile lines that depend
only on the input path are stripped so reconstructed modules compare equal.
"""
import subprocess


def _strip_volatile(ir: bytes) -> bytes:
    keep = []
    for line in ir.split(b"\n"):
        if line.startswith(b"; ModuleID =") or line.startswith(b"source_filename ="):
            continue
        keep.append(line)
    return b"\n".join(keep)


def canonical(ll_path: str, llvm_bin: str) -> bytes:
    asm = subprocess.run(
        [f"{llvm_bin}/llvm-as", ll_path, "-o", "-"],
        check=True, capture_output=True,
    ).stdout
    dis = subprocess.run(
        [f"{llvm_bin}/llvm-dis", "-o", "-"],
        input=asm, check=True, capture_output=True,
    ).stdout
    return _strip_volatile(dis)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd research/harness && LLVM_BIN=/usr/lib/llvm-22/bin python -m pytest test_normalize.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add research/harness/normalize.py research/harness/test_normalize.py
git commit -m "Add canonical IR normalizer for round-trip comparison

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 0.5: Track registry

**Files:** Create `research/harness/tracks.py`.

- [ ] **Step 1: Write the registry**

`research/harness/tracks.py`:

```python
"""Registry of track encode/decode commands.

Each track maps to an `encode` and `decode` argv prefix. The harness appends two
positional args to each: `encode <input.ll> <tokens>` and
`decode <tokens> <output.ll>`. The "identity" track copies the file through
unchanged and exists only to self-test the harness.
"""
import sys

PY = sys.executable

TRACKS = {
    "identity": {"encode": ["cp"], "decode": ["cp"]},
    "text": {
        "encode": [PY, "-m", "text_codec", "encode"],
        "decode": [PY, "-m", "text_codec", "decode"],
    },
}
```

- [ ] **Step 2: Commit**

```bash
git add research/harness/tracks.py
git commit -m "Add track registry for the round-trip harness

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 0.6: Round-trip runner

**Files:** Create `research/harness/roundtrip.py`, `research/harness/test_roundtrip.py`.

- [ ] **Step 1: Write the failing test (uses the identity track)**

`research/harness/test_roundtrip.py`:

```python
import os
import pytest
from roundtrip import roundtrip

LLVM_BIN = os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin")
CORPUS = os.path.join(os.path.dirname(__file__), "..", "corpus", "core")


def test_identity_track_round_trips_smoke():
    module = os.path.join(CORPUS, "identity-smoke.ll")
    ok, _ = roundtrip("identity", module, LLVM_BIN)
    assert ok


def test_identity_track_round_trips_all_core():
    for name in sorted(os.listdir(CORPUS)):
        module = os.path.join(CORPUS, name)
        ok, diff = roundtrip("identity", module, LLVM_BIN)
        assert ok, f"{name} failed identity round-trip:\n{diff}"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd research/harness && LLVM_BIN=/usr/lib/llvm-22/bin python -m pytest test_roundtrip.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'roundtrip'`.

- [ ] **Step 3: Write minimal implementation**

`research/harness/roundtrip.py`:

```python
"""Structural round-trip gate: encode -> decode -> normalize both -> diff.

A module round-trips losslessly iff the canonical form of the reconstruction
equals the canonical form of the input (see normalize.canonical and spec §2).
The reconstruction is also required to assemble (a fast verify).
"""
import argparse
import difflib
import os
import subprocess
import sys
import tempfile

from normalize import canonical
from tracks import TRACKS


def roundtrip(track: str, module: str, llvm_bin: str):
    spec = TRACKS[track]
    with tempfile.TemporaryDirectory() as d:
        tokens = os.path.join(d, "tokens")
        recon = os.path.join(d, "recon.ll")
        subprocess.run(spec["encode"] + [module, tokens], check=True)
        subprocess.run(spec["decode"] + [tokens, recon], check=True)
        # Fast fail: reconstruction must assemble.
        subprocess.run([f"{llvm_bin}/llvm-as", recon, "-o", os.devnull], check=True)
        want = canonical(module, llvm_bin)
        got = canonical(recon, llvm_bin)
        if want == got:
            return True, ""
        diff = "\n".join(difflib.unified_diff(
            want.decode(errors="replace").splitlines(),
            got.decode(errors="replace").splitlines(),
            fromfile="original", tofile="reconstructed", lineterm="",
        ))
        return False, diff


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("track")
    ap.add_argument("module")
    ap.add_argument("--llvm-bin", default=os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin"))
    args = ap.parse_args(argv)
    ok, diff = roundtrip(args.track, args.module, args.llvm_bin)
    if not ok:
        sys.stderr.write(diff + "\n")
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd research/harness && LLVM_BIN=/usr/lib/llvm-22/bin python -m pytest test_roundtrip.py -v`
Expected: PASS (2 passed) — the identity track round-trips every core module, proving the harness itself is correct.

- [ ] **Step 5: Commit**

```bash
git add research/harness/roundtrip.py research/harness/test_roundtrip.py
git commit -m "Add structural round-trip runner (self-tested via identity track)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 0.7: Eval / comparison table

**Files:** Create `research/harness/eval.py`.

- [ ] **Step 1: Write the eval script**

`research/harness/eval.py`:

```python
"""Run every track over the corpus and emit a Markdown comparison table.

Metrics per (track, partition): round-trip pass rate, mean token-sequence length,
and mean bytes/token compression vs the raw input. Token count is read from the
tokens file (whitespace-separated ints) when present; tracks whose tokens are not
ints report '-'.
"""
import argparse
import os
import subprocess
import tempfile

from normalize import canonical  # noqa: F401  (kept for parity / future metrics)
from roundtrip import roundtrip
from tracks import TRACKS

HERE = os.path.dirname(__file__)
CORPUS = os.path.join(HERE, "..", "corpus")


def _modules(partition):
    d = os.path.join(CORPUS, partition)
    return [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.endswith(".ll")]


def _token_count(track, module, llvm_bin):
    spec = TRACKS[track]
    with tempfile.TemporaryDirectory() as d:
        tokens = os.path.join(d, "tokens")
        subprocess.run(spec["encode"] + [module, tokens], check=True)
        data = open(tokens, "rb").read()
        try:
            return len(data.split())
        except Exception:
            return None


def evaluate(tracks, partitions, llvm_bin):
    rows = []
    for track in tracks:
        for part in partitions:
            mods = _modules(part)
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


def render(rows):
    out = ["| Track | Partition | Round-trip | Mean tokens | Bytes/token |",
           "|-------|-----------|-----------|-------------|-------------|"]
    for track, part, passes, total, lengths, ratios in rows:
        mt = f"{sum(lengths)/len(lengths):.0f}" if lengths else "-"
        br = f"{sum(ratios)/len(ratios):.2f}" if ratios else "-"
        out.append(f"| {track} | {part} | {passes}/{total} | {mt} | {br} |")
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracks", default="identity,text")
    ap.add_argument("--partitions", default="core")
    ap.add_argument("--llvm-bin", default=os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin"))
    args = ap.parse_args(argv)
    rows = evaluate(args.tracks.split(","), args.partitions.split(","), args.llvm_bin)
    print(render(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Smoke-run the eval (identity track only, before Track C exists)**

Run: `cd research/harness && LLVM_BIN=/usr/lib/llvm-22/bin python eval.py --tracks identity --partitions core`
Expected: a Markdown table with `identity | core | 8/8 | <n> | <ratio>`.

- [ ] **Step 3: Commit**

```bash
git add research/harness/eval.py
git commit -m "Add eval script producing the track comparison table

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 1 — Track C: reversible text codec (milestone M1)

### Task 1.1: Raw byte-level codec (guaranteed lossless, no training)

**Files:** Create `research/text-codec/text_codec/__init__.py`, `research/text-codec/text_codec/__main__.py`, `research/text-codec/test_text_codec.py`.

Byte-level encoding maps each input byte to an integer token (0–255), so `decode(encode(x)) == x` for any bytes — lossless by construction, before any BPE training. This establishes correctness; Task 1.3 adds BPE for compression metrics.

- [ ] **Step 1: Write the failing test**

`research/text-codec/test_text_codec.py`:

```python
import os
import subprocess
import sys

HERE = os.path.dirname(__file__)
CORPUS = os.path.join(HERE, "..", "corpus", "core")
PY = sys.executable


def _run(args, **kw):
    return subprocess.run([PY, "-m", "text_codec", *args], cwd=HERE, check=True, **kw)


def test_byte_roundtrip_is_exact(tmp_path):
    src = os.path.join(CORPUS, "float-semantics.ll")
    tokens = tmp_path / "t"
    recon = tmp_path / "r.ll"
    _run(["encode", src, str(tokens)])
    _run(["decode", str(tokens), str(recon)])
    assert recon.read_bytes() == open(src, "rb").read()


def test_tokens_are_space_separated_ints(tmp_path):
    src = os.path.join(CORPUS, "identity-smoke.ll")
    tokens = tmp_path / "t"
    _run(["encode", src, str(tokens)])
    ids = tokens.read_text().split()
    assert ids and all(0 <= int(x) <= 255 for x in ids)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd research/text-codec && python -m pytest test_text_codec.py -v`
Expected: FAIL — `No module named text_codec` (or non-zero exit from the missing CLI).

- [ ] **Step 3: Write minimal implementation**

`research/text-codec/text_codec/__init__.py`:

```python
```

(empty file — marks the package)

`research/text-codec/text_codec/__main__.py`:

```python
"""Track C reversible text codec.

`encode <in> <out>` writes space-separated byte ids (0-255); `decode <in> <out>`
reconstructs the exact bytes. Byte-level mapping is lossless by construction.
"""
import sys


def encode(in_path, out_path):
    data = open(in_path, "rb").read()
    with open(out_path, "w") as f:
        f.write(" ".join(str(b) for b in data))


def decode(in_path, out_path):
    ids = open(in_path).read().split()
    with open(out_path, "wb") as f:
        f.write(bytes(int(x) for x in ids))


def main(argv):
    cmd, in_path, out_path = argv[0], argv[1], argv[2]
    if cmd == "encode":
        encode(in_path, out_path)
    elif cmd == "decode":
        decode(in_path, out_path)
    else:
        raise SystemExit(f"unknown command {cmd!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd research/text-codec && python -m pytest test_text_codec.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add research/text-codec/text_codec/__init__.py research/text-codec/text_codec/__main__.py research/text-codec/test_text_codec.py
git commit -m "Add Track C raw byte-level reversible codec

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 1.2: Wire Track C into the harness round-trip

**Files:** Modify `research/harness/test_roundtrip.py` (add Track C cases).

The `text` track is already registered (Task 0.5). The harness invokes `python -m text_codec`, which must be importable — run the harness with `text-codec/` on `PYTHONPATH`.

- [ ] **Step 1: Add the failing test**

Append to `research/harness/test_roundtrip.py`:

```python
def test_text_track_round_trips_all_core():
    for name in sorted(os.listdir(CORPUS)):
        module = os.path.join(CORPUS, name)
        ok, diff = roundtrip("text", module, LLVM_BIN)
        assert ok, f"{name} failed text round-trip:\n{diff}"
```

- [ ] **Step 2: Run it to verify it fails (or errors) without text_codec on the path**

Run: `cd research/harness && LLVM_BIN=/usr/lib/llvm-22/bin python -m pytest test_roundtrip.py::test_text_track_round_trips_all_core -v`
Expected: FAIL/ERROR — `No module named text_codec`.

- [ ] **Step 3: Make text_codec importable from the harness**

Run the harness with both source dirs on the path. Document this in `research/README.md` and use it henceforth:

```bash
export PYTHONPATH="$PWD/research/text-codec:$PWD/research/harness"
```

- [ ] **Step 4: Run to verify it passes**

Run: `cd research/harness && PYTHONPATH="$PWD/../text-codec:$PWD" LLVM_BIN=/usr/lib/llvm-22/bin python -m pytest test_roundtrip.py -v`
Expected: PASS (all identity + text cases) — Track C round-trips the whole core corpus.

- [ ] **Step 5: Commit**

```bash
git add research/harness/test_roundtrip.py research/README.md
git commit -m "Gate Track C through the round-trip harness

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 1.3: ByteLevel BPE training (compression layer, stays lossless)

**Files:** Create `research/text-codec/text_codec/train.py`; modify `research/text-codec/text_codec/__main__.py` (use a trained model when present), `research/text-codec/test_text_codec.py` (add a trained round-trip test).

Byte-level BPE (HF `tokenizers`) keeps the 256 base-byte tokens, so it remains lossless while merges shorten sequences. Training is optional: the codec falls back to raw bytes if no model file is given.

- [ ] **Step 1: Write the failing test**

Append to `research/text-codec/test_text_codec.py`:

```python
def test_trained_bpe_roundtrip_is_exact(tmp_path):
    model = tmp_path / "bpe.json"
    _run(["train", CORPUS, str(model), "--vocab-size", "500"])
    src = os.path.join(CORPUS, "structs.ll")
    tokens = tmp_path / "t"
    recon = tmp_path / "r.ll"
    _run(["encode", src, str(tokens), "--model", str(model)])
    _run(["decode", str(tokens), str(recon), "--model", str(model)])
    assert recon.read_bytes() == open(src, "rb").read()
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd research/text-codec && python -m pytest test_text_codec.py::test_trained_bpe_roundtrip_is_exact -v`
Expected: FAIL — `train` is an unknown command / `--model` unrecognized.

- [ ] **Step 3: Implement the trainer**

`research/text-codec/text_codec/train.py`:

```python
"""Train a byte-level BPE tokenizer over the corpus.

Byte-level alphabet keeps all 256 bytes as base tokens, so encode/decode stays
lossless regardless of the learned merges; training only shortens sequences.
"""
import glob
import os

from tokenizers import Tokenizer
from tokenizers.models import BPE
from tokenizers.pre_tokenizers import ByteLevel as ByteLevelPre
from tokenizers.decoders import ByteLevel as ByteLevelDec
from tokenizers.trainers import BpeTrainer


def train(corpus_dir: str, model_path: str, vocab_size: int) -> None:
    files = sorted(glob.glob(os.path.join(corpus_dir, "**", "*.ll"), recursive=True))
    tok = Tokenizer(BPE())
    tok.pre_tokenizer = ByteLevelPre(add_prefix_space=False)
    tok.decoder = ByteLevelDec()
    trainer = BpeTrainer(vocab_size=vocab_size, initial_alphabet=ByteLevelPre.alphabet())
    tok.train(files, trainer)
    tok.save(model_path)
```

- [ ] **Step 4: Extend the CLI to use a model when given**

Replace `research/text-codec/text_codec/__main__.py` with:

```python
"""Track C reversible text codec.

Without --model: space-separated byte ids (0-255), lossless by construction.
With --model: a trained byte-level BPE (still lossless; shorter sequences).
"""
import argparse
import sys

from .train import train as _train


def _encode_bytes(in_path, out_path):
    data = open(in_path, "rb").read()
    open(out_path, "w").write(" ".join(str(b) for b in data))


def _decode_bytes(in_path, out_path):
    ids = open(in_path).read().split()
    open(out_path, "wb").write(bytes(int(x) for x in ids))


def _encode_bpe(in_path, out_path, model):
    from tokenizers import Tokenizer
    tok = Tokenizer.from_file(model)
    text = open(in_path, "rb").read().decode("latin-1")
    ids = tok.encode(text).ids
    open(out_path, "w").write(" ".join(str(i) for i in ids))


def _decode_bpe(in_path, out_path, model):
    from tokenizers import Tokenizer
    tok = Tokenizer.from_file(model)
    ids = [int(x) for x in open(in_path).read().split()]
    text = tok.decode(ids)
    open(out_path, "wb").write(text.encode("latin-1"))


def main(argv=None):
    ap = argparse.ArgumentParser(prog="text_codec")
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("encode", "decode"):
        p = sub.add_parser(name)
        p.add_argument("in_path")
        p.add_argument("out_path")
        p.add_argument("--model", default=None)
    pt = sub.add_parser("train")
    pt.add_argument("corpus_dir")
    pt.add_argument("model_path")
    pt.add_argument("--vocab-size", type=int, default=2000)
    args = ap.parse_args(argv)

    if args.cmd == "train":
        _train(args.corpus_dir, args.model_path, args.vocab_size)
    elif args.cmd == "encode":
        (_encode_bpe(args.in_path, args.out_path, args.model) if args.model
         else _encode_bytes(args.in_path, args.out_path))
    elif args.cmd == "decode":
        (_decode_bpe(args.in_path, args.out_path, args.model) if args.model
         else _decode_bytes(args.in_path, args.out_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
```

Note: `latin-1` round-trips bytes 0–255 one-to-one, so byte-level BPE over the latin-1 view stays exact.

- [ ] **Step 5: Run the trained round-trip + all prior tests**

Run: `cd research/text-codec && pip install -r requirements.txt && python -m pytest test_text_codec.py -v`
Expected: PASS (byte tests + trained BPE test). If `tokenizers` decode does not reproduce bytes exactly, the test fails loudly — do not relax it; fix the encoder/decoder (the byte-level pre-tokenizer + decoder pairing is what guarantees exactness).

- [ ] **Step 6: Commit**

```bash
git add research/text-codec
git commit -m "Add lossless byte-level BPE training to Track C

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

### Task 1.4: Record the first eval snapshot

**Files:** Create `research/EVAL.md`.

- [ ] **Step 1: Generate the table**

Run:

```bash
cd research && PYTHONPATH="$PWD/text-codec:$PWD/harness" LLVM_BIN=/usr/lib/llvm-22/bin \
  python harness/eval.py --tracks identity,text --partitions core > EVAL.md
cat EVAL.md
```

Expected: a table with `text | core | 8/8 | <tokens> | <bytes/token>`.

- [ ] **Step 2: Commit**

```bash
git add research/EVAL.md
git commit -m "Record first eval snapshot (Track C vs identity, core corpus)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Follow-on plans (roadmap — each needs its own spec + plan)

These phases implement the structured tracks, where the lossless pain points (spec §4, P1–P7) actually live. They are **not** detailed here because the structured **token schema is itself a design artifact** — it should be brainstormed into its own spec (`research/structured-codec`’s encoding format) before fine-grained TDD tasks can be written without speculation. Each phase below produces working, testable software gated by the same harness.

### Phase 2 — Track B (fidelity-first structured codec)
Own spec first (token schema: how references, types, modifiers, constants, module/function scope are encoded). Then a plan with one TDD task group per pain point, each ending in a green round-trip test on the relevant `corpus/core` module:
- **P1 references:** build a `ModuleSlotTracker`-based id space; encode BB/arg/global/instruction operands as exact ids. Test: `phi-self-ref.ll`, `phi-forward-ref.ll`, `globals-decls.ll` round-trip.
- **P2 types:** per-module interned type table; encode full types incl. opaque-pointer off-operand element types. Test: `opaque-ptr.ll`, `structs.ll`.
- **P3 modifiers:** per-opcode modifier tokens (predicates, flags, atomics, GEP inbounds+srcty, phi incoming blocks, switch cases). Test: dedicated per-opcode modules added to corpus.
- **P4 constants:** exact APInt/APFloat (raw bits), aggregates, constexpr, null/undef/poison. Test: `wide-int.ll`, `float-semantics.ll`.
- **P5 module scope + P6 function frame:** prologue/header tokens; deterministic order. Test: `globals-decls.ll` and a multi-function module.
- **P7 detokenizer:** two-pass `IRBuilder` reconstruction + `verifyModule`. Test: full `corpus/core` round-trip green (modulo metadata).

### Phase 3 — Track A (hybrid structured + sidecar)
Own spec: the structured-stream vocabulary and the sidecar schema (position-keyed literals). Plan reuses Track B’s reconstruction; tasks split the stream/sidecar and prove the structured vocab stays small while `corpus/core` round-trip stays green.

### Phase 4 — Comparison writeup
Run `eval.py` across `identity,text,structured-codec,hybrid`; extend metrics with structured-vocab size and ML-signal notes; write `research/EVAL.md` + a short analysis. (No new spec needed — uses existing harness.)

### Phase 5 — Metadata/debug (stretch)
Extend Tracks B and A to reconstruct attached metadata, `!dbg`, named/distinct nodes; gate on `corpus/metadata`. Own spec (metadata is a large sub-domain).

---

## Self-review

**Spec coverage:** Decision 1 (structural gate) → Tasks 0.4, 0.6. Decision 2 (modulo metadata) → corpus partitions (0.3) + Phase 5. Decision 3 (subdirs/one branch) → 0.2 file structure. Decision 4 (v1.0.0 release) → 0.1. Decision 5 (pin LLVM 22) → all harness commands. Decision 6 (order C→B→A) → Phases 1/2/3. Spec §4 pain points P1–P7 → Phase 2 roadmap; P8 → 0.4; P9 → pinned LLVM 22. Track C → Phase 1. Eval (§6) → 0.7, 1.4. Gap: structured tracks intentionally deferred to follow-on plans (stated explicitly).

**Placeholder scan:** Phases 2–5 are roadmap-by-design (the only non-detailed section), flagged with the reason (token schema is unspecified). Phases 0–1 contain complete code and exact commands.

**Type/name consistency:** `roundtrip(track, module, llvm_bin) -> (bool, str)` used identically in 0.6, 0.7, 1.2. `TRACKS` keys `identity`/`text` consistent across 0.5/0.7/1.2/1.4. `canonical(ll_path, llvm_bin)` consistent in 0.4/0.6. `text_codec` CLI verbs `encode`/`decode`/`train` and `--model` consistent across 1.1/1.3/0.5.
