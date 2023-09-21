; RUN: %llvm-tokenizer %s | FileCheck %s
; RUN: %llvm-tokenizer %s -mode=serialize | FileCheck %s -check-prefix=CHECK-SERIALIZED

define i64 @f2(i64 %a, i64 %b) optnone {
  %sum = add i64 %a, %b
  ret i64 %sum
}

; CHECK: type: attribute
; CHECK: instruction_index: 0
; CHECK: attribute_id: 42

; CHECK-SERIALIZED: tokens: [334, 175, 55, 123, 40, 40, 43, 117, 2, 335]

