#!/usr/bin/env python3
"""
Download a YouTube source as audio and write ANNA provenance metadata.

Requires yt-dlp and ffmpeg on PATH:
    python tools/youtube_sample_ingest.py "https://www.youtube.com/watch?v=..." --out Samples
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def run_json(command: list[str]) -> dict:
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return json.loads(result.stdout)


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Download a YouTube sample with ANNA metadata.")
    parser.add_argument("url", help="YouTube URL to download")
    parser.add_argument("--out", default="Samples", help="Output folder for audio and sidecar metadata")
    parser.add_argument("--format", default="wav", choices=["wav", "mp3", "flac", "m4a"], help="Audio format")
    args = parser.parse_args()

    if shutil.which("yt-dlp") is None:
        print("yt-dlp is required. Install it with: python -m pip install yt-dlp", file=sys.stderr)
        return 2

    if shutil.which("ffmpeg") is None:
        print("ffmpeg is required on PATH for audio extraction.", file=sys.stderr)
        return 2

    output_dir = Path(args.out).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    info = run_json(["yt-dlp", "--dump-single-json", "--no-playlist", args.url])
    title_template = "%(title).180B [%(id)s].%(ext)s"
    output_template = str(output_dir / title_template)

    run(
        [
            "yt-dlp",
            "--no-playlist",
            "--extract-audio",
            "--audio-format",
            args.format,
            "--audio-quality",
            "0",
            "--output",
            output_template,
            args.url,
        ]
    )

    video_id = str(info.get("id", "")).strip()
    expected_suffix = f"[{video_id}].{args.format}"
    candidates = sorted(path for path in output_dir.glob(f"*.{args.format}") if path.name.endswith(expected_suffix))

    if not candidates:
        print("Download completed, but the output file could not be located.", file=sys.stderr)
        return 1

    audio_file = candidates[-1]
    sidecar = audio_file.with_name(f"{audio_file.stem}.anna-origin.json")
    metadata = {
        "sourceType": "youtube",
        "sourceUrl": info.get("webpage_url") or args.url,
        "sourceTitle": info.get("title") or audio_file.stem,
        "sourceAuthor": info.get("uploader") or info.get("channel") or "",
        "sourceId": video_id,
        "downloadedAt": datetime.now(timezone.utc).isoformat(),
        "localFileName": audio_file.name,
    }
    sidecar.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"Audio: {audio_file}")
    print(f"Metadata: {sidecar}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
