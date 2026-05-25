"""Structural round-trip gate: encode -> decode -> normalize both -> diff.

A module round-trips losslessly iff the canonical form of the reconstruction
equals the canonical form of the input (see normalize.canonical and spec §2).
The reconstruction is also required to assemble (a fast verify).
"""
import argparse
import difflib
import os
import subprocess
import sys
import tempfile

from normalize import canonical
from tracks import TRACKS


def roundtrip(track: str, module: str, llvm_bin: str) -> tuple[bool, str]:
    spec = TRACKS[track]
    with tempfile.TemporaryDirectory() as d:
        tokens = os.path.join(d, "tokens")
        recon = os.path.join(d, "recon.ll")
        subprocess.run(spec["encode"] + [module, tokens], check=True)
        subprocess.run(spec["decode"] + [tokens, recon], check=True)
        # Fast fail: reconstruction must assemble.
        subprocess.run([f"{llvm_bin}/llvm-as", recon, "-o", os.devnull], check=True)
        want = canonical(module, llvm_bin)
        got = canonical(recon, llvm_bin)
        if want == got:
            return True, ""
        diff = "\n".join(difflib.unified_diff(
            want.decode(errors="replace").splitlines(),
            got.decode(errors="replace").splitlines(),
            fromfile="original", tofile="reconstructed", lineterm="",
        ))
        return False, diff


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("track")
    ap.add_argument("module")
    ap.add_argument("--llvm-bin", default=os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin"))
    args = ap.parse_args(argv)
    ok, diff = roundtrip(args.track, args.module, args.llvm_bin)
    if not ok:
        sys.stderr.write(diff + "\n")
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
