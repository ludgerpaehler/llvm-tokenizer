%pair = type { i32, i64 }
%node = type { i32, ptr }
%packed = type <{ i8, i32 }>

define %pair @mk(i32 %a, i64 %b) {
  %tmp0 = insertvalue %pair undef, i32 %a, 0
  %tmp1 = insertvalue %pair %tmp0, i64 %b, 1
  ret %pair %tmp1
}

define i32 @first(ptr %n) {
  %f = getelementptr %node, ptr %n, i64 0, i32 0
  %v = load i32, ptr %f
  ret i32 %v
}

define i8 @firstpacked(ptr %p) {
  %g = getelementptr %packed, ptr %p, i64 0, i32 0
  %v = load i8, ptr %g
  ret i8 %v
}
