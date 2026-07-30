"""Make the harness and text-codec packages importable when running pytest from
research/ without manually setting PYTHONPATH.

sys.path covers in-process imports (normalize/tracks/roundtrip); the PYTHONPATH
env var is needed because the `text` track runs `python -m text_codec` in a
subprocess that does not inherit sys.path.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
_DIRS = [os.path.join(HERE, "harness"), os.path.join(HERE, "text-codec")]

for _p in _DIRS:
    if _p not in sys.path:
        sys.path.insert(0, _p)

_existing = os.environ.get("PYTHONPATH", "")
os.environ["PYTHONPATH"] = os.pathsep.join(_DIRS + ([_existing] if _existing else []))
