#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout VireoProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < vp::NUM; ++i) {
        const auto& s = vp::specs[i];
        const juce::ParameterID id { s.id, 1 };
        // switches are two-state choices: AudioParameterBool keeps unsnapped values that do not survive a state round trip
        if (s.type == vp::Bool) l.add (std::make_unique<juce::AudioParameterChoice> (id, s.name, juce::StringArray { "Off", "On" }, s.def > .5f ? 1 : 0));
        else if (s.type == vp::Choice) {
            juce::StringArray names; for (int k = 0; k < s.n; ++k) names.add (s.names[k]);
            l.add (std::make_unique<juce::AudioParameterChoice> (id, s.name, names, (int) s.def));
        } else {
            juce::NormalisableRange<float> r;
            if (s.expo) {
                const float mn = s.min, mx = s.max;
                r = juce::NormalisableRange<float> (mn, mx,
                    [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                    [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                    [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
            } else r = juce::NormalisableRange<float> (s.min, s.max, s.step);
            l.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, r, s.def));
        }
    }
    return l;
}

VireoProcessor::VireoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VIREO", layout()) {
    for (int i = 0; i < vp::NUM; ++i) raw[i] = apvts.getRawParameterValue (vp::specs[i].id);
    setCurrentProgram (0);
    startTimerHz (10);
}

bool VireoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VireoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    for (int i = 0; i < vp::NUM; ++i) engine.p[i] = raw[i]->load();
    engine.prepare (sampleRate, juce::jmax (16, samplesPerBlock));
    engine.buildIR();
    prepared = true;
}

// rebuilding the reverb impulse allocates, so it happens here on the message thread
void VireoProcessor::timerCallback() {
    if (prepared && engine.irNeedsBuild()) engine.buildIR();
}

void VireoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    const int prevArp = (int) engine.p[vp::arp_on], prevMode = (int) engine.p[vp::v_mode];
    for (int i = 0; i < vp::NUM; ++i) engine.p[i] = raw[i]->load();
    if ((int) engine.p[vp::arp_on] != prevArp || (int) engine.p[vp::v_mode] != prevMode) engine.allOff();

    engine.hostBpm = 0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) engine.hostBpm = *bpm;

    // the on-screen wheels
    const float b = uiBend.load(), m = uiMod.load();
    if (b != lastBend) { engine.bend = b; lastBend = b; }
    if (m != lastMod) { engine.mw = m; lastMod = m; }

    keyboard.processNextMidiBuffer (midi, 0, n, true);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    int pos = 0;
    auto renderTo = [&] (int end) { if (end > pos) { engine.render (L + pos, R + pos, end - pos); pos = end; } };
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        renderTo (juce::jlimit (0, n, meta.samplePosition));
        if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel()) { engine.bend = (msg.getPitchWheelValue() - 8192) / 8192.f; }
        else if (msg.isController() && msg.getControllerNumber() == 1) engine.mw = msg.getControllerValue() / 127.f;
        else if (msg.isSustainPedalOn()) engine.setSustain (true);
        else if (msg.isSustainPedalOff()) engine.setSustain (false);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allOff();
    }
    renderTo (n);

    // meters and scope for the editor
    float pl = 0, pr = 0; int w = scopeW.load();
    for (int i = 0; i < n; ++i) { pl = std::max (pl, std::abs (L[i])); pr = std::max (pr, std::abs (R[i])); scope[w] = L[i]; w = (w + 1) & (SCOPE - 1); }
    scopeW.store (w);
    peakL.store (std::max (pl, peakL.load() * .9f)); peakR.store (std::max (pr, peakR.load() * .9f));
    voicesNow.store (engine.activeCount());
}

void VireoProcessor::setCurrentProgram (int index) {
    index = juce::jlimit (0, vp::NUM_PRESETS - 1, index);
    program = index;
    const auto& pr = vp::presets[index];
    for (int i = 0; i < vp::NUM; ++i)
        if (auto* p = apvts.getParameter (vp::specs[i].id))
            p->setValueNotifyingHost (p->convertTo0to1 (pr.v[i]));
}

const juce::String VireoProcessor::getProgramName (int index) {
    return index >= 0 && index < vp::NUM_PRESETS ? juce::String (vp::presets[index].name) : juce::String();
}

void VireoProcessor::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty ("program", program.load(), nullptr);
    state.setProperty ("theme", theme, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void VireoProcessor::setStateInformation (const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary (data, sizeInBytes)) {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (apvts.state.getType())) {
            program = (int) tree.getProperty ("program", 0);
            theme = (int) tree.getProperty ("theme", 0);
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* VireoProcessor::createEditor() { return new VireoEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VireoProcessor(); }
