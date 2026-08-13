"""Merge per-cell metrics JSONs into results.csv + results.json with provenance.

The provenance block stamps the exact config, tool versions, LLVM version, git
commit, and corpus file list so any reported table or plot is traceable to its
inputs.
"""
import json
import os
import subprocess

import _common
from _common import REPO_ROOT, partition_modules

COLUMNS = [
    "track", "partition", "encoding", "vocab_size", "modules",
    "roundtrip_passes", "roundtrip_pass_rate",
    "mean_tokens", "mean_bytes_per_token",
]


def _git_commit():
    try:
        return subprocess.run(
            ["git", "-C", REPO_ROOT, "rev-parse", "HEAD"],
            check=True, capture_output=True, text=True,
        ).stdout.strip()
    except Exception:
        return None


def _llvm_version(llvm_bin):
    try:
        return subprocess.run(
            [f"{llvm_bin}/llvm-as", "--version"],
            check=True, capture_output=True, text=True,
        ).stdout.strip()
    except Exception:
        return None


def _tokenizers_version():
    try:
        import tokenizers
        return tokenizers.__version__
    except Exception:
        return None


def _tiktoken_version():
    try:
        import tiktoken
        return tiktoken.__version__
    except Exception:
        return None


def _csv_cell(v):
    if v is None:
        return ""
    if isinstance(v, float):
        return f"{v:.4f}"
    return str(v)


def aggregate(metric_files, config, llvm_bin, out_csv, out_json):
    rows = []
    for path in sorted(metric_files):
        with open(path) as f:
            rows.append(json.load(f))
    rows.sort(key=lambda r: (r["track"], r["partition"], r.get("vocab_size") or -1))

    lines = [",".join(COLUMNS)]
    for r in rows:
        lines.append(",".join(_csv_cell(r.get(c)) for c in COLUMNS))
    with open(out_csv, "w") as f:
        f.write("\n".join(lines) + "\n")

    corpus = {p: [os.path.basename(m) for m in partition_modules(p)]
              for p in config.get("partitions", [])}
    provenance = {
        "config": config,
        "git_commit": _git_commit(),
        "llvm_version": _llvm_version(llvm_bin),
        "tokenizers_version": _tokenizers_version(),
        "tiktoken_version": _tiktoken_version(),
        "corpus": corpus,
    }
    with open(out_json, "w") as f:
        json.dump({"provenance": provenance, "results": rows}, f, indent=2)


if __name__ == "__main__":
    aggregate(
        metric_files=list(snakemake.input.metrics),  # noqa: F821
        config=dict(snakemake.params.config),  # noqa: F821
        llvm_bin=snakemake.params.llvm_bin,  # noqa: F821
        out_csv=snakemake.output.csv,  # noqa: F821
        out_json=snakemake.output.json,  # noqa: F821
    )
