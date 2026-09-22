#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Presets.h"

class VireoProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    VireoProcessor();
    ~VireoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vireo"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return vp::NUM_PRESETS; }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> uiBend { 0 }, uiMod { 0 }, peakL { 0 }, peakR { 0 };
    std::atomic<int> program { 0 };
    int theme = 0;

    // a short history of the output for the scope on the screen
    static constexpr int SCOPE = 4096;
    float scope[SCOPE] {};
    std::atomic<int> scopeW { 0 };
    int activeVoices() const { return voicesNow.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;
    vireo::Engine engine;
    std::atomic<float>* raw[vp::NUM] {};
    std::atomic<int> voicesNow { 0 };
    bool prepared = false;
    float lastBend = 0, lastMod = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VireoProcessor)
};
