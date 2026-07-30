"""Track C reversible text codec.

Without --model: space-separated byte ids (0-255), lossless by construction.
With --model: a trained byte-level BPE (still lossless; shorter sequences).
"""
import argparse
import sys

from .train import train as _train


def _encode_bytes(in_path, out_path):
    data = open(in_path, "rb").read()
    open(out_path, "w").write(" ".join(str(b) for b in data))


def _decode_bytes(in_path, out_path):
    ids = open(in_path).read().split()
    open(out_path, "wb").write(bytes(int(x) for x in ids))


def _encode_bpe(in_path, out_path, model):
    from tokenizers import Tokenizer
    tok = Tokenizer.from_file(model)
    text = open(in_path, "rb").read().decode("latin-1")
    ids = tok.encode(text).ids
    open(out_path, "w").write(" ".join(str(i) for i in ids))


def _decode_bpe(in_path, out_path, model):
    from tokenizers import Tokenizer
    tok = Tokenizer.from_file(model)
    ids = [int(x) for x in open(in_path).read().split()]
    text = tok.decode(ids)
    open(out_path, "wb").write(text.encode("latin-1"))


def main(argv=None):
    ap = argparse.ArgumentParser(prog="text_codec")
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("encode", "decode"):
        p = sub.add_parser(name)
        p.add_argument("in_path")
        p.add_argument("out_path")
        p.add_argument("--model", default=None)
    pt = sub.add_parser("train")
    pt.add_argument("corpus_dir")
    pt.add_argument("model_path")
    pt.add_argument("--vocab-size", type=int, default=2000)
    args = ap.parse_args(argv)

    if args.cmd == "train":
        _train(args.corpus_dir, args.model_path, args.vocab_size)
    elif args.cmd == "encode":
        (_encode_bpe(args.in_path, args.out_path, args.model) if args.model
         else _encode_bytes(args.in_path, args.out_path))
    elif args.cmd == "decode":
        (_decode_bpe(args.in_path, args.out_path, args.model) if args.model
         else _decode_bytes(args.in_path, args.out_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
