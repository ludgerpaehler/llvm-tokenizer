; RUN: %llvm-tokenizer %s -mode=serialize -int-constants-list=%S/data/integers1.csv | FileCheck %s

define i64 @f1(i64 %a, i64 %b) {
	%sum = add i64 %a, %b
	%sum2 = add i64 %sum, 5
	ret i64 %sum2
}

; CHECK: [
; CHECK:   {
; CHECK:     name: f1
; CHECK:     tokens: [59, 44, 44, 59, 2, 39, 38, 47, 2]
; CHECK:   }
; CHECK: ]

