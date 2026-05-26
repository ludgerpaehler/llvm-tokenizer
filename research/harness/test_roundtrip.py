import os
from roundtrip import roundtrip

LLVM_BIN = os.environ.get("LLVM_BIN", "/usr/lib/llvm-22/bin")
CORPUS = os.path.join(os.path.dirname(__file__), "..", "corpus", "core")


def test_identity_track_round_trips_smoke():
    module = os.path.join(CORPUS, "identity-smoke.ll")
    ok, diff = roundtrip("identity", module, LLVM_BIN)
    assert ok, diff


def test_identity_track_round_trips_all_core():
    for name in sorted(os.listdir(CORPUS)):
        if not name.endswith(".ll"):
            continue
        module = os.path.join(CORPUS, name)
        ok, diff = roundtrip("identity", module, LLVM_BIN)
        assert ok, f"{name} failed identity round-trip:\n{diff}"


def test_text_track_round_trips_all_core():
    for name in sorted(os.listdir(CORPUS)):
        if not name.endswith(".ll"):
            continue
        module = os.path.join(CORPUS, name)
        ok, diff = roundtrip("text", module, LLVM_BIN)
        assert ok, f"{name} failed text round-trip:\n{diff}"


def test_structured_track_round_trips_identity_smoke():
    module = os.path.join(CORPUS, "identity-smoke.ll")
    ok, diff = roundtrip("structured", module, LLVM_BIN)
    assert ok, f"identity-smoke failed structured round-trip:\n{diff}"
