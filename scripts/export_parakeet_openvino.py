#!/usr/bin/env python3
"""Construit le package OpenVINO de Parakeet TDT 0.6B v3.

Source : export ONNX officiel istupakov/parakeet-tdt-0.6b-v3-onnx
(pas besoin de torch/NeMo). Produit dans --out :
  encoder.xml/.bin (FP16), decoder_joint.xml/.bin, frontend.bin, tokens.txt

Usage :
  pip install onnx openvino numpy huggingface_hub
  python scripts/export_parakeet_openvino.py --out models/parakeet-v3
"""
import argparse
import subprocess
import sys
from pathlib import Path

REPO = "istupakov/parakeet-tdt-0.6b-v3-onnx"
FILES = [
    "encoder-model.onnx",
    "encoder-model.onnx.data",
    "decoder_joint-model.onnx",
    "nemo128.onnx",
    "vocab.txt",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="models/parakeet-v3")
    ap.add_argument("--onnx-dir", default=None,
                    help="dossier ONNX existant (évite le re-téléchargement)")
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    tmp = Path(str(out) + "_onnx_tmp")
    tmp.mkdir(exist_ok=True)

    # 1) Récupération ONNX
    if args.onnx_dir:
        src = Path(args.onnx_dir)
        missing = [f for f in FILES if not (src / f).exists()]
        if missing:
            print(f"Fichiers manquants dans {src} : {missing}")
            return 1
    else:
        from huggingface_hub import hf_hub_download
        src = tmp
        for f in FILES:
            print("télécharge", f)
            hf_hub_download(REPO, f, local_dir=tmp)

    # 2) Conversion OpenVINO (FP16 par défaut : idéal NPU)
    for name, dst in [("encoder-model.onnx", "encoder.xml"),
                      ("decoder_joint-model.onnx", "decoder_joint.xml")]:
        r = subprocess.run(
            [sys.executable, "-m", "openvino.tools.ovc",
             str(src / name), "--output_model", str(out / dst)],
            capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout, r.stderr)
            return 1
        print(dst, "OK")

    # 3) Frontend : fenêtre hanning(512) + banc mel slaney 257x128 extraits
    #    tels quels du graphe NeMo.
    import numpy as np
    import onnx
    from onnx import numpy_helper

    m = onnx.load(src / "nemo128.onnx")
    bank = win = None
    for init in m.graph.initializer:
        a = numpy_helper.to_array(init).astype(np.float32)
        if a.shape == (257, 128):
            bank = a
        elif init.name == "hann_window":
            win = a
    assert bank is not None and win is not None and win.size == 512
    np.concatenate([win.ravel(), bank.ravel()]).tofile(out / "frontend.bin")
    print("frontend.bin OK")

    # 4) tokens.txt : une pièce par ligne ; blank implicite en dernier.
    vocab = (src / "vocab.txt").read_text(encoding="utf-8").splitlines()
    with (out / "tokens.txt").open("w", encoding="utf-8") as f:
        for line in vocab:
            piece, _, _idx = line.rpartition(" ")
            f.write(piece + "\n")
    print("tokens.txt OK :", len(vocab), "pièces")

    if not args.onnx_dir:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)
    print("Package prêt :", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
