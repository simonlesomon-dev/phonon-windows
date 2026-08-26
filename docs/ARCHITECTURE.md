# Architecture

```
┌───────────────────────────── Windows ─────────────────────────────┐
│                                                                   │
│  Ctrl+Espace ──► RegisterHotKey (Win32)                           │
│       │                                                           │
│       ▼                                                           │
│  App (machine à états Idle → Recording → Processing)              │
│       │                          ▲                                │
│       ▼                          │ texte                          │
│  AudioCapture (WASAPI)   Paster (presse-papiers + Ctrl+V)         │
│  mix format → mono 16 kHz                                         │
│       │                          ▲                                │
│       ▼                          │                                │
│  MelFrontend (log-mel 80) ──► Transcriber (OpenVINO)              │
│                                   encoder / decoder / joint       │
│                                   décodage TDT glouton            │
│                                   device = NPU > GPU > CPU        │
│                                                                   │
│  TrayIcon (état + périphérique)   ModelDownloader (WinHTTP+zip)   │
└───────────────────────────────────────────────────────────────────┘
```

## Composants

| Module | Rôle |
|---|---|
| `app.cpp` | Boucle Win32, raccourci global, machine à états |
| `audio_capture.cpp` | WASAPI mode partagé, événement-driven, conversion mono 16 kHz float |
| `resampler.cpp` | Rééchantillonnage linéaire arbitraire → 16 kHz |
| `mel_frontend.cpp` | Réplique exacte du préprocesseur NeMo (pré-accentuation 0.97, FFT 512 fenêtre paddée, banc mel slaney 257×128 chargé de `frontend.bin`, log + 2⁻²⁴, normalisation par énoncé) |
| `transcriber.cpp` | OpenVINO Runtime ; encoder sur NPU/GPU/CPU, decoder_joint sur CPU ; décodage TDT glouton conforme à la référence onnx-asr (frame [1,1024,1], saut par durée prédite) |
| `paster.cpp` | Presse-papiers CF_UNICODETEXT + SendInput Ctrl+V, restauration optionnelle |
| `model_downloader.cpp` | Téléchargement HTTPS (WinHTTP) du zip du modèle + extraction |
| `settings.cpp` | Clé Run HKCU pour le démarrage auto |

## Choix du périphérique

Au premier lancement, `Transcriber::pickDevice()` parcourt les devices
OpenVINO disponibles et retourne le premier parmi `NPU`, `GPU`, `CPU`.
L'encoder tourne sur ce device ; decoder/joint sont légers et restent
sur CPU/GPU si nécessaire (le NPU est optimisé pour des graphes statiques).

## Décodage TDT

Token-and-Duration Transducer : chaque pas du joint network produit
des logits vocabulaire+blank **et** une distribution de durée.
Si blank est émis avec une probabilité > 0.5, on avance de la durée
prédite (sauts possibles), sinon on émet le token argmax et on met à jour
l'état LSTM du prédicteur.

## Limitations connues v0.1

- Le frontend mel réplique fidèlement l'algorithme d'onnx-asr ;
  validé par comparaison avec la bibliothèque de référence.
- Pas de ponctuation prédictive supplémentaire ; normalisation française
  simple appliquée après décodage.
- Le test d'intégration complet nécessite le package converti
  (`PHONON_MODEL_DIR`).
