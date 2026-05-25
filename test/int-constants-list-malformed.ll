; A malformed or out-of-range line in the integer-constants list must not crash
; the tokenizer. Previously parsing used std::stol, which threw
; std::invalid_argument on non-numeric lines and std::out_of_range on values
; that overflow `long`; both aborted the process. Such lines are now skipped,
; and the surviving valid constant keeps a dense, contiguous token id.
;
; RUN: echo 'not_a_number' > %t.list
; RUN: echo '99999999999999999999999999999999' >> %t.list
; RUN: echo '7' >> %t.list
; RUN: %llvm-tokenizer -mode=serialize -int-constants-list=%t.list %s | FileCheck %s

define i64 @f() {
  ret i64 7
}

; The two bad lines are skipped, so the valid constant 7 is the first (and only)
; listed constant: token id 0, i.e. ConstantIntegerOperandIndex + 0 == 34 with
; the default max-instruction-operand-reference-diff. (35 would be the shared
; out-of-range slot, which is what a non-dense id would have collided with.)
; CHECK: name: f
; CHECK: tokens: [{{.*}}, 34, {{.*}}]
