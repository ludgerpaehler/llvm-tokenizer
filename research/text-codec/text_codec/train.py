"""Train a byte-level BPE tokenizer over the corpus.

Byte-level alphabet keeps all 256 bytes as base tokens, so encode/decode stays
lossless regardless of the learned merges; training only shortens sequences.
"""
import glob
import os

from tokenizers import Tokenizer
from tokenizers.models import BPE
from tokenizers.pre_tokenizers import ByteLevel as ByteLevelPre
from tokenizers.decoders import ByteLevel as ByteLevelDec
from tokenizers.trainers import BpeTrainer


def train(corpus_dir: str, model_path: str, vocab_size: int) -> None:
    files = sorted(glob.glob(os.path.join(corpus_dir, "**", "*.ll"), recursive=True))
    tok = Tokenizer(BPE())
    tok.pre_tokenizer = ByteLevelPre(add_prefix_space=False)
    tok.decoder = ByteLevelDec()
    trainer = BpeTrainer(vocab_size=vocab_size, initial_alphabet=ByteLevelPre.alphabet())
    tok.train(files, trainer)
    tok.save(model_path)
