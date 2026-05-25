import os
import textwrap
import pytest
from normalize import canonical

LLVM_BIN = os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin")


def _write(tmp_path, name, text):
    p = tmp_path / name
    p.write_text(textwrap.dedent(text))
    return str(p)


def test_whitespace_differences_canonicalize_equal(tmp_path):
    a = _write(tmp_path, "a.ll", """
        define i64 @f(i64 %a) {
          ret i64 %a
        }
    """)
    b = _write(tmp_path, "b.ll", "define i64 @f(i64 %a){ret i64 %a}\n")
    assert canonical(a, LLVM_BIN) == canonical(b, LLVM_BIN)


def test_semantically_different_modules_differ(tmp_path):
    a = _write(tmp_path, "a.ll", "define i64 @f(i64 %a){ret i64 %a}\n")
    b = _write(tmp_path, "b.ll", "define i64 @f(i64 %a){ret i64 0}\n")
    assert canonical(a, LLVM_BIN) != canonical(b, LLVM_BIN)
