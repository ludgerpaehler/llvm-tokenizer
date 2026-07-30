"""Run every track over the corpus and emit a Markdown comparison table.

Metrics per (track, partition): round-trip pass rate, mean token-sequence length,
and mean bytes/token compression vs the raw input. Token count is read from the
tokens file (whitespace-separated ints) when present; tracks whose tokens are not
ints report '-'.
"""
import argparse
import os
import subprocess
import tempfile

from roundtrip import roundtrip
from tracks import TRACKS

HERE = os.path.dirname(__file__)
CORPUS = os.path.join(HERE, "..", "corpus")


def _modules(partition):
    d = os.path.join(CORPUS, partition)
    return [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.endswith(".ll")]


def _token_count(track, module, llvm_bin):
    spec = TRACKS[track]
    with tempfile.TemporaryDirectory() as d:
        tokens = os.path.join(d, "tokens")
        subprocess.run(spec["encode"] + [module, tokens], check=True)
        data = open(tokens, "rb").read()
        try:
            return len(data.split())
        except Exception:
            return None


def evaluate(tracks, partitions, llvm_bin, per_track_modules):
    rows = []
    for track in tracks:
        for part in partitions:
            all_mods = _modules(part)
            subset = per_track_modules.get(track)
            mods = [m for m in all_mods if subset is None or os.path.basename(m) in subset]
            passes, lengths, ratios = 0, [], []
            for m in mods:
                ok, _ = roundtrip(track, m, llvm_bin)
                passes += int(ok)
                n = _token_count(track, m, llvm_bin)
                if n is not None:
                    lengths.append(n)
                    ratios.append(os.path.getsize(m) / max(n, 1))
            rows.append((track, part, passes, len(mods), lengths, ratios))
    return rows


def render(rows):
    out = ["| Track | Partition | Round-trip | Mean tokens | Bytes/token |",
           "|-------|-----------|-----------|-------------|-------------|"]
    for track, part, passes, total, lengths, ratios in rows:
        mt = f"{sum(lengths)/len(lengths):.0f}" if lengths else "-"
        br = f"{sum(ratios)/len(ratios):.2f}" if ratios else "-"
        out.append(f"| {track} | {part} | {passes}/{total} | {mt} | {br} |")
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracks", default="identity,text")
    ap.add_argument("--partitions", default="core")
    ap.add_argument("--llvm-bin", default=os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin"))
    ap.add_argument(
        "--modules-for-track",
        action="append",
        default=[],
        help="Limit one track to a CSV of module basenames: e.g. structured=identity-smoke.ll",
    )
    args = ap.parse_args(argv)
    per_track_modules = {}
    for spec in args.modules_for_track:
        track, csv = spec.split("=", 1)
        per_track_modules[track] = set(csv.split(","))
    rows = evaluate(
        args.tracks.split(","), args.partitions.split(","), args.llvm_bin,
        per_track_modules,
    )
    print(render(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
