"""Shared helpers for the experiment scripts.

Puts the existing research packages (harness/, text-codec/) on sys.path so we can
reuse `normalize.canonical` and the `text_codec` package instead of duplicating
them, and pins PYTHONPATH so `python -m text_codec` subprocesses resolve too.
"""
import os
import sys

# experiments/workflow/scripts/_common.py -> repo root is three levels up.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
RESEARCH = os.path.join(REPO_ROOT, "research")
HARNESS = os.path.join(RESEARCH, "harness")
TEXT_CODEC = os.path.join(RESEARCH, "text-codec")
CORPUS = os.path.join(RESEARCH, "corpus")

_DIRS = [HARNESS, TEXT_CODEC]
for _p in _DIRS:
    if _p not in sys.path:
        sys.path.insert(0, _p)

_existing = os.environ.get("PYTHONPATH", "")
os.environ["PYTHONPATH"] = os.pathsep.join(_DIRS + ([_existing] if _existing else []))


def partition_modules(partition):
    """Sorted list of .ll module paths in a corpus partition."""
    import glob
    d = os.path.join(CORPUS, partition)
    return sorted(glob.glob(os.path.join(d, "**", "*.ll"), recursive=True))
