#pragma once
#include <JuceHeader.h>
#include "YinPitchDetector.h"

/**
 * TsengoVoiceSynth — PluginProcessor
 *
 * Audio pipeline:
 *   Mic input → ring buffer → YIN pitch detection (background thread) →
 *   MIDI noteOn/noteOff output → DAW piano roll
 *
 * Parameters exposed (APVTS):
 *   "threshold"  — YIN confidence threshold (0.05 – 0.50)
 *   "volume"     — output level (0 – 1)
 *   "attack"     — note debounce attack ms (5 – 200)
 *   "release"    — note hold time ms (50 – 2000)
 *   "mincents"   — pitch deviation tolerance in cents (5 – 50)
 */
class TsengoVoiceSynthProcessor : public juce::AudioProcessor
{
public:
    TsengoVoiceSynthProcessor();
    ~TsengoVoiceSynthProcessor() override;

    // ── AudioProcessor ────────────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tsengo Voice Synth"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return true; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Public state (read by editor on message thread) ───────────────────
    struct DetectionState
    {
        float frequency   { 0.0f };
        float confidence  { 0.0f };
        int   midiNote    { -1 };
        float midiCents   { 0.0f };
        float inputLevel  { 0.0f };
    };
    DetectionState getDetectionState() const noexcept;

    // ── Parameter layout ──────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // ── Parameters ────────────────────────────────────────────────────────
    std::atomic<float>* pThreshold { nullptr };
    std::atomic<float>* pVolume    { nullptr };
    std::atomic<float>* pAttack    { nullptr };
    std::atomic<float>* pRelease   { nullptr };
    std::atomic<float>* pMinCents  { nullptr };

    // ── YIN ───────────────────────────────────────────────────────────────
    YinPitchDetector yin_;
    static constexpr int kYinBuf { 2048 };

    // ── MIDI state ────────────────────────────────────────────────────────
    int   currentNote_       { -1 };
    int   candidateNote_     { -1 };
    int   candidateCount_    { 0 };
    int   attackSamples_     { 0 };
    int   releaseSamples_    { 0 };
    int   releaseCounter_    { 0 };

    // ── Level metering (shared with editor) ───────────────────────────────
    mutable juce::SpinLock stateLock_;
    DetectionState         sharedState_;

    // ── Helpers ───────────────────────────────────────────────────────────
    void emitNoteOn  (int note, juce::MidiBuffer&, int sampleOffset);
    void emitNoteOff (int note, juce::MidiBuffer&, int sampleOffset);
    int  attackThresh()  const noexcept;
    int  releaseThresh() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoVoiceSynthProcessor)
};
