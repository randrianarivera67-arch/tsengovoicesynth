#include "PitchTracker.h"
#include <cmath>
#include <algorithm>

//==============================================================================
void PitchTracker::prepare (double sampleRate, int maxWindow)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    const size_t n = (size_t) std::max (64, maxWindow / 2) + 2;
    diff_.assign (n, 0.f);
    cmnd_.assign (n, 1.f);

    reset();
}

void PitchTracker::reset()
{
    med_.fill (-1.f);
    medSorted_.fill (-1.f);
    medCount_ = 0;
    medWrite_ = 0;
}

void PitchTracker::setRange (float fMin, float fMax)
{
    fMin_ = std::clamp (fMin, 30.f, 900.f);
    fMax_ = std::clamp (fMax, fMin_ * 1.2f, 3000.f);
}

//==============================================================================
float PitchTracker::parabolicInterp (const std::vector<float>& d, int tau)
{
    if (tau <= 0 || tau >= (int) d.size() - 1) return (float) tau;

    const float s0 = d[(size_t) tau - 1];
    const float s1 = d[(size_t) tau];
    const float s2 = d[(size_t) tau + 1];
    const float denom = s0 - 2.f * s1 + s2;

    if (std::abs (denom) < 1e-8f) return (float) tau;
    return (float) tau + 0.5f * (s0 - s2) / denom;
}

//==============================================================================
PitchTracker::Result PitchTracker::analyse (const float* buf, int N)
{
    Result r;
    if (buf == nullptr || N < 128) return r;

    const int   W  = N / 2;
    const float sr = (float) sampleRate_;

    if ((int) diff_.size() < W + 1)
    {
        // prepare() was never called with a big enough window — grow once here
        // rather than read out of bounds. Not expected on the audio thread.
        diff_.resize ((size_t) W + 2, 0.f);
        cmnd_.resize ((size_t) W + 2, 1.f);
    }

    int minTau = std::max (2,     (int) std::floor (sr / fMax_));
    int maxTau = std::min (W - 2, (int) std::ceil  (sr / fMin_));
    if (maxTau <= minTau + 2) { minTau = 2; maxTau = W - 2; }
    if (maxTau < 4) return r;

    // Difference function + cumulative mean normalisation
    diff_[0] = 0.f;
    cmnd_[0] = 1.f;
    float runSum = 0.f;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        float sum = 0.f;
        for (int j = 0; j < W; ++j)
        {
            const float d = buf[j] - buf[j + tau];
            sum += d * d;
        }
        diff_[(size_t) tau] = sum;
        runSum += sum;
        cmnd_[(size_t) tau] = runSum > 0.f ? sum * (float) tau / runSum : 1.f;
    }

    // First minimum below the threshold, inside the allowed range
    int bestTau = -1;
    for (int tau = minTau; tau <= maxTau; ++tau)
    {
        if (cmnd_[(size_t) tau] < kYinThreshold)
        {
            while (tau + 1 <= maxTau && cmnd_[(size_t) tau + 1] < cmnd_[(size_t) tau])
                ++tau;
            bestTau = tau;
            break;
        }
    }

    if (bestTau < 0)
    {
        bestTau = minTau;
        float best = cmnd_[(size_t) minTau];
        for (int tau = minTau + 1; tau <= maxTau; ++tau)
            if (cmnd_[(size_t) tau] < best) { best = cmnd_[(size_t) tau]; bestTau = tau; }
    }

    r.confidence = std::clamp (1.f - cmnd_[(size_t) bestTau], 0.f, 1.f);
    if (r.confidence < kMinConfidence) return r;   // unvoiced / out of range

    const float tauF = parabolicInterp (cmnd_, bestTau);
    if (tauF < 1.f) return r;

    const float hz = sr / tauF;
    if (hz < fMin_ * 0.85f || hz > fMax_ * 1.15f) return r;

    r.hz = hz;
    return r;
}

//==============================================================================
float PitchTracker::medianFiltered (float hz)
{
    med_[(size_t) medWrite_] = hz;
    medWrite_ = (medWrite_ + 1) % kMedianLength;
    if (medCount_ < kMedianLength) ++medCount_;

    for (int i = 0; i < medCount_; ++i)
        medSorted_[(size_t) i] = med_[(size_t) i];

    std::sort (medSorted_.begin(), medSorted_.begin() + medCount_);
    return medSorted_[(size_t) (medCount_ / 2)];
}
