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
