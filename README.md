# Vireo

A synth I built that runs in the browser and also works as a plugin in Logic, Ableton, FL Studio or pretty much any DAW.

Try it here: https://bxzex.github.io/vireo/

It's a two-oscillator poly synth with unison, a sub, noise and FM. There's a filter with its own envelope, two LFOs, glide, an arpeggiator, and drive, chorus, delay and reverb on the end. It comes with 100 presets and six different panel colours. The mod wheel can be set to vibrato, pitch, cutoff or LFO 2.

You can play it with your computer keyboard (A to K, with Z and X to change octave), by clicking the keys, or with a MIDI controller.

## Getting the plugin

Grab it from the [releases page](https://github.com/bxzex/vireo/releases/latest).

On a Mac, unzip it and double-click `Install Vireo.command`. That puts the AU and VST3 in your plugin folders and the standalone app in Applications. I haven't paid for Apple notarisation, so macOS will be suspicious of the download. The script takes care of that. If it still won't open, right-click it and choose Open.

On Windows, copy `Vireo.vst3` into `C:\Program Files\Common Files\VST3` and rescan in your DAW.

## MIDI kit

On the page, hit MIDI kits. There are 60 melodies, basslines, chord progressions and arps in four minor keys. Click one to hear it on the current sound, save the .mid, or just drag it into your DAW. There's also a zip with all of them.

## How it's put together

The browser version is one HTML file running on Web Audio. The plugin is a C++ port of the same engine using JUCE. The parameters and presets are generated from the web page (`plugin/tools/gen.py`), so both versions always have the same sounds.

I tested the plugin with pluginval at its strictest setting and with Apple's auval, and both pass. I also rendered every preset offline to make sure none of them are silent or clipping.

To build it yourself:

```
cd plugin
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Vireo_AU Vireo_VST3 Vireo_Standalone
```

GitHub Actions builds the Mac and Windows versions for every release.

It's AGPL-3.0, same as the free version of JUCE.

© 2026 bxzex
