; Test that we can read textual IR from STDIN
; RUN: cat %s | %llvm-tokenizer - | FileCheck %s
; Test that we can read bitcode from a file
; RUN opt-17 %s -o %t
; RUN %llvm-tokenizer %t | FileCheck %s
; Test that we can read bitcode from STDIN
; RUN opt-17 %s | %llvm-tokenizer | FileCheck %s

define i64 @f() {
	ret i64 1
}

; CHECK: [
; CHECK:   {
; CHECK:     name: f
; CHECK:     tokens [
; CHECK:       {
; CHECK:         type: opcode
; CHECK:         instruction_index: 0
; CHECK:         opcode: 1
; CHECK:       }
; CHECK:       {
; CHECK:         type: constant_integer_operand
; CHECK:         instruction_index: 0
; CHECK:         integer_constant: 1
; CHECK:       }
; CHECK:     ]
; CHECK:   }
; CHECK: ]
