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
