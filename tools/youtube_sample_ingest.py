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


class CommandFailed(RuntimeError):
    def __init__(self, command: list[str], returncode: int, output: str):
        super().__init__(output)
        self.command = command
        self.returncode = returncode
        self.output = output


def command_output(error: subprocess.CalledProcessError) -> str:
    parts = []
    if error.stdout:
        parts.append(str(error.stdout).strip())
    if error.stderr:
        parts.append(str(error.stderr).strip())

    output = "\n".join(part for part in parts if part)
    if not output:
        output = f"Command failed with exit code {error.returncode}: {' '.join(error.cmd)}"

    return output


def run_json(command: list[str]) -> dict:
    try:
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        return json.loads(result.stdout)
    except subprocess.CalledProcessError as error:
        raise CommandFailed(command, error.returncode, command_output(error)) from error


def run(command: list[str]) -> None:
    try:
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as error:
        raise CommandFailed(command, error.returncode, command_output(error)) from error


def write_origin_sidecar(audio_file: Path, metadata: dict) -> Path:
    sidecar = audio_file.with_name(f"{audio_file.stem}.anna-origin.json")
    sidecar.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
    return sidecar


def main() -> int:
    parser = argparse.ArgumentParser(description="Download a YouTube sample with ANNA metadata.")
    parser.add_argument("url", help="YouTube URL to download")
    parser.add_argument("--out", default="Samples", help="Output folder for audio and sidecar metadata")
    parser.add_argument("--format", default="wav", choices=["wav", "mp3", "flac", "m4a"], help="Audio format")
    parser.add_argument("--json-output", action="store_true", help="Print a machine-readable result object")
    args = parser.parse_args()

    ytdlp = ["yt-dlp"]
    if shutil.which("yt-dlp") is None:
        try:
            import yt_dlp  # noqa: F401
            ytdlp = [sys.executable, "-m", "yt_dlp"]
        except ImportError:
            print("yt-dlp is required. Install it with: python -m pip install yt-dlp", file=sys.stderr)
            return 2

    try:
        ffmpeg_location = shutil.which("ffmpeg")
        if ffmpeg_location is None:
            import imageio_ffmpeg

            ffmpeg = Path(imageio_ffmpeg.get_ffmpeg_exe())
            if ffmpeg.exists():
                ffmpeg_location = str(ffmpeg)
                path = str(ffmpeg.parent)
                import os

                os.environ["PATH"] = path + os.pathsep + os.environ.get("PATH", "")

        if not ffmpeg_location:
            print("ffmpeg is required for audio extraction. Install ffmpeg or imageio-ffmpeg.", file=sys.stderr)
            return 2

        subprocess.run([ffmpeg_location, "-version"], check=True, text=True, capture_output=True)
    except Exception as error:
        print(f"ffmpeg is required for audio extraction, but ANNA could not start it: {error}", file=sys.stderr)
        return 2

    output_dir = Path(args.out).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        info = run_json(ytdlp + ["--dump-single-json", "--no-playlist", args.url])
    except CommandFailed as error:
        print("YouTube metadata fetch failed.", file=sys.stderr)
        print(error.output, file=sys.stderr)
        return error.returncode or 1
    title_template = "%(title).180B [%(id)s].%(ext)s"
    output_template = str(output_dir / title_template)

    download_command = [
        *ytdlp,
        "--no-playlist",
        "--extract-audio",
        "--audio-format",
        args.format,
        "--audio-quality",
        "0",
        "--output",
        output_template,
    ]

    if ffmpeg_location:
        download_command.extend(["--ffmpeg-location", ffmpeg_location])

    download_command.append(args.url)
    try:
        run(download_command)
    except CommandFailed as error:
        print("YouTube audio conversion failed.", file=sys.stderr)
        print(error.output, file=sys.stderr)
        return error.returncode or 1

    video_id = str(info.get("id", "")).strip()
    expected_suffix = f"[{video_id}].{args.format}"
    candidates = sorted(path for path in output_dir.glob(f"*.{args.format}") if path.name.endswith(expected_suffix))

    if not candidates:
        print("Download completed, but the output file could not be located.", file=sys.stderr)
        return 1

    delivery_file = candidates[-1]
    import_file = delivery_file
    metadata = {
        "sourceType": "youtube",
        "sourceUrl": info.get("webpage_url") or args.url,
        "sourceTitle": info.get("title") or delivery_file.stem,
        "sourceAuthor": info.get("uploader") or info.get("channel") or "",
        "sourceId": video_id,
        "downloadedAt": datetime.now(timezone.utc).isoformat(),
        "localFileName": delivery_file.name,
    }
    sidecar = write_origin_sidecar(delivery_file, metadata)

    if args.format != "wav":
        import_file = delivery_file.with_suffix(".anna-import.wav")
        try:
            run(
                [
                    ffmpeg_location,
                    "-y",
                    "-i",
                    str(delivery_file),
                    "-vn",
                    "-acodec",
                    "pcm_s16le",
                    "-ar",
                    "44100",
                    "-ac",
                    "2",
                    str(import_file),
                ]
            )
        except CommandFailed as error:
            print("ANNA waveform import render failed.", file=sys.stderr)
            print(error.output, file=sys.stderr)
            return error.returncode or 1

        import_metadata = dict(metadata)
        import_metadata["localFileName"] = import_file.name
        import_metadata["deliveryFileName"] = delivery_file.name
        import_metadata["importRenderedFrom"] = str(delivery_file)
        sidecar = write_origin_sidecar(import_file, import_metadata)

    if args.json_output:
        print(
            json.dumps(
                {
                    "success": True,
                    "audio": str(import_file),
                    "importAudio": str(import_file),
                    "deliveryAudio": str(delivery_file),
                    "metadata": str(sidecar),
                    "sourceTitle": metadata["sourceTitle"],
                    "sourceUrl": metadata["sourceUrl"],
                },
                ensure_ascii=False,
            )
        )
    else:
        print(f"Audio: {audio_file}")
        print(f"Metadata: {sidecar}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
