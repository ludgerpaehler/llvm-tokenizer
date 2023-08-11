; RUN: %llvm-tokenizer %s | FileCheck %s

define i64 @f2(i64 %a, i64 %b) {
  %sum = add i64 %a, %b
	ret i64 %sum
}

; CHECK: function {
; CHECK:   name: f2
; CHECK:   tokens [
; CHECK:     {
; CHECK:       type: opcode
; CHECK:       instruction_index: 0
; CHECK:       opcode: 13
; CHECK:     }
; CHECK:     {
; CHECK:       type: unknown_operand
; CHECK:       instruction_index: 0
; CHECK:     }
; CHECK:     {
; CHECK:       type: unknown_operand
; CHECK:       instruction_index: 0
; CHECK:     }
; CHECK:     {
; CHECK:       type: opcode
; CHECK:       instruction_index: 1
; CHECK:       opcode: 1
; CHECK:     }
; CHECK:     {
; CHECK:       type: instruction_operand
; CHECK:       instruction_index: 1
; CHECK:       instruction_reference: 0
; CHECK:     }
; CHECK:   ]
; CHECK: }
