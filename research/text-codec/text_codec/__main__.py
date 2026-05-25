"""Track C reversible text codec.

`encode <in> <out>` writes space-separated byte ids (0-255); `decode <in> <out>`
reconstructs the exact bytes. Byte-level mapping is lossless by construction.
"""
import sys


def encode(in_path, out_path):
    data = open(in_path, "rb").read()
    with open(out_path, "w") as f:
        f.write(" ".join(str(b) for b in data))


def decode(in_path, out_path):
    ids = open(in_path).read().split()
    with open(out_path, "wb") as f:
        f.write(bytes(int(x) for x in ids))


def main(argv):
    cmd, in_path, out_path = argv[0], argv[1], argv[2]
    if cmd == "encode":
        encode(in_path, out_path)
    elif cmd == "decode":
        decode(in_path, out_path)
    else:
        raise SystemExit(f"unknown command {cmd!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
