#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

//==============================================================================
// TriggerEngine — trainable percussive trigger pads.
//
// This replaces the old "energy + zero-crossing" heuristic. Each pad now learns
// the sound YOU make: you arm a pad, hit the sound a handful of times, and the
// engine stores a timbre fingerprint per hit. Incoming onsets are then matched
// against every stored example (1-nearest-neighbour over z-scored features)
// and only fire the pad they actually resemble. That is the same working model
// as Dubler 2's trigger training — a pad means nothing until you train it.
//
// Threading:
//   * processAudio()  — called from the microphone device thread only.
//   * popEvent()      — called from the plugin's processBlock() thread only.
//   * everything else — message (UI) thread.
// No allocation happens on the audio thread: every buffer is pre-sized.
//==============================================================================

class TriggerEngine
{
public:
    static constexpr int kNumPads     = 6;
    static constexpr int kMaxExamples = 16;   // hits stored per pad
    static constexpr int kNumFeatures = 14;   // timbre fingerprint size
    static constexpr int kFftOrder    = 10;
    static constexpr int kFftSize     = 1 << kFftOrder;   // 1024
    static constexpr int kHop         = 128;
    static constexpr int kRingSize    = 8192;             // power of two

    using FeatureVec = std::array<float, kNumFeatures>;

    struct Event
    {
        int   pad        = -1;
        int   velocity   = 100;
        float confidence = 0.f;
    };

    TriggerEngine();

    void prepare (double sampleRate);
    void reset();

    //==========================================================================
    // Audio threads
    void processAudio (const float* in, int numSamples);   // mic thread
    bool popEvent (Event& e);                              // processBlock thread

    //==========================================================================
    // UI thread — training
    void beginTraining (int pad);
    void endTraining();
    int  getTrainingPad()   const { return trainingPad_.load(); }
    int  getExampleCount (int pad) const;
    bool isPadTrained (int pad) const { return getExampleCount (pad) >= 2; }
    void clearPad (int pad);
    void clearAll();

    // UI read-outs
    int   getLastPad()        const { return lastPad_.load(); }
    float getLastConfidence() const { return lastConfidence_.load(); }
    int   getLastHitCounter() const { return hitCounter_.load(); }
    float getOnsetLevel()     const { return onsetLevel_.load(); }

    //==========================================================================
    // Parameters
    std::atomic<bool>  enabled     { false };
    std::atomic<float> sensitivity { 0.5f };   // 0 = only loud hits, 1 = very touchy
    std::atomic<float> strictness  { 0.5f };   // 0 = fires easily, 1 = must match well
    std::atomic<float> noiseFloor  { 0.01f };  // set by the calibration wizard

    //==========================================================================
    // State persistence (training data survives project reload)
    juce::String saveToString() const;
    void         loadFromString (const juce::String& s);

private:
    void  recomputeStats();                       // UI thread
    void  extractFeatures (const float* w, int n, FeatureVec& out, float& peak);
    int   classify (const FeatureVec& f, float& confidenceOut) const;
    void  pushEvent (const Event& e);
    void  handleOnsetWindow();

    //--------------------------------------------------------------------------
    // Training data
    struct Pad
    {
        std::array<FeatureVec, kMaxExamples> examples {};
        std::atomic<int> count { 0 };
    };
    std::array<Pad, kNumPads> pads_;

    std::array<std::atomic<float>, kNumFeatures> invStd_;

    std::atomic<int>   trainingPad_    { -1 };
    std::atomic<int>   lastPad_        { -1 };
    std::atomic<float> lastConfidence_ { 0.f };
    std::atomic<int>   hitCounter_     { 0 };
    std::atomic<float> onsetLevel_     { 0.f };

    //--------------------------------------------------------------------------
    // Audio-thread state
    double sampleRate_ { 44100.0 };

    std::array<float, kRingSize> ring_ {};
    int   writePos_    { 0 };
    int   hopFill_     { 0 };
    float envSlow_     { 0.f };
    int   blockGap_    { 0 };        // hops still to wait before a new onset
    int   pendingCount_{ -1 };       // samples left to collect after an onset
    int   onsetPos_    { 0 };        // ring position where the onset started

    std::array<float, kFftSize>     window_ {};
    std::array<float, kFftSize * 2> fftBuf_ {};
    std::array<float, kFftSize>     timeBuf_ {};
    juce::dsp::FFT fft_ { kFftOrder };

    //--------------------------------------------------------------------------
    // Lock-free event FIFO (mic thread -> processBlock thread)
    static constexpr int kFifoSize = 32;
    std::array<Event, kFifoSize> fifo_ {};
    std::atomic<int> fifoWrite_ { 0 };
    std::atomic<int> fifoRead_  { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriggerEngine)
};
