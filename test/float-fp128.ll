; Simply check that we can run without crashing. We aren't actually handling
; these values at all yet.

; RUN: %llvm-tokenizer %s

define x86_fp80 @f1(x86_fp80 %a) {
  %sum = fadd x86_fp80 %a, 0xK4001A000000000000000
  ret x86_fp80 %sum
}

define fp128 @f2(fp128 %a) {
  %sum = fadd fp128 %a, 0xL00000000000000004001400000000000
  ret fp128 %sum
}

define ppc_fp128 @f3(ppc_fp128 %a) {
  %sum = fadd ppc_fp128 %a, 0xM40140000000000000000000000000000
  ret ppc_fp128 %sum
}

