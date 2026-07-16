# ANNA

ANNA is a native C++/JUCE music production workstation focused on YouTube/local sample import, chopping, pitch/time editing, playlist-style sequencing, channel FX, arrangement playback, and SoundCloud-ready export.

## Launch

From `C:\anna`:

```bat
launch.bat
```

The root launcher starts the built desktop app from `C:\anna\build-anna`.

## Build And Smoke Test

From `C:\anna\anna`:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tests\run_smoke.ps1
```

This configures CMake, builds the Release app, runs unit checks, and verifies the app artifact.

## Core Workflow

- Import samples from YouTube or load local audio into the sample browser.
- Clip and edit slices in the waveform and Slice Editor.
- Select a sample on the left, then add it as a track in the playlist sequencer.
- Use `Piano Roll (2)` for pitched note sequencing when needed.
- Shape each track in `FX Rack (4)` with EQ, color, compression, delay, and reverb.
- Export regular WAV files or SoundCloud-ready WAV files with sampled-from metadata.
