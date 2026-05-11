#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CrystalLookAndFeel.h"

// ── Crystal background panel ──────────────────────────────────────────────────
class CrystalPanel : public juce::Component
{
public:
    explicit CrystalPanel (const juce::String& title = {});
    void paint (juce::Graphics&) override;
private:
    juce::String title_;
};

// ── Animated waveform ─────────────────────────────────────────────────────────
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay();
    void setLevel     (float l) noexcept { level_     = l; }
    void setFrequency (float f) noexcept { frequency_ = f; }
    void paint (juce::Graphics&) override;
    void timerCallback() override { repaint(); }
private:
    std::atomic<float> level_     { 0.0f };
    std::atomic<float> frequency_ { 0.0f };
    float phase_   { 0.0f };
    float waveAmp_ { 0.0f };
};

// ── Level meter (vertical) ────────────────────────────────────────────────────
class LevelMeter : public juce::Component
{
public:
    enum class MeterColour { Green, Orange };
    LevelMeter (MeterColour c, const juce::String& label);
    void setValue (float v) noexcept { value_ = juce::jlimit (0.0f, 1.0f, v); }
    void paint (juce::Graphics&) override;
private:
    MeterColour  meterColour_;
    juce::String label_;
    float        value_ { 0.0f };
};

// ── Read-only piano strip C3–B5 ───────────────────────────────────────────────
class PianoStrip : public juce::Component
{
public:
    PianoStrip();
    void setActiveNote (int midi) noexcept { activeNote_ = midi; repaint(); }
    void paint (juce::Graphics&) override;
private:
    static constexpr int kFirst { 48 };
    static constexpr int kLast  { 83 };
    int activeNote_ { -1 };
    struct Key { int midi; bool black; float x, w, h; };
    std::vector<Key> keys_;
    void buildKeys (int totalWidth);
};

// ── Main editor ───────────────────────────────────────────────────────────────
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

    TsengoVoiceSynthProcessor& proc_;
    CrystalLookAndFeel laf_;

    // Header
    juce::Label titleLabel_, versionLabel_;

    // Left panel
    CrystalPanel    leftPanel_;
    juce::Label     micLabel_, statusLabel_;
    juce::Label     noteLabel_, freqLabel_;
    juce::Label     confTitleLabel_, confValLabel_;
    WaveformDisplay waveform_;

    // Sidechain status banner
    juce::Label     scStatusLabel_;

    // Center panel — knobs
    CrystalPanel centerPanel_;
    juce::Slider threshSlider_, volumeSlider_, attackSlider_, releaseSlider_;
    juce::Label  threshLbl_,    volumeLbl_,    attackLbl_,    releaseLbl_;

    using APVTS = juce::AudioProcessorValueTreeState;
    std::unique_ptr<APVTS::SliderAttachment> threshAtt_, volumeAtt_,
                                              attackAtt_, releaseAtt_;
    // Right panel
    CrystalPanel rightPanel_;
    juce::Label  midiNoteLabel_, midiNumLabel_, midiChLabel_;

    // Meters
    LevelMeter inMeter_  { LevelMeter::MeterColour::Green,  "IN"  };
    LevelMeter outMeter_ { LevelMeter::MeterColour::Orange, "OUT" };

    // Piano
    PianoStrip piano_;

    float blinkPhase_ { 0.0f };

    static const juce::String kNoteNames[12];
    static juce::String midiName (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoVoiceSynthEditor)
};
