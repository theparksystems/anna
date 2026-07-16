# ANNA

ANNA is a native C++/JUCE music production workstation focused on sample import, chopping, pitch/time editing, sequencing, channel FX, arrangement playback, and SoundCloud-ready export.

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

- Load or drag audio files into the sample browser.
- Clip and edit slices in the waveform and Slice Editor.
- Use the Slice Editor's `Open FX` button to jump directly to the matching channel FX chain.
- Sequence samples in `Step Seq (1)` or `Piano Roll (2)`.
- Build full songs in `Arrangement (3)` and enable `Song Mode`.
- Export regular WAV files or SoundCloud-ready WAV files with sampled-from metadata.

