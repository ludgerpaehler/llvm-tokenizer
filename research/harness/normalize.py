"""Canonicalize LLVM IR for structural round-trip comparison.

A module is canonicalized by assembling then disassembling it
(`llvm-as | llvm-dis`), which applies LLVM's own normalization (value
renumbering, attribute ordering, float printing). Two volatile lines that depend
only on the input path are stripped so reconstructed modules compare equal.
"""
import subprocess


def _strip_volatile(ir: bytes) -> bytes:
    keep = []
    for line in ir.split(b"\n"):
        if line.startswith(b"; ModuleID =") or line.startswith(b"source_filename ="):
            continue
        keep.append(line)
    return b"\n".join(keep)


def canonical(ll_path: str, llvm_bin: str) -> bytes:
    asm = subprocess.run(
        [f"{llvm_bin}/llvm-as", ll_path, "-o", "-"],
        check=True, capture_output=True,
    ).stdout
    dis = subprocess.run(
        [f"{llvm_bin}/llvm-dis", "-o", "-"],
        input=asm, check=True, capture_output=True,
    ).stdout
    return _strip_volatile(dis)
