#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TsengoEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit TsengoEditor (TsengoProcessor&);
    ~TsengoEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;
    void refreshDevices();

    TsengoProcessor& p_;

    // Device selector
    juce::Label    lblDevice_;
    juce::ComboBox cmbDevice_;
    juce::TextButton btnRefresh_ { "↺" };
    juce::TextButton btnConnect_ { "CONNECT" };

    // Knobs
    juce::Slider sldThreshold_, sldSmoothing_, sldGain_;
    juce::Label  lblThr_, lblSmo_, lblGain_;

    // Display
    juce::Label lblNote_, lblHz_, lblConf_;

    // Meters
    float dispMic_  = 0.f;
    float dispMidi_ = 0.f;

    // Note names
    static const char* noteName (int m)
    {
        static const char* n[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return m >= 0 ? n[m % 12] : "--";
    }

    // Colors
    static juce::Colour cyan()   { return juce::Colour (0xFF00E5FF); }
    static juce::Colour dark()   { return juce::Colour (0xFF080D18); }
    static juce::Colour panel()  { return juce::Colour (0xFF0F1929); }
    static juce::Colour surf()   { return juce::Colour (0xFF172236); }
    static juce::Colour green()  { return juce::Colour (0xFF00C853); }
    static juce::Colour orange() { return juce::Colour (0xFFFF6D00); }
    static juce::Colour purple() { return juce::Colour (0xFF7C4DFF); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoEditor)
};
