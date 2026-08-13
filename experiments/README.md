# Experiments

Reproducible experimental campaigns for the lossless-tokenizer research effort,
orchestrated with [Snakemake](https://snakemake.github.io/). Each campaign is a
DAG of independent jobs, so runs are task-parallel (`-jN`) and incrementally
cached, and every result carries a provenance stamp.

This folder reuses the existing research code under `../research/` (the Track C
byte-level BPE codec and the round-trip `canonical` gate) without modifying it.

## Campaign: `vocab_sweep`

Sweeps the Track C byte-level BPE **vocab size** — the one real tunable, which
the existing `research/harness/eval.py` never exercises (it only runs the raw
1-byte-per-token codec). For each vocab size we train a model over the corpus,
then per corpus partition measure:

- **round-trip pass rate** (must stay N/N — byte-level BPE is lossless by
  construction),
- **mean tokens** per module, and
- **mean bytes/token** (compression proxy).

The raw-byte `identity` codec is recorded as a baseline reference, and OpenAI's
`tiktoken` encodings (`gpt2`, `p50k_base`, `cl100k_base`, `o200k_base`) are
included as external comparison points. tiktoken ships *fixed pretrained*
byte-level BPE (no training, no vocab sweep), so each encoding is one comparison
row plotted at its own `n_vocab`. First run downloads the encodings to the
default tiktoken cache.

### DAG

```
train (1 job / vocab)  ->  eval_text (1 job / vocab x partition)  --.
eval_identity (1 job / partition)  ------------------------------------> aggregate -> plot
eval_tiktoken (1 job / encoding x partition, no train dep)  ----------'
```

## Environment

```bash
conda env create -f experiments/environment.yml   # or: mamba env create -f ...
conda activate llvm-tok-exp
```

LLVM's `llvm-as`/`llvm-dis` are provided externally. Point `LLVM_BIN` at your
install; it overrides the `llvm_bin` default in the config:

```bash
export LLVM_BIN=/usr/lib/llvm-22/bin   # or e.g. /usr/lib/llvm-20/bin
```

## Run

```bash
# Dry run — inspect the DAG and confirm it fans out per vocab size.
snakemake -s experiments/workflow/Snakefile -n

# Real run with 4 parallel jobs.
snakemake -s experiments/workflow/Snakefile -j4
```

## Outputs

Everything lands under `experiments/results/<campaign>/` (gitignored):

- `models/vocab-<N>/model.json` — trained tokenizer per vocab size
- `metrics/*.json` — one file per evaluated cell
- `results.csv` — flat table of all cells
- `results.json` — the same rows plus a `provenance` block (resolved config,
  git commit, LLVM version, `tokenizers` version, corpus file list)
- `plots/*.png`, `plots/*.svg` — bytes/token, mean-tokens, and pass-rate vs.
  vocab size

## Configuration

All sweep parameters live in `config/vocab_sweep.yaml` — vocab sizes,
partitions, whether to include the identity baseline, and the default LLVM bin
dir. Edit that file to reshape the campaign; the DAG adjusts automatically.

## Reproducibility

- Pinned toolchain via `environment.yml`.
- Single source of truth for parameters (`config/vocab_sweep.yaml`).
- Per-run provenance stamped into `results.json`.
- Byte-level BPE training is deterministic given corpus + vocab size, and the
  Snakemake DAG guarantees correct incremental re-execution (a second run with
  no input changes reports "nothing to be done").
