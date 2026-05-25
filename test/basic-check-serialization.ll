; RUN: %llvm-tokenizer %s -mode=serialize -int-constants-list=%S/data/integers1.csv -print-serialization-config | FileCheck %s --check-prefixes=CHECK,CHECK-%llvm_major

define i64 @f1(i64 %a, i64 %b) {
	%sum = add i64 %a, %b
	%sum2 = add i64 %sum, 5
	ret i64 %sum2
}

; CHECK: config {
; CHECK:   PaddingTokenIndex: 0
; CHECK:   InstructionOperandRange {
; CHECK:     Begin: 1
; CHECK:     End: 33
; CHECK:   }
; CHECK:   ConstantOperandRange {
; CHECK:     Begin: 34
; CHECK:     End: 39
; CHECK:   }
; CHECK:   ConstantFloatOperandIndex: 40
; CHECK:   ConstantGlobalValueIndex: 41
; CHECK:   UnknownConstantOperandIndex: 42
; CHECK:   BasicBlockOperandIndex: 43
; CHECK:   InlineASMOperandIndex: 44
; CHECK:   ArgumentOperandIndex: 45
; CHECK:   UnknownOperandIndex: 46
; CHECK:   OpcodeRange {
; CHECK:     Begin: 47
; CHECK-19:     End: 114
; CHECK-22:     End: 115
; CHECK:   }
; CHECK:   TypeRange {
; CHECK-19:     Begin: 115
; CHECK-22:     Begin: 116
; CHECK:     End: 137
; CHECK:   }
; CHECK:   AttributeRange {
; CHECK:     Begin: 138
; CHECK-19:     End: 231
; CHECK-22:     End: 240
; CHECK:   }
; CHECK-19: FunctionStart: 232
; CHECK-19: FunctionEnd: 233
; CHECK-22: FunctionStart: 241
; CHECK-22: FunctionEnd: 242
; CHECK: }

; The serialized token stream is LLVM-version-specific because LLVM's opcode,
; type, and attribute enum values differ between releases. The expected streams:
;   LLVM 19: [232, 60, 128, 45, 45, 60, 128, 2, 38, 48, 122, 2, 233]
;   LLVM 22: [241, 60, 128, 45, 45, 60, 128, 2, 38, 48, 123, 2, 242]
; Reading the LLVM 19 stream:
;   232 - function start token
;   60  - the first add opcode (OpcodeIndex 47 + opcode 13)
;   128 - the i64 result type of the first instruction (TypeIndex 115 + 13)
;   45  - argument operand (%a)
;   45  - argument operand (%b)
;   60  - the second add opcode
;   128 - i64 result type of the second instruction
;   2   - instruction reference, one instruction back (InstructionOperandIndex 1 + 1)
;   38  - the integer constant 5 (ConstantIntegerOperandIndex 34 + 5 - 1)
;   48  - the ret opcode (47 + 1)
;   122 - the void return type (TypeIndex 115 + 7)
;   2   - instruction reference to the immediately preceding instruction
;   233 - function end token
; For LLVM 22 the void return type token shifts from 122 to 123 (TypeIndex moves
; from 115 to 116), and the function start/end tokens become 241/242.

; CHECK: functions [
; CHECK:   {
; CHECK:     name: f1
; CHECK-19:     tokens: [232, 60, 128, 45, 45, 60, 128, 2, 38, 48, 122, 2, 233]
; CHECK-22:     tokens: [241, 60, 128, 45, 45, 60, 128, 2, 38, 48, 123, 2, 242]
; CHECK:   }
; CHECK: ]
