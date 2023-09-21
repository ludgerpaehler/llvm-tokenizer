; RUN: %llvm-tokenizer %s | FileCheck %s

define i64 @f2(i64 %a, i64 %b) {
  %sum = add i64 %a, %b
  ret i64 %sum
}

declare i64 @f3(i64 %a)

; CHECK: name: f2
; CHECK-NOT: name: f3

