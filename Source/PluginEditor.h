#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VoiceSynthEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit VoiceSynthEditor(VoiceSynthProcessor&);
    ~VoiceSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VoiceSynthProcessor& processor;

    // UI components
    juce::Label  titleLabel;
    juce::Label  noteLabel;
    juce::Label  inLevelLabel;
    juce::Label  outLevelLabel;

    // Level bars
    float displayInputLevel  = 0.0f;
    float displayOutputLevel = 0.0f;

    // Note names
    static const char* noteName(int midi)
    {
        static const char* names[] = {
            "Do","Do#","Re","Re#","Mi","Fa",
            "Fa#","Sol","Sol#","La","La#","Si"
        };
        if (midi < 0) return "---";
        return names[midi % 12];
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceSynthEditor)
};
