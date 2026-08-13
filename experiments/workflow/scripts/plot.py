"""Render campaign plots from results.csv.

One connected line per corpus partition for the corpus-trained Track C models
(x = BPE vocab size), for three measures: mean bytes/token, mean tokens, and
round-trip pass rate. The raw-byte identity baseline (no vocab size) is drawn as
a dashed horizontal reference line. OpenAI tiktoken encodings are overlaid as
star markers at their own n_vocab, so tokenizer *family* (trained vs. tiktoken)
is carried by marker shape, and partition by color.

Colors follow the data-viz reference palette (categorical slots, fixed order),
text stays in ink tokens, and series are direct-labeled so identity is never
carried by color alone.
"""
import csv
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Categorical slots (light mode), assigned in fixed order — never cycled.
SERIES_COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
INK_PRIMARY = "#0b0b0b"
INK_SECONDARY = "#52514e"
INK_MUTED = "#898781"
GRID = "#e1e0d9"
SURFACE = "#fcfcfb"

MEASURES = [
    ("mean_bytes_per_token", "Bytes / token", "bytes_per_token"),
    ("mean_tokens", "Mean tokens", "mean_tokens"),
    ("roundtrip_pass_rate", "Round-trip pass rate", "roundtrip_pass_rate"),
]


def _load(csv_path):
    with open(csv_path) as f:
        return list(csv.DictReader(f))


def _num(v):
    return float(v) if v not in (None, "") else None


def _series_by_partition(rows, column):
    """partition -> sorted [(vocab, value)] for corpus-trained Track C rows."""
    series = defaultdict(list)
    for r in rows:
        if r["track"] != "text":
            continue
        vocab = r.get("vocab_size")
        val = _num(r.get(column))
        if vocab in (None, "") or val is None:
            continue
        series[r["partition"]].append((int(vocab), val))
    for p in series:
        series[p].sort()
    return series


def _tiktoken_by_partition(rows, column):
    """partition -> [(n_vocab, value, encoding)] for tiktoken rows."""
    points = defaultdict(list)
    for r in rows:
        if r["track"] != "tiktoken":
            continue
        vocab = r.get("vocab_size")
        val = _num(r.get(column))
        if vocab in (None, "") or val is None:
            continue
        points[r["partition"]].append((int(vocab), val, r.get("encoding") or ""))
    for p in points:
        points[p].sort()
    return points


def _baselines(rows, column):
    """partition -> identity-baseline value."""
    out = {}
    for r in rows:
        if r["track"] == "identity":
            v = _num(r.get(column))
            if v is not None:
                out[r["partition"]] = v
    return out


def _style_axes(ax):
    ax.set_facecolor(SURFACE)
    ax.grid(True, color=GRID, linewidth=0.8, zorder=0)
    ax.set_axisbelow(True)
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)
    for spine in ("left", "bottom"):
        ax.spines[spine].set_color("#c3c2b7")
    ax.tick_params(colors=INK_MUTED, labelsize=9)


def _plot_measure(rows, column, ylabel, out_stub):
    series = _series_by_partition(rows, column)
    tiktoken = _tiktoken_by_partition(rows, column)
    baselines = _baselines(rows, column)
    # Colors follow the partition (entity), consistent across families.
    partitions = sorted(set(series) | set(tiktoken))
    color_for = {p: SERIES_COLORS[i % len(SERIES_COLORS)]
                 for i, p in enumerate(partitions)}

    fig, ax = plt.subplots(figsize=(6.4, 4.0), dpi=150)
    fig.patch.set_facecolor(SURFACE)
    _style_axes(ax)

    for part in partitions:
        color = color_for[part]
        xs = [v for v, _ in series.get(part, [])]
        ys = [y for _, y in series.get(part, [])]
        if xs:
            ax.plot(xs, ys, color=color, linewidth=2.0, marker="o",
                    markersize=6, label=part, zorder=3)
            ax.annotate(part, (xs[-1], ys[-1]), color=color, fontsize=9,
                        xytext=(6, 0), textcoords="offset points",
                        va="center", fontweight="bold")
        if part in baselines:
            ax.axhline(baselines[part], color=color, linewidth=1.2,
                       linestyle="--", alpha=0.6, zorder=1)
        # tiktoken overlaid as stars at each encoding's n_vocab.
        for vx, vy, enc_name in tiktoken.get(part, []):
            ax.scatter([vx], [vy], color=color, marker="*", s=150,
                       edgecolors=SURFACE, linewidths=0.8, zorder=4)
            ax.annotate(enc_name, (vx, vy), color=INK_SECONDARY, fontsize=8,
                        xytext=(0, 8), textcoords="offset points",
                        ha="center", va="bottom")

    # Log x: Track C sweep (<=4k) and tiktoken (50k-200k) span two orders of
    # magnitude; linear scale would crush the trained sweep against the axis.
    ax.set_xscale("log")
    ax.set_xlabel("Vocab size (BPE vocab / tiktoken n_vocab), log scale",
                  color=INK_SECONDARY, fontsize=10)
    ax.set_ylabel(ylabel, color=INK_SECONDARY, fontsize=10)
    ax.set_title(f"{ylabel} vs. vocab size", color=INK_PRIMARY,
                 fontsize=12, fontweight="bold", loc="left", pad=10)

    # Family legend: shape distinguishes trained vs tiktoken (never color alone).
    from matplotlib.lines import Line2D
    handles = [
        Line2D([0], [0], color=INK_MUTED, marker="o", linewidth=2.0,
               markersize=6, label="Track C (corpus-trained)"),
        Line2D([0], [0], color=INK_MUTED, marker="*", linewidth=0,
               markersize=12, label="tiktoken (pretrained)"),
    ]
    if tiktoken:
        ax.legend(handles=handles, frameon=False, fontsize=8,
                  labelcolor=INK_SECONDARY, loc="best")
    elif len(partitions) >= 2:
        ax.legend(frameon=False, fontsize=9, labelcolor=INK_SECONDARY)
    fig.tight_layout()

    for ext in ("png", "svg"):
        fig.savefig(f"{out_stub}.{ext}", facecolor=SURFACE, bbox_inches="tight")
    plt.close(fig)


def main(csv_path, out_dir, out_files):
    os.makedirs(out_dir, exist_ok=True)
    rows = _load(csv_path)
    for column, ylabel, stub in MEASURES:
        _plot_measure(rows, column, ylabel, os.path.join(out_dir, stub))
    # Touch a sentinel so the rule has a single stable output to depend on.
    with open(out_files, "w") as f:
        f.write("ok\n")


if __name__ == "__main__":
    main(
        csv_path=snakemake.input.csv,  # noqa: F821
        out_dir=snakemake.params.plot_dir,  # noqa: F821
        out_files=snakemake.output.sentinel,  # noqa: F821
    )
