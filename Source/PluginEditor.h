#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CrystalLookAndFeel.h"

// ─────────────────────────────────────────────────────────────────────────────
// Reusable crystal panel
class CrystalPanel : public juce::Component
{
public:
    explicit CrystalPanel (const juce::String& title = {});
    void paint (juce::Graphics&) override;

private:
    juce::String title_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Waveform display
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay();
    void setLevel (float level) noexcept { level_ = level; }
    void setFrequency (float hz)   noexcept { frequency_ = hz; }
    void paint (juce::Graphics&) override;
    void timerCallback() override { repaint(); }

private:
    std::atomic<float> level_     { 0.0f };
    std::atomic<float> frequency_ { 0.0f };
    float phase_   { 0.0f };
    float waveAmp_ { 0.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// Level meter (vertical)
class LevelMeter : public juce::Component
{
public:
    enum class Colour { Green, Orange };
    LevelMeter (Colour c, const juce::String& label);
    void setValue (float v) noexcept { value_ = juce::jlimit (0.0f, 1.0f, v); }
    void paint (juce::Graphics&) override;

private:
    Colour meterColour_;
    juce::String label_;
    float value_ { 0.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// Piano keyboard strip (C3–B5, read-only highlight)
class PianoStrip : public juce::Component
{
public:
    PianoStrip();
    void setActiveNote (int midiNote) noexcept { activeNote_ = midiNote; repaint(); }
    void paint (juce::Graphics&) override;

private:
    static constexpr int kFirst { 48 };   // C3
    static constexpr int kLast  { 83 };   // B5
    int activeNote_ { -1 };

    struct Key { int midi; bool black; float x, w, h; };
    std::vector<Key> keys_;
    void buildKeys (int totalWidth);
};

// ─────────────────────────────────────────────────────────────────────────────
// Main editor
class TsengoVoiceSynthEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit TsengoVoiceSynthEditor (TsengoVoiceSynthProcessor&);
    ~TsengoVoiceSynthEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildLayout();

    TsengoVoiceSynthProcessor& proc_;
    CrystalLookAndFeel         laf_;

    // ── Header ────────────────────────────────────────────────────────────
    juce::Label titleLabel_, versionLabel_;

    // ── Left panel — mic + detection ──────────────────────────────────────
    CrystalPanel    leftPanel_;
    juce::Label     micLabel_;
    juce::ComboBox  deviceCombo_;
    WaveformDisplay waveform_;
    juce::Label     statusLabel_;
    juce::Label     noteLabel_, freqLabel_;
    juce::Label     confLabel_, confValLabel_;

    // ── Center panel — controls ───────────────────────────────────────────
    CrystalPanel    centerPanel_;
    juce::Slider    threshSlider_, volumeSlider_, attackSlider_, releaseSlider_;
    juce::Label     threshLbl_, volumeLbl_, attackLbl_, releaseLbl_;

    using APVTS = juce::AudioProcessorValueTreeState;
    std::unique_ptr<APVTS::SliderAttachment> threshAtt_, volumeAtt_, attackAtt_, releaseAtt_;

    // ── Right panel — MIDI display ────────────────────────────────────────
    CrystalPanel    rightPanel_;
    juce::Label     midiNoteLabel_, midiNumLabel_, midiChLabel_;

    // ── Meters ────────────────────────────────────────────────────────────
    LevelMeter inMeter_  { LevelMeter::Colour::Green,  "IN" };
    LevelMeter outMeter_ { LevelMeter::Colour::Orange, "OUT" };

    // ── Piano ─────────────────────────────────────────────────────────────
    PianoStrip piano_;

    // ── Blink state ───────────────────────────────────────────────────────
    float blinkPhase_ { 0.0f };

    static const juce::String noteNames[12];
    static juce::String midiNoteName (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoVoiceSynthEditor)
};
