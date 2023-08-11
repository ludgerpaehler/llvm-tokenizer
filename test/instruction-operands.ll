; RUN: %llvm-tokenizer %s | FileCheck %s

define i64 @f1(i64 %a, i64 %b) {
	%sum = add i64 %a, %b
	%dif = sub i64 %a, %b
	%total = add i64 %sum, %dif
	ret i64 %total
}

; CHECK:         type: instruction_operand
; CHECK:         instruction_reference: 0
; CHECK:         type: instruction_operand
; CHECK:         instruction_reference: 1
; CHECK:         type: instruction_operand
; CHECK:         instruction_reference: 2

