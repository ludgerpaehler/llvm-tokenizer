; RUN: %llvm-tokenizer %s | FileCheck %s --check-prefixes=CHECK,CHECK-%llvm_major
; RUN: %llvm-tokenizer %s -mode=serialize | FileCheck %s --check-prefix=CHECK-SERIALIZED-%llvm_major

define i64 @f2(i64 %a, i64 %b) optnone {
  %sum = add i64 %a, %b
  ret i64 %sum
}

; CHECK: type: attribute
; CHECK: instruction_index: 0
; CHECK-19: attribute_id: 46
; CHECK-22: attribute_id: 50

; The two opcode tokens (add, ret) are stored 0-based; every other token,
; including the optnone attribute, is unchanged.
; CHECK-SERIALIZED-19: tokens: [227, 179, 54, 123, 40, 40, 42, 117, 2, 228]
; CHECK-SERIALIZED-22: tokens: [236, 183, 54, 123, 40, 40, 42, 118, 2, 237]
