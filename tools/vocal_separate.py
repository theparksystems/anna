#!/usr/bin/env python3
"""Optional AI vocal separation bridge for ANNA.

Uses Demucs when available and prints one JSON object on the final line.
ANNA falls back to its built-in center splitter if this script cannot run.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("audio", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    audio = args.audio.resolve()
    out_dir = args.out.resolve()

    if not audio.exists():
        raise SystemExit(f"input audio not found: {audio}")

    if shutil.which("demucs") is None:
        raise SystemExit("demucs is not installed")

    out_dir.mkdir(parents=True, exist_ok=True)

    command = [
        "demucs",
        "--two-stems",
        "vocals",
        "--name",
        "htdemucs",
        "--out",
        str(out_dir),
        str(audio),
    ]

    subprocess.run(command, check=True)

    stem_root = out_dir / "htdemucs" / audio.stem
    vocals = stem_root / "vocals.wav"
    instrumental = stem_root / "no_vocals.wav"

    if not vocals.exists() or not instrumental.exists():
        raise SystemExit("demucs finished but expected stems were not found")

    print(json.dumps({
        "method": "demucs",
        "vocals": str(vocals),
        "instrumental": str(instrumental),
    }))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"demucs failed with exit code {exc.returncode}") from exc
