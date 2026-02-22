# EnzoGain

A gain plugin with saturation, LFO modulation, and stereo panning — built with JUCE and a WebView UI.

## Features

- **Gain** — 0% to 150% with smooth ramping
- **Saturation** — 4 curves: Tape, Tube, Digital, Fold (with auto-gain compensation)
- **LFO** — Modulates gain with adjustable rate (0.1–20 Hz) and strength
- **Panning** — Equal-power stereo pan
- **WebView UI** — Modern browser-based interface

## Download

Go to the [Releases](../../releases) page and download:

| Platform | File | Formats |
|----------|------|---------|
| macOS | `EnzoGain-macOS.zip` | VST3, AU, Standalone |
| Windows | `EnzoGain-Windows.zip` | VST3, Standalone |

## Install

### macOS
Copy the plugin to the correct folder:
- **VST3** → `~/Library/Audio/Plug-Ins/VST3/`
- **AU** → `~/Library/Audio/Plug-Ins/Components/`
- **Standalone** → `/Applications/`

### Windows
- **VST3** → `C:\Program Files\Common Files\VST3\`
- **Standalone** → Run `EnzoGain.exe` directly

Then open your DAW and rescan plugins.

## Build from Source

**Requirements:** CMake 3.22+, a C++ compiler (Xcode on macOS, Visual Studio 2019+ on Windows), and Git.

```bash
git clone https://github.com/enzoalexandershah-del/EnzoGain.git
cd EnzoGain
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

JUCE is downloaded automatically if not found locally.

## License

Made by EnzoShah.
