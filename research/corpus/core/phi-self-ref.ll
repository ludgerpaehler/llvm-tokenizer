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
