; RUN: %llvm-tokenizer %s | FileCheck %s

define float @f(float %a) {
	%sum = fadd float %a, 3.0
	%sum2 = fadd float %sum, 4.0
	ret float %sum2
}

; CHECK:         type: constant_float_operand
; CHECK:         float_constant:  3.0
; CHECK:         type: constant_float_operand
; CHECK:         float_constant:  4.0
