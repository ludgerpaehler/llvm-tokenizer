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
