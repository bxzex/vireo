// Renders every preset through the plugin's own processor with the same notes the
// web self-test uses, and prints peak and RMS so the two builds can be compared.
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot") { // draw the editor to a PNG, once per theme
        VireoProcessor p; p.setPlayConfigDetails (0, 2, 44100, 512); p.prepareToPlay (44100, 512); p.setCurrentProgram (argc > 3 ? atoi (argv[3]) : 10);
        for (int t = 0; t < 6; ++t) {
            p.theme = t; std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor()); ed->setSize (1360, 960);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.f);
            juce::File f = juce::File (argv[2]).getChildFile ("editor-" + juce::String (t) + ".png"); f.deleteFile();
            juce::PNGImageFormat png; if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        }
        return 0;
    }
    const double sr = 44100; const int block = 512, len = (int) (sr * 2.4);
    VireoProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
    int silent = 0, hot = 0, nan = 0; double mn = 0, mx = -200;
    for (int i = 0; i < vp::NUM_PRESETS; ++i) {
        p.setCurrentProgram (i); p.prepareToPlay (sr, block);
        const juce::String cat (vp::presets[i].cat);
        std::vector<int> notes = cat == "BASS" ? std::vector<int> { 36 } : cat == "LEAD" ? std::vector<int> { 72 } : std::vector<int> { 48, 55, 60, 63 };
        juce::AudioBuffer<float> buf (2, block); double sum = 0; float pk = 0; bool bad = false; long N = 0;
        juce::AudioBuffer<float> all (2, len);
        for (int pos = 0; pos < len; pos += block) {
            const int n = std::min (block, len - pos); buf.setSize (2, n, false, false, true);
            juce::MidiBuffer m;
            for (int k : notes) {
                const int on = (int) (.05 * sr), off = (int) (1.3 * sr);
                if (on >= pos && on < pos + n) m.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) 114), on - pos);
                if (off >= pos && off < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, k), off - pos);
            }
            p.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) { const float* d = buf.getReadPointer (c); for (int s = 0; s < n; ++s) { const float v = d[s]; if (v != v) bad = true; pk = std::max (pk, std::abs (v)); sum += v * v; ++N; all.setSample (c, pos + s, v); } }
        }
        const double rms = std::sqrt (sum / N), db = 20 * std::log10 (rms + 1e-12);
        if (rms < 1e-3) ++silent; if (pk > 1) ++hot; if (bad) ++nan;
        mn = std::min (mn, db); mx = std::max (mx, db);
        std::printf ("%-18s peak %.3f  rms %6.1f dB\n", vp::presets[i].name, pk, db);
        if (argc > 1) { // write wavs for listening
            juce::File f = juce::File (argv[1]).getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + " " + vp::presets[i].name + ".wav");
            f.deleteFile(); juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> ww (w); ww->writeFromAudioSampleBuffer (all, 0, len); }
        }
    }
    // tuning: a sine at A4 through an open filter, frequency from zero crossings
    {
        VireoProcessor t; t.setPlayConfigDetails (0, 2, 48000, 512);
        t.setCurrentProgram (0);
        auto set = [&] (const char* id, float v) { auto* q = t.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set ("o1_wave", 3); set ("o2_on", 0); set ("sub_lvl", 0); set ("f_cut", 18000); set ("f_env", 0); set ("rv_mix", 0); set ("fx_drive", 0); set ("v_mode", 0); set ("vol", .5f); set ("trim", 1);
        t.prepareToPlay (48000, 512);
        std::vector<float> out; juce::AudioBuffer<float> b (2, 512); bool first = true;
        for (int pos = 0; pos < 48000; pos += 512) { juce::MidiBuffer m; if (first) { m.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 127), 0); first = false; } t.processBlock (b, m); for (int s = 0; s < 512; ++s) out.push_back (b.getSample (0, s)); }
        std::vector<double> cr; for (size_t i = 14400; i < 45600; ++i) if (out[i - 1] < 0 && out[i] >= 0) cr.push_back ((double) i - 1 + (-out[i - 1]) / (out[i] - out[i - 1]));
        std::printf ("\nA4 renders at %.3f Hz\n", 48000.0 * (cr.size() - 1) / (cr.back() - cr.front()));
    }
    std::printf ("%d presets. silent %d, over 0 dBFS %d, NaN %d. RMS %.1f to %.1f dBFS\n", vp::NUM_PRESETS, silent, hot, nan, mn, mx);
    return (silent || hot || nan) ? 1 : 0;
}
