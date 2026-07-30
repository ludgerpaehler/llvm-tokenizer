import os
import subprocess
import sys

HERE = os.path.dirname(__file__)
CORPUS = os.path.join(HERE, "..", "corpus", "core")
PY = sys.executable


def _run(args, **kw):
    return subprocess.run([PY, "-m", "text_codec", *args], cwd=HERE, check=True, **kw)


def test_byte_roundtrip_is_exact(tmp_path):
    src = os.path.join(CORPUS, "float-semantics.ll")
    tokens = tmp_path / "t"
    recon = tmp_path / "r.ll"
    _run(["encode", src, str(tokens)])
    _run(["decode", str(tokens), str(recon)])
    assert recon.read_bytes() == open(src, "rb").read()


def test_tokens_are_space_separated_ints(tmp_path):
    src = os.path.join(CORPUS, "identity-smoke.ll")
    tokens = tmp_path / "t"
    _run(["encode", src, str(tokens)])
    ids = tokens.read_text().split()
    assert ids and all(0 <= int(x) <= 255 for x in ids)


def test_trained_bpe_roundtrip_is_exact(tmp_path):
    model = tmp_path / "bpe.json"
    _run(["train", CORPUS, str(model), "--vocab-size", "500"])
    src = os.path.join(CORPUS, "structs.ll")
    tokens = tmp_path / "t"
    recon = tmp_path / "r.ll"
    _run(["encode", src, str(tokens), "--model", str(model)])
    _run(["decode", str(tokens), str(recon), "--model", str(model)])
    assert recon.read_bytes() == open(src, "rb").read()
