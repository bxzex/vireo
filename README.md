# Vireo

A polyphonic synthesizer by bxzex. You can play it in the browser, or download it as an AU/VST3 plugin for Logic Pro, Ableton Live, FL Studio and other DAWs.

https://bxzex.github.io/vireo/

It has two oscillators with up to seven unison voices each, plus a sub, noise, FM and a pitch envelope. After that come a multimode filter with its own envelope, two LFOs, mono and legato glide, an arpeggiator, and drive, chorus, ping-pong delay and reverb. There are 50 presets, six panel finishes, and a mod wheel you can route to vibrato, pitch, cutoff or LFO 2.

You can play it with your computer keyboard (A to K for notes, Z and X for octaves), with the on-screen keys, or from a MIDI controller.

## Plugin

Get it from [Releases](https://github.com/bxzex/vireo/releases/latest):

- **Vireo-macOS.zip** has the AU, VST3 and a standalone app, universal for Apple Silicon and Intel. Double-click `Install Vireo.command` inside. The plugins aren't notarised by Apple, so the script also removes the quarantine flag macOS puts on downloads.
- **Vireo-Windows.zip** has the VST3 and a standalone .exe. Copy `Vireo.vst3` into `C:\Program Files\Common Files\VST3`.

The plugin is a C++ port of the browser engine, built on JUCE. It uses the same parameters and the same 50 presets. Both are generated from the web page by `plugin/tools/gen.py`, so the two builds can't drift apart.

Both formats pass pluginval at strictness 10, and the AU passes Apple's `auval`. An offline render of all 50 presets comes out with none silent and none clipping, and A4 lands at 440.000 Hz.

## MIDI kit

The page includes 30 clips (melodies, basslines, chord progressions and arps) in C, D, F and A minor. Click one to hear it, download it as a .mid, or drag it straight into your DAW. The full kit comes as a zip of 120 files.

## Building the plugin

```
cd plugin
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Vireo_AU Vireo_VST3 Vireo_Standalone
```

CMake fetches JUCE on its own. Every published release gets built on GitHub Actions for both macOS and Windows.

Licensed under the AGPL-3.0, the same as the open source edition of JUCE.

© 2026 bxzex
