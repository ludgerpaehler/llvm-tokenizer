define i32 @f() !dbg !4 {
  ret i32 0, !dbg !7
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, emissionKind: FullDebug)
!1 = !DIFile(filename: "f.c", directory: "/tmp")
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, unit: !0)
!7 = !DILocation(line: 1, column: 1, scope: !4)
