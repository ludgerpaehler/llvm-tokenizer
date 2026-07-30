define i32 @load_elt(ptr %p, i64 %idx) {
  %slot = alloca [4 x i32], align 16
  %gep = getelementptr inbounds [4 x i32], ptr %slot, i64 0, i64 %idx
  store i32 7, ptr %gep, align 4
  %v = load i32, ptr %gep, align 4
  ret i32 %v
}
