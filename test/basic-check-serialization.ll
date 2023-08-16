; RUN: %llvm-tokenizer %s -mode=serialize -int-constants-list=%S/data/integers1.csv -print-serialization-config | FileCheck %s

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
; CHECK:     End: 114
; CHECK:   }
; CHECK:   TypeRange {
; CHECK: 	   Begin: 115
; CHECK: 	   End: 137
; CHECK:   }
; CHECK: }

; We should end up with the following (serialized tokens) based on the
; input sequence and config above.
; 60 - The first add opcode (13+47)
; 128 - The output type of the first instruction. 13 is an Integer type
;       (128-115=13)
; 45 - Global argument operand
; 45 - Global argument operand
; 60 - The second add opcode
; 128 - Integer output type for the second instruction.
; 2 - Instruction reference operand, referencing the first instruction
;     (One instruction back, 1+1)
; 38 - An integer constant operand. This specific one being 5, (34 + 5 - 1),
;      subtracting one due to zero-based indexing.
; 48 - The return opcode
; 122 - The type of the return opcode, void in this case.
; 2 - Instruction reference operand, referencing the add instruction directly
;     before it.

; CHECK: functions [
; CHECK: 	{
; CHECK: 		name: f1
; CHECK: 		tokens: [60, 128, 45, 45, 60, 128, 2, 38, 48, 122, 2]
; CHECK: 	}
; CHECK: ]

