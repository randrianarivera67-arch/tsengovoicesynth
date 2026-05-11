#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

/**
 * YIN pitch detection — improved v2.1
 * de Cheveigné & Kawahara (2002)
 *
 * Improvements over v2.0:
 *  - Larger buffer (4096) for better low-freq resolution
 *  - Fixed threshold direction (confidence >= threshold, not inverted)
 *  - Octave error correction heuristic
 *  - 7-frame median filter on detected MIDI notes (rejects brief wrong notes)
 *  - Stable smoothing with separate fast/slow paths
 */
class YinPitchDetector
{
public:
    struct Result
    {
        float frequency  = 0.0f;  // Hz — 0 = no pitch
        float confidence = 0.0f;  // 0..1
        int   midiNote   = -1;    // -1 = no pitch
        float midiCents  = 0.0f;  // deviation cents (-50..+50)
    };

    explicit YinPitchDetector (int bufferSize = 4096,
                               float sampleRate = 44100.0f,
                               float threshold  = 0.12f);

    void   prepare      (float sampleRate, int bufferSize);
    Result analyseBlock (const float* samples, int numSamples);
    Result process      (float sample);

    static int   frequencyToMidi   (float freq);
    static float midiToFrequency   (int   note);
    static float midiCentDeviation (float freq, int midiNote);

private:
    void  computeDifference  (const float* buf, int size);
    void  cumulativeMeanNorm ();
    float parabolicInterp    (float x) const;
    int   octaveCorrect      (float rawFreq, int rawMidi) const;
    int   medianNote         (int newNote);

    float sampleRate_ { 44100.0f };
    float threshold_  { 0.12f };
    int   bufSize_    { 4096 };
    int   halfBuf_    { 2048 };

    std::vector<float> buffer_;
    std::vector<float> diff_;
    std::vector<float> cmndf_;
    int writePos_  { 0 };
    int samplesIn_ { 0 };

    // Smoothing
    float smoothedFreq_ { 0.0f };
    float smoothedConf_ { 0.0f };

    // Median filter — 7-frame window on MIDI note
    static constexpr int kMedianLen { 7 };
    std::array<int, kMedianLen> noteHistory_ {};
    int histIdx_ { 0 };
    bool histFilled_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YinPitchDetector)
};
