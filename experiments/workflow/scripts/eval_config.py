"""Evaluate one campaign cell and write a metrics JSON.

A cell is either a trained Track-C model at a given vocab size, or the raw-byte
`identity` baseline. For each module in the partition we encode/decode, require
the reconstruction to assemble, and compare canonical forms (reusing
research/harness/normalize.canonical). We also record mean token count and mean
bytes/token, mirroring research/harness/eval.py's formulas.
"""
import difflib
import json
import os
import subprocess
import sys
import tempfile

import _common
from _common import partition_modules
from normalize import canonical

PY = sys.executable


def _codec_argv(cmd, model):
    argv = [PY, "-m", "text_codec", cmd]
    return argv, (["--model", model] if model else [])


def _roundtrip_module(module, llvm_bin, model):
    """Return (ok, token_count, diff) for one module using the text codec."""
    enc, enc_model = _codec_argv("encode", model)
    dec, dec_model = _codec_argv("decode", model)
    with tempfile.TemporaryDirectory() as d:
        tokens = os.path.join(d, "tokens")
        recon = os.path.join(d, "recon.ll")
        subprocess.run(enc + [module, tokens] + enc_model, check=True)
        subprocess.run(dec + [tokens, recon] + dec_model, check=True)
        subprocess.run([f"{llvm_bin}/llvm-as", recon, "-o", os.devnull], check=True)
        token_count = len(open(tokens, "rb").read().split())
        want = canonical(module, llvm_bin)
        got = canonical(recon, llvm_bin)
        if want == got:
            return True, token_count, ""
        diff = "\n".join(difflib.unified_diff(
            want.decode(errors="replace").splitlines(),
            got.decode(errors="replace").splitlines(),
            fromfile="original", tofile="reconstructed", lineterm="",
        ))
        return False, token_count, diff


def _roundtrip_identity(module, llvm_bin):
    """Raw-byte baseline: the file is copied through unchanged."""
    token_count = len(open(module, "rb").read().split())
    canonical(module, llvm_bin)  # must assemble
    return True, token_count, ""


def _roundtrip_tiktoken(module, llvm_bin, enc):
    """Return (ok, token_count, diff) for one module using a tiktoken encoding.

    tiktoken byte-level BPE is lossless for the ASCII/UTF-8 text of .ll modules:
    encode_ordinary -> decode_bytes reproduces the exact input bytes.
    """
    with tempfile.TemporaryDirectory() as d:
        recon = os.path.join(d, "recon.ll")
        text = open(module, "rb").read().decode("utf-8")
        ids = enc.encode_ordinary(text)
        open(recon, "wb").write(enc.decode_bytes(ids))
        subprocess.run([f"{llvm_bin}/llvm-as", recon, "-o", os.devnull], check=True)
        want = canonical(module, llvm_bin)
        got = canonical(recon, llvm_bin)
        if want == got:
            return True, len(ids), ""
        diff = "\n".join(difflib.unified_diff(
            want.decode(errors="replace").splitlines(),
            got.decode(errors="replace").splitlines(),
            fromfile="original", tofile="reconstructed", lineterm="",
        ))
        return False, len(ids), diff


def evaluate(track, partition, llvm_bin, vocab_size, model, encoding=None):
    modules = partition_modules(partition)
    enc = None
    if track == "tiktoken":
        import tiktoken
        enc = tiktoken.get_encoding(encoding)
        # Fixed pretrained vocab -> the row's x-axis position on the vocab plots.
        vocab_size = enc.n_vocab
    passes = 0
    lengths, ratios, failures = [], [], []
    for m in modules:
        if track == "identity":
            ok, n, diff = _roundtrip_identity(m, llvm_bin)
        elif track == "tiktoken":
            ok, n, diff = _roundtrip_tiktoken(m, llvm_bin, enc)
        else:
            ok, n, diff = _roundtrip_module(m, llvm_bin, model)
        passes += int(ok)
        lengths.append(n)
        ratios.append(os.path.getsize(m) / max(n, 1))
        if not ok:
            failures.append({"module": os.path.basename(m), "diff": diff})
    total = len(modules)
    return {
        "track": track,
        "partition": partition,
        "vocab_size": vocab_size,
        "encoding": encoding,
        "modules": total,
        "roundtrip_passes": passes,
        "roundtrip_pass_rate": passes / total if total else 0.0,
        "mean_tokens": sum(lengths) / len(lengths) if lengths else None,
        "mean_bytes_per_token": sum(ratios) / len(ratios) if ratios else None,
        "failures": failures,
    }


if __name__ == "__main__":
    track = snakemake.params.track  # noqa: F821
    model = (snakemake.input.get("model", None)  # noqa: F821
             if track == "text" else None)
    result = evaluate(
        track=track,
        partition=snakemake.params.partition,  # noqa: F821
        llvm_bin=snakemake.params.llvm_bin,  # noqa: F821
        vocab_size=snakemake.params.get("vocab_size", None),  # noqa: F821
        model=model,
        encoding=snakemake.params.get("encoding", None),  # noqa: F821
    )
    os.makedirs(os.path.dirname(snakemake.output.metrics), exist_ok=True)  # noqa: F821
    with open(snakemake.output.metrics, "w") as f:  # noqa: F821
        json.dump(result, f, indent=2)
