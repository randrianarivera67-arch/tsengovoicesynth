#include "YinPitchDetector.h"
#include <cmath>
#include <numeric>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
YinPitchDetector::YinPitchDetector (int bufferSize, float sampleRate, float threshold)
    : sampleRate_ (sampleRate),
      threshold_  (threshold),
      bufSize_    (bufferSize),
      halfBuf_    (bufferSize / 2)
{
    buffer_.assign (static_cast<size_t> (bufSize_), 0.0f);
    diff_  .assign (static_cast<size_t> (halfBuf_), 0.0f);
    cmndf_ .assign (static_cast<size_t> (halfBuf_), 0.0f);
}

void YinPitchDetector::prepare (float newSampleRate, int newBufferSize)
{
    sampleRate_ = newSampleRate;
    bufSize_    = newBufferSize;
    halfBuf_    = newBufferSize / 2;
    writePos_   = 0;
    samplesIn_  = 0;
    smoothedFreq_ = 0.0f;
    smoothedConf_ = 0.0f;

    buffer_.assign (static_cast<size_t> (bufSize_), 0.0f);
    diff_  .assign (static_cast<size_t> (halfBuf_), 0.0f);
    cmndf_ .assign (static_cast<size_t> (halfBuf_), 0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 2 — difference function d(tau)
void YinPitchDetector::computeDifference (const float* buf, int size)
{
    const int half = size / 2;
    for (int tau = 0; tau < half; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < half; ++j)
        {
            const float delta = buf[j] - buf[j + tau];
            sum += delta * delta;
        }
        diff_[static_cast<size_t> (tau)] = sum;
    }
}

// Step 3 — cumulative mean normalised difference function
void YinPitchDetector::cumulativeMeanNorm ()
{
    cmndf_[0] = 1.0f;
    double runningSum = 0.0;
    for (int tau = 1; tau < halfBuf_; ++tau)
    {
        runningSum += static_cast<double> (diff_[static_cast<size_t> (tau)]);
        if (runningSum == 0.0)
            cmndf_[static_cast<size_t> (tau)] = 1.0f;
        else
            cmndf_[static_cast<size_t> (tau)] =
                static_cast<float> (diff_[static_cast<size_t> (tau)]
                                    * static_cast<double> (tau) / runningSum);
    }
}

// Step 5 — parabolic interpolation around minimum at x
float YinPitchDetector::parabolicInterp (float x) const
{
    const int xi = static_cast<int> (x);
    if (xi <= 0 || xi >= halfBuf_ - 1) return x;
    const float s0 = cmndf_[static_cast<size_t> (xi - 1)];
    const float s1 = cmndf_[static_cast<size_t> (xi)];
    const float s2 = cmndf_[static_cast<size_t> (xi + 1)];
    const float denom = 2.0f * s1 - s2 - s0;
    if (std::abs (denom) < 1e-8f) return x;
    return x + (s2 - s0) / (2.0f * denom);
}

// ─────────────────────────────────────────────────────────────────────────────
YinPitchDetector::Result YinPitchDetector::analyseBlock (const float* samples, int numSamples)
{
    // Fill ring buffer
    for (int i = 0; i < numSamples; ++i)
    {
        buffer_[static_cast<size_t> (writePos_)] = samples[i];
        writePos_ = (writePos_ + 1) % bufSize_;
    }
    samplesIn_ += numSamples;
    if (samplesIn_ < bufSize_) return {};

    // Flatten ring buffer into contiguous array for analysis
    std::vector<float> flat (static_cast<size_t> (bufSize_));
    for (int i = 0; i < bufSize_; ++i)
        flat[static_cast<size_t> (i)] = buffer_[static_cast<size_t> ((writePos_ + i) % bufSize_)];

    computeDifference (flat.data(), bufSize_);
    cumulativeMeanNorm ();

    // Step 4 — absolute threshold search
    int    bestTau = -1;
    float  bestVal = threshold_;
    for (int tau = 2; tau < halfBuf_; ++tau)
    {
        if (cmndf_[static_cast<size_t> (tau)] < bestVal)
        {
            // Walk to the local minimum
            while (tau + 1 < halfBuf_ &&
                   cmndf_[static_cast<size_t> (tau + 1)] < cmndf_[static_cast<size_t> (tau)])
                ++tau;
            bestTau = tau;
            bestVal = cmndf_[static_cast<size_t> (tau)];
            break;
        }
    }

    Result result;
    if (bestTau > 0)
    {
        const float refined  = parabolicInterp (static_cast<float> (bestTau));
        const float rawFreq  = sampleRate_ / refined;
        const float rawConf  = 1.0f - bestVal;

        // Frequency guard (human voice / instrument range 50–2000 Hz)
        if (rawFreq >= 50.0f && rawFreq <= 2000.0f)
        {
            smoothedFreq_ = smoothedFreq_ == 0.0f
                                ? rawFreq
                                : smoothedFreq_ + kSmooth * (rawFreq - smoothedFreq_);
            smoothedConf_ += kSmooth * (rawConf - smoothedConf_);

            result.frequency  = smoothedFreq_;
            result.confidence = juce::jlimit (0.0f, 1.0f, smoothedConf_);
            result.midiNote   = frequencyToMidi (smoothedFreq_);
            result.midiCents  = midiCentDeviation (smoothedFreq_, result.midiNote);
        }
    }
    else
    {
        smoothedFreq_ *= 0.9f;
        smoothedConf_ *= 0.85f;
    }

    return result;
}

YinPitchDetector::Result YinPitchDetector::process (float sample)
{
    return analyseBlock (&sample, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
int YinPitchDetector::frequencyToMidi (float freq)
{
    if (freq <= 0.0f) return -1;
    return juce::roundToInt (69.0f + 12.0f * std::log2 (freq / 440.0f));
}

float YinPitchDetector::midiToFrequency (int note)
{
    return 440.0f * std::pow (2.0f, (note - 69) / 12.0f);
}

float YinPitchDetector::midiCentDeviation (float freq, int midiNote)
{
    if (midiNote < 0 || freq <= 0.0f) return 0.0f;
    const float noteFreq = midiToFrequency (midiNote);
    return 1200.0f * std::log2 (freq / noteFreq);
}
