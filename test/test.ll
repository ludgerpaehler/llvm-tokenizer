; RUN: %llvm-tokenizer %s | grep token

define i64 @f2(i64 %a, i64 %b) {
    %sum = add i64 %a, %b
    ret i64 %sum
}
