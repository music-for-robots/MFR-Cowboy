# MFR-Cowboy

[![Build](https://github.com/music-for-robots/MFR-Cowboy/actions/workflows/build.yml/badge.svg)](https://github.com/music-for-robots/MFR-Cowboy/actions/workflows/build.yml)

A free, open-source guitar effect by [Music For Robots](https://musicforrobots.com) for **late-'90s Northwest indie guitar**: a big, clean, bright 100 W combo with six 10-inch speakers, a spring reverb, and old tape that puts a hard ceiling on the top end. The grit comes from a **cheap distortion pedal stomped on and off**, and **PULSE** can do the stomping for you, in time with your DAW.

## The rig

- **Distortion:** a cheap op-amp distortion with Dist, Tone and Level, and an ON footswitch. **PULSE** switches it on and off in 1/4, 1/8 or 1/16 notes, locked to the host's tempo and bar position (with a 3 ms crossfade, so it doesn't click). With no host clock, as in the standalone, it free-runs at 120 BPM. The pedal's light shows when it's in.
- **Amp:** a blackface-style clean combo. Its Treble/Middle/Bass tone stack is modelled on the real circuit (in double precision), with a **Bright** switch and lots of power-stage headroom.
- **Spring reverb:** a small tank with the dispersive "boing" of real springs.
- **6x10 cab:** a 10-inch cone body around 550 Hz, a broad upper-mid scoop, and cone breakup at about 2.6 kHz before the cliff.
- **Tape:** soft saturation and a softened top, like an old 1-inch machine.

The cab voicing was fitted to the measured tonal balance of a reference recording's clean intro guitar. At the defaults it lands within about 2 dB on average, from 200 Hz to 8 kHz. `CowboyTest --fit` searches the tone controls against that curve again.

## Controls

| Section | Controls |
| --- | --- |
| **Distortion** | Dist, Tone, Level, ON, PULSE (Off, 1/4, 1/8, 1/16) |
| **Amp** | Volume, Treble, Middle, Bass, BRIGHT, AMP + CAB on/off |
| **Spring** | Reverb amount |
| **Tape** | Tape amount |
| **Out** | Output, and INIT to reset everything |

Knobs: double-click or Alt-click resets a knob, Ctrl/Shift-drag gives fine control, and you can click a value to type one in.

Formats: VST3 and Standalone (Windows 64-bit). Stereo output (from the spring), mono or stereo input.

## Installing

Download the latest release from the [Releases](../../releases) page. Use the installer, or copy `MFR-Cowboy.vst3` into `C:\Program Files\Common Files\VST3\` yourself, then rescan plugins in your DAW.

## Building from source

Requirements: CMake 3.22+ and Visual Studio 2022 with the C++ desktop workload. JUCE 8 is included as a git submodule.

```
git clone --recursive https://github.com/music-for-robots/MFR-Cowboy.git
cd MFR-Cowboy
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built plugin ends up in `build/Cowboy_artefacts/Release/VST3/`. There are two offline tools:

- **`CowboyTest`** checks that PULSE switches in time, the voicing against the reference curve, and held-chord stability with the pedal off and on. It fails on regressions, and writes a short demo WAV.
- **`CowboySnapshot`** renders the UI to a PNG.

To package it locally, run `scripts/package.ps1`. It needs [Inno Setup 6](https://jrsoftware.org/isinfo.php).

## Releasing

1. Bump `VERSION` in `project(...)` in `CMakeLists.txt`.
2. Add a section for the new version to `CHANGELOG.md`.
3. Commit, then tag and push, e.g. `git tag v1.0.1 && git push origin v1.0.1`.

GitHub Actions then builds the plugin, runs `CowboyTest` and [pluginval](https://github.com/Tracktion/pluginval) (strictness 10), and publishes a release with the zip and the installer.

## Source layout

| Path | What's in it |
| --- | --- |
| `Source/CowboyDSP.h` | Distortion and PULSE, amp, spring, cab and tape (plain C++) |
| `Source/PluginProcessor.*` | Parameters, host tempo for PULSE, 4× oversampling, stereo output |
| `Source/PluginEditor.*` | User interface |
| `tools/` | `CowboyTest` and `CowboySnapshot` |
| `installer/`, `scripts/package.ps1` | Installer script and packaging |
| `.github/workflows/build.yml` | CI build, validation and releases |

## License

MFR-Cowboy is licensed under the [GNU AGPLv3](LICENSE) (or any later version). It is built with [JUCE](https://juce.com), which is used under the AGPLv3, and with the Steinberg VST3 SDK. VST is a trademark of Steinberg Media Technologies GmbH. MFR-Cowboy is an independent project.
