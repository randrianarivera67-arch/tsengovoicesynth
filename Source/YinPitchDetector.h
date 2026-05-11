#pragma once
#include <JuceHeader.h>
#include <vector>

/**
 * YIN pitch detection algorithm.
 * de Cheveigné & Kawahara (2002) — "YIN, a fundamental frequency estimator
 * for speech and music", JASA 111(4).
 *
 * Thread-safe: one instance per audio thread — do NOT share across threads.
 */
class YinPitchDetector
{
public:
    struct Result
    {
        float   frequency   = 0.0f;   // Hz, 0 = no pitch detected
        float   confidence  = 0.0f;   // 0..1
        int     midiNote    = -1;      // -1 = no pitch
        float   midiCents   = 0.0f;   // deviation from nearest semitone (-50..+50)
    };

    explicit YinPitchDetector (int bufferSize = 2048,
                               float sampleRate = 44100.0f,
                               float threshold  = 0.15f);

    void prepare (float newSampleRate, int newBufferSize);

    /**
     * Push one audio sample.  Returns a valid Result each time the internal
     * ring-buffer has collected enough data; otherwise returns Result{}.
     */
    Result process (float sample);

    /** Direct block analysis — more efficient in processBlock. */
    Result analyseBlock (const float* samples, int numSamples);

    static int   frequencyToMidi  (float freq);
    static float midiToFrequency  (int note);
    static float midiCentDeviation(float freq, int midiNote);

private:
    void   computeDifference    (const float* buf, int size);
    void   cumulativeMeanNorm   ();
    float  parabolicInterp      (float x) const;

    float              sampleRate_  { 44100.0f };
    float              threshold_   { 0.15f };
    int                bufSize_     { 2048 };
    int                halfBuf_     { 1024 };

    std::vector<float> buffer_;       // ring buffer
    std::vector<float> diff_;         // difference function d(tau)
    std::vector<float> cmndf_;        // CMNDF
    int                writePos_  { 0 };
    int                samplesIn_ { 0 };

    // Smoothing
    float              smoothedFreq_ { 0.0f };
    float              smoothedConf_ { 0.0f };
    static constexpr float kSmooth   { 0.15f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YinPitchDetector)
};
