"""Train one byte-level BPE model for a given vocab size.

Thin wrapper over research/text-codec/text_codec/train.py so the sweep reuses the
exact training logic the round-trip tests validate. Invoked by the Snakemake
`train` rule; parameters arrive via the `snakemake` global.
"""
import os

import _common  # noqa: F401  (sets sys.path for the text_codec import below)
from text_codec.train import train


def main(corpus_dir, model_path, vocab_size):
    os.makedirs(os.path.dirname(model_path), exist_ok=True)
    train(corpus_dir, model_path, int(vocab_size))


if __name__ == "__main__":
    # Snakemake injects the `snakemake` object into the module namespace.
    main(
        corpus_dir=snakemake.input.corpus,  # noqa: F821
        model_path=snakemake.output.model,  # noqa: F821
        vocab_size=snakemake.params.vocab_size,  # noqa: F821
    )
