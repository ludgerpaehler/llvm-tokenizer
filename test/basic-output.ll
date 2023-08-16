; RUN: %llvm-tokenizer %s | FileCheck %s
; RUN: %llvm-tokenizer -output-mode=json -pretty-print %s | FileCheck %s -check-prefix=CHECK-JSON

define i64 @f2(i64 %a, i64 %b) {
  %sum = add i64 %a, %b
	ret i64 %sum
}

; CHECK: functions [
; CHECK:   {
; CHECK:     name: f2
; CHECK:     tokens [
; CHECK:       {
; CHECK:         type: opcode
; CHECK:         instruction_index: 0
; CHECK:         opcode: 13
; CHECK:       }
; CHECK:       {
; CHECK:         type: argument_operand
; CHECK:         instruction_index: 0
; CHECK:       }
; CHECK:       {
; CHECK:         type: argument_operand
; CHECK:         instruction_index: 0
; CHECK:       }
; CHECK:       {
; CHECK:         type: opcode
; CHECK:         instruction_index: 1
; CHECK:         opcode: 1
; CHECK:       }
; CHECK:       {
; CHECK:         type: instruction_operand
; CHECK:         instruction_index: 1
; CHECK:         instruction_reference: 0
; CHECK:       }
; CHECK:     ]
; CHECK:   }
; CHECK: ]

; CHECK-JSON: "functions": [
; CHECK-JSON:   {
; CHECK-JSON:     "name": "f2",
; CHECK-JSON:     "tokens": [
; CHECK-JSON:       {
; CHECK-JSON:         "type": "opcode",
; CHECK-JSON:         "instruction_index": 0,
; CHECK-JSON:         "opcode": 13
; CHECK-JSON:       },
; CHECK-JSON:       {
; CHECK-JSON:         "type": "argument_operand",
; CHECK-JSON:         "instruction_index": 0
; CHECK-JSON:       },
; CHECK-JSON:       {
; CHECK-JSON:         "type": "argument_operand",
; CHECK-JSON:         "instruction_index": 0
; CHECK-JSON:       },
; CHECK-JSON:       {
; CHECK-JSON:         "type": "opcode",
; CHECK-JSON:         "instruction_index": 1,
; CHECK-JSON:         "opcode": 1
; CHECK-JSON:       },
; CHECK-JSON:       {
; CHECK-JSON:         "type": "instruction_operand",
; CHECK-JSON:         "instruction_index": 1,
; CHECK-JSON:         "instruction_reference": 0
; CHECK-JSON:       }
; CHECK-JSON:     ]
; CHECK-JSON:   }
; CHECK-JSON: ]
