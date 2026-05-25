; A self-referential phi (%x references itself across the loop back-edge)
; produces an operand-reference distance of zero. Serialize mode must emit it
; rather than aborting on the SSA-positive-distance assertion.
; Regression test for assert(InstructionDistance > 0).
; RUN: %llvm-tokenizer -mode=serialize %s | FileCheck %s

define i64 @loop(i64 %n) {
entry:
  br label %loop
loop:
  %x = phi i64 [ 0, %entry ], [ %x, %loop ]
  %c = icmp slt i64 %x, %n
  br i1 %c, label %loop, label %done
done:
  ret i64 %x
}

; CHECK: name: loop
; The self-reference serializes to the distance-0 slot (InstructionOperandIndex
; + 0 == 1), which is independent of the LLVM version. Its presence proves the
; token stream was produced instead of aborting.
; CHECK: tokens: [{{.*}}, 1, {{.*}}]
