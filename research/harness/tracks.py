"""Registry of track encode/decode commands.

Each track maps to an `encode` and `decode` argv prefix. The harness appends two
positional args to each: `encode <input.ll> <tokens>` and
`decode <tokens> <output.ll>`. The "identity" track copies the file through
unchanged and exists only to self-test the harness.
"""
import sys

PY = sys.executable

TRACKS = {
    "identity": {"encode": ["cp"], "decode": ["cp"]},
    "text": {
        "encode": [PY, "-m", "text_codec", "encode"],
        "decode": [PY, "-m", "text_codec", "decode"],
    },
}
