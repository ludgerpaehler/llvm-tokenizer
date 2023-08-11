; RUN: %llvm-tokenizer %s | FileCheck %s

define i64 @f(i64 %a) {
	%sum = add i64 %a, 5
	%sum2 = add i64 %sum, 10
	ret i64 %sum2
}

; CHECK:         type: constant_integer_operand
; CHECK:         integer_constant: 5
; CHECK:         type: constant_integer_operand
; CHECK:         integer_constant: 10
