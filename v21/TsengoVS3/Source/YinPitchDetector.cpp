#include "YinPitchDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>

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
    noteHistory_.fill (-1);
}

void YinPitchDetector::prepare (float sr, int bufSize)
{
    sampleRate_ = sr;
    bufSize_    = bufSize;
    halfBuf_    = bufSize / 2;
    writePos_   = 0;
    samplesIn_  = 0;
    smoothedFreq_ = 0.0f;
    smoothedConf_ = 0.0f;
    histIdx_    = 0;
    histFilled_ = false;
    noteHistory_.fill (-1);

    buffer_.assign (static_cast<size_t> (bufSize_), 0.0f);
    diff_  .assign (static_cast<size_t> (halfBuf_), 0.0f);
    cmndf_ .assign (static_cast<size_t> (halfBuf_), 0.0f);
}

// ── Step 2: difference function ──────────────────────────────────────────────
void YinPitchDetector::computeDifference (const float* buf, int size)
{
    const int half = size / 2;
    for (int tau = 0; tau < half; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < half; ++j)
        {
            const float d = buf[j] - buf[j + tau];
            sum += d * d;
        }
        diff_[static_cast<size_t> (tau)] = sum;
    }
}

// ── Step 3: CMNDF ─────────────────────────────────────────────────────────────
void YinPitchDetector::cumulativeMeanNorm ()
{
    cmndf_[0] = 1.0f;
    double runSum = 0.0;
    for (int tau = 1; tau < halfBuf_; ++tau)
    {
        runSum += diff_[static_cast<size_t> (tau)];
        cmndf_[static_cast<size_t> (tau)] = (runSum == 0.0)
            ? 1.0f
            : static_cast<float> (diff_[static_cast<size_t> (tau)] * tau / runSum);
    }
}

// ── Step 5: parabolic interpolation ──────────────────────────────────────────
float YinPitchDetector::parabolicInterp (float x) const
{
    const int xi = static_cast<int> (x);
    if (xi <= 0 || xi >= halfBuf_ - 1) return x;
    const float s0 = cmndf_[static_cast<size_t> (xi - 1)];
    const float s1 = cmndf_[static_cast<size_t> (xi)];
    const float s2 = cmndf_[static_cast<size_t> (xi + 1)];
    const float d  = 2.0f * s1 - s2 - s0;
    if (std::abs (d) < 1e-8f) return x;
    return x + (s2 - s0) / (2.0f * d);
}

// ── Octave correction ─────────────────────────────────────────────────────────
// YIN sometimes returns half the true frequency (octave below).
// Heuristic: if a sub-harmonic tau gives nearly equal CMNDF value,
// prefer the higher-frequency candidate (shorter tau = higher pitch).
int YinPitchDetector::octaveCorrect (float rawFreq, int rawMidi) const
{
    // Look for a candidate at 2x the frequency (half the period)
    const float doubleFreq = rawFreq * 2.0f;
    if (doubleFreq > 1600.0f) return rawMidi; // already high enough

    const int   halfTau = static_cast<int> (sampleRate_ / doubleFreq);
    if (halfTau < 2 || halfTau >= halfBuf_ - 1) return rawMidi;

    const float halfVal = cmndf_[static_cast<size_t> (halfTau)];
    const float currTau = static_cast<int> (sampleRate_ / rawFreq);
    const float currVal = currTau > 0 && currTau < halfBuf_
                            ? cmndf_[static_cast<size_t> (currTau)]
                            : 1.0f;

    // If the double-freq candidate has a similar dip, pick it
    if (halfVal < currVal * 1.15f && halfVal < 0.35f)
        return frequencyToMidi (doubleFreq);

    return rawMidi;
}

// ── 7-frame median filter ─────────────────────────────────────────────────────
int YinPitchDetector::medianNote (int newNote)
{
    noteHistory_[static_cast<size_t> (histIdx_)] = newNote;
    histIdx_ = (histIdx_ + 1) % kMedianLen;
    if (histIdx_ == 0) histFilled_ = true;

    const int validLen = histFilled_ ? kMedianLen : histIdx_;
    if (validLen == 0) return newNote;

    // Copy valid portion and sort
    std::array<int, kMedianLen> tmp {};
    int cnt = 0;
    for (int i = 0; i < kMedianLen && cnt < validLen; ++i)
        tmp[static_cast<size_t> (cnt++)] = noteHistory_[static_cast<size_t> (i)];

    std::sort (tmp.begin(), tmp.begin() + cnt);
    return tmp[static_cast<size_t> (cnt / 2)];
}

// ── Main analysis ─────────────────────────────────────────────────────────────
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

    // Flatten ring buffer
    std::vector<float> flat (static_cast<size_t> (bufSize_));
    for (int i = 0; i < bufSize_; ++i)
        flat[static_cast<size_t> (i)] = buffer_[static_cast<size_t> ((writePos_ + i) % bufSize_)];

    computeDifference (flat.data(), bufSize_);
    cumulativeMeanNorm ();

    // Find first tau below threshold
    int   bestTau = -1;
    float bestVal = threshold_;  // FIX: threshold correct direction

    for (int tau = 2; tau < halfBuf_; ++tau)
    {
        if (cmndf_[static_cast<size_t> (tau)] < bestVal)
        {
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
        const float rawConf  = 1.0f - bestVal;  // FIX: confidence correct direction

        if (rawFreq >= 60.0f && rawFreq <= 1100.0f)  // vocal range C2–C6
        {
            // Smooth frequency with slower coefficient for stability
            const float alpha = 0.25f;
            smoothedFreq_ = smoothedFreq_ < 1.0f
                              ? rawFreq
                              : smoothedFreq_ + alpha * (rawFreq - smoothedFreq_);

            smoothedConf_ += 0.2f * (rawConf - smoothedConf_);

            const float conf = juce::jlimit (0.0f, 1.0f, smoothedConf_);

            // FIX: only emit result if confidence above threshold
            if (conf >= (1.0f - threshold_))
            {
                int rawMidi = frequencyToMidi (smoothedFreq_);
                rawMidi     = juce::jlimit (24, 96, rawMidi);

                // Octave correction
                const int correctedMidi = octaveCorrect (smoothedFreq_, rawMidi);

                // Median filter — rejects brief wrong detections
                const int filteredMidi  = medianNote (correctedMidi);

                result.frequency  = smoothedFreq_;
                result.confidence = conf;
                result.midiNote   = filteredMidi;
                result.midiCents  = midiCentDeviation (smoothedFreq_, filteredMidi);
            }
        }
    }
    else
    {
        // Decay smoothing when no pitch
        smoothedFreq_ *= 0.92f;
        smoothedConf_ *= 0.88f;
        medianNote (-1); // push silence into median buffer
    }

    return result;
}

YinPitchDetector::Result YinPitchDetector::process (float sample)
{
    return analyseBlock (&sample, 1);
}

// ── Utilities ─────────────────────────────────────────────────────────────────
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
    return 1200.0f * std::log2 (freq / midiToFrequency (midiNote));
}
