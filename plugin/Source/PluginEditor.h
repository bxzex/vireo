#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

struct Theme { const char* name; juce::uint32 panel, led, ink, label, title, sub, rule, well, logo; int wood[6]; };

class VireoEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VireoEditor (VireoProcessor&);
    ~VireoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // shared with the child components
    VireoProcessor& proc;
    const Theme& theme() const;
    juce::Font silk (float h, bool bold = false) const;
    juce::Font lcd (float h) const;
    void touched (int id);
    int statusId = -1; juce::uint32 statusT = 0;
    juce::Image woodL, woodR, brushed;
    void setTheme (int t);
    void refreshPresetName();

private:
    void timerCallback() override;
    juce::Typeface::Ptr tfSemi, tfBold, tfLcd;
    std::unique_ptr<juce::Component> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VireoEditor)
};
