@g = global i32 42, align 4
@ro = constant [3 x i8] c"hi\00"
@alias = alias i32, ptr @g

declare i32 @puts(ptr)

define i32 @use() {
  %p = getelementptr [3 x i8], ptr @ro, i64 0, i64 0
  %r = call i32 @puts(ptr %p)
  %v = load i32, ptr @g, align 4
  ret i32 %v
}
