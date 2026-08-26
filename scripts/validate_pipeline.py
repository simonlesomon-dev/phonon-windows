#!/usr/bin/env python3
"""Validation bout-en-bout du package OpenVINO : reproduit en Python
exactement le pipeline C++ (frontend mel + encoder + boucle TDT)."""
import sys
import wave

import numpy as np
import openvino as ov


def load_wav(path):
    with wave.open(path, "rb") as w:
        assert w.getframerate() == 16000 and w.getnchannels() == 1
        pcm = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
    return (pcm / 32768.0).astype(np.float32)


def mel_frontend(pcm, win, bank):
    n = len(pcm)
    pre = np.empty(n, dtype=np.float32)
    pre[0] = pcm[0]
    pre[1:] = pcm[1:] - np.float32(0.97) * pcm[:-1]

    n_frames = n // 160 + 1
    valid = n // 160
    padded = np.pad(pre, (256, 256))
    frames = np.lib.stride_tricks.sliding_window_view(padded, 512)[::160][:n_frames]
    spec = np.abs(np.fft.rfft(frames * win, axis=1)) ** 2
    logmel = np.log(spec @ bank + np.float32(2 ** -24))

    mean = logmel[:valid].mean(axis=0, keepdims=True)
    var = ((logmel[:valid] - mean) ** 2).sum(axis=0) / max(1, valid - 1)
    norm = (logmel[:valid] - mean) / (np.sqrt(var) + 1e-5)
    out = np.zeros_like(logmel)
    out[:valid] = norm
    return out  # [T,128]


def main():
    wav_path = sys.argv[1] if len(sys.argv) > 1 else "models/test_fr.wav"
    model_dir = "models/parakeet-v3"

    const = np.fromfile(f"{model_dir}/frontend.bin", dtype=np.float32)
    win, bank = const[:512], const[512:].reshape(257, 128)

    tokens = open(f"{model_dir}/tokens.txt", encoding="utf-8").read().splitlines()
    blank = len(tokens) - 1
    print(f"vocab={len(tokens)} blank_id={blank}")

    core = ov.Core()
    devices = core.get_available_devices()
    print("devices:", devices)
    dev = next((d for p in ("NPU", "GPU", "CPU") for d in devices if d.startswith(p)), "CPU")
    print("device:", dev)

    enc = core.compile_model(f"{model_dir}/encoder.xml", dev if dev != "NPU" else "CPU")
    joint = core.compile_model(f"{model_dir}/decoder_joint.xml", "CPU")

    pcm = load_wav(wav_path)
    feats = mel_frontend(pcm, win, bank)
    T = feats.shape[0]
    print(f"frames: {T} ({T/100:.1f}s)")

    res = enc({"audio_signal": feats.T[None].astype(np.float32),
               "length": np.array([T], dtype=np.int64)})
    enc_out = res["outputs"]  # [1,1024,T'] channels-first
    Te = int(res["encoded_lengths"][0])
    print(f"encoder frames: {Te}")

    tgt = np.full((1, 1), blank, dtype=np.int32)
    tlen = np.ones((1,), dtype=np.int32)
    s1 = np.zeros((2, 1, 640), dtype=np.float32)
    s2 = np.zeros((2, 1, 640), dtype=np.float32)

    emitted = []
    t = 0
    n_emitted = 0
    while t < Te and len(emitted) < 1500:
        frame = enc_out[:, :, t:t + 1]  # [1,1024,1]
        out = joint({
            "encoder_outputs": frame,
            "targets": tgt,
            "target_length": tlen,
            "input_states_1": s1,
            "input_states_2": s2,
        })
        logits = out["outputs"].reshape(-1)[:8193]
        durs = out["outputs"].reshape(-1)[8193:]
        token = int(np.argmax(logits))
        step = int(np.argmax(durs))

        if token != blank:
            s1 = out["output_states_1"]
            s2 = out["output_states_2"]
            tgt[:] = token
            emitted.append(token)
            n_emitted += 1

        if step > 0:
            t += step
            n_emitted = 0
        elif token == blank or n_emitted == 10:
            t += 1
            n_emitted = 0

    text = "".join(tokens[i].replace("▁", " ") for i in emitted)
    print("TRANSCRIPTION:", text.strip())
    return text.strip()


if __name__ == "__main__":
    main()
