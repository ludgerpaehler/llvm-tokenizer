; RUN: %llvm-tokenizer %s | FileCheck %s

@a = global i64 1

define i64 @f() {
	%b = load i64, i64* @a
	ret i64 %b
}

; CHECK:        type: global_value_operand
; CHECK:        instruction_index: 0

