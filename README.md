# Phonon Windows

Dictée vocale **100 % locale** pour Windows 11 : appuyez sur `Ctrl + Espace`,
parlez en français, rappuyez — le texte est collé dans l'application active.

- Modèle : [Parakeet TDT 0.6B v3](https://huggingface.co/nvidia/parakeet-tdt-0.6b-v3) (NVIDIA, open weights)
- Inférence : **OpenVINO**, priorité **NPU → GPU → CPU**
- Capture audio : WASAPI (microphone par défaut)
- Aucune connexion réseau requise après le téléchargement initial du modèle
- Aucun historique audio ou texte conservé

## Installation

Téléchargez `PhononWindows-Setup.exe` depuis la page
[Releases](../../releases/latest) et lancez-le.
Au premier démarrage, l'application télécharge automatiquement le modèle
(~1,2 Go) vers `%LOCALAPPDATA%\PhononWindows\models`.

## Utilisation

| Action | Effet |
|---|---|
| `Ctrl + Espace` (1er appui) | Démarre l'enregistrement |
| `Ctrl + Espace` (2e appui) | Arrête, transcrit et colle le texte |

L'icône de la barre système indique l'état et le périphérique utilisé :
**NPU** (recommandé, consommation minimale), **GPU** ou **CPU**.

Menu contextuel de l'icône : démarrage automatique avec Windows, quitter.

## Compilation

Prérequis : Visual Studio 2022, CMake ≥ 3.20,
[OpenVINO](https://docs.openvino.ai/) ≥ 2024.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release
```

### Conversion du modèle

Le package est construit depuis l'export ONNX officiel
[`istupakov/parakeet-tdt-0.6b-v3-onnx`](https://huggingface.co/istupakov/parakeet-tdt-0.6b-v3-onnx)
(sans torch ni NeMo) :

```bash
pip install onnx openvino numpy huggingface_hub
python scripts/export_parakeet_openvino.py --out models/parakeet-v3
```

Contenu produit (~1,2 Go en FP16) : `encoder.xml/.bin`,
`decoder_joint.xml/.bin`, `frontend.bin`, `tokens.txt`.
Copiez le dossier dans `%LOCALAPPDATA%\PhononWindows\models\parakeet-v3`
ou publiez-le comme asset de release (`parakeet-v3-openvino.zip`).

Validation bout-en-bout du package :

```bash
pip install openvino numpy
python scripts/validate_pipeline.py   # transcrit models/test_fr.wav
```

### Installeur

```powershell
choco install innosetup -y
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\phonon.iss
# => dist/PhononWindows-Setup.exe
```

## Architecture

Voir [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Confidentialité

- L'audio est traité exclusivement en local.
- Aucun enregistrement n'est écrit sur disque ni envoyé sur le réseau
  (le seul téléchargement est celui du modèle, à l'installation).
- La désinstallation supprime le modèle et tous les fichiers associés.

## Licence

MIT.
