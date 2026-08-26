#!/usr/bin/env python3
"""Génère frontend.bin : fenêtre hanning (512) + banc mel slaney (257x128),
extraits tels quels du graphe NeMo exporté par onnx-asr."""
import numpy as np
import sys

bank = np.load("models/onnx/mel_bank.npy")
win = np.load("models/onnx/hann_win.npy")
assert bank.shape == (257, 128), bank.shape
assert win.shape == (512,), win.shape

out = np.concatenate([win.astype(np.float32).ravel(),
                      bank.astype(np.float32).ravel()])
path = sys.argv[1] if len(sys.argv) > 1 else "models/parakeet-v3/frontend.bin"
out.tofile(path)
print(f"{path} écrit ({out.size} float32)")
