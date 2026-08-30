#pragma once
#include <vector>
#include <array>

//==============================================================================
// PitchTracker — YIN fundamental-frequency estimation restricted to a known
// vocal range, plus a running median.
//
// Deliberately free of any framework dependency: it builds and is tested
// with a plain compiler, and the real-time path never allocates — every buffer
// is sized in prepare().
//==============================================================================
class PitchTracker
{
public:
    struct Result
    {
        float hz         = -1.f;   // -1 when nothing periodic was found
        float confidence = 0.f;    // 0..1 (1 - normalised aperiodicity)
    };

    static constexpr int kMedianLength = 7;

    void prepare (double sampleRate, int maxWindow);
    void reset();

    void setRange (float fMin, float fMax);
    float getMinHz() const { return fMin_; }
    float getMaxHz() const { return fMax_; }

    // buf must hold at least N samples; the search uses the first N/2 as the
    // comparison window, so N should be twice the longest period of interest.
    Result analyse (const float* buf, int N);

    // Running median over the last kMedianLength calls. Allocation-free.
    float medianFiltered (float hz);

private:
    static float parabolicInterp (const std::vector<float>& d, int tau);

    double sampleRate_ { 44100.0 };
    float  fMin_ { 70.f };
    float  fMax_ { 1100.f };

    // YIN's aperiodicity threshold. Deliberately separate from the plugin's
    // microphone gate — conflating the two made both hard to tune.
    static constexpr float kYinThreshold = 0.15f;

    // When no minimum passes kYinThreshold we fall back to the best minimum in
    // range — but only if it is still reasonably periodic. Without this, an
    // out-of-range or unvoiced sound gets snapped to the edge of the range and
    // reported as a confident note.
    static constexpr float kMinConfidence = 0.40f;

    std::vector<float> diff_, cmnd_;

    std::array<float, kMedianLength> med_ {};
    std::array<float, kMedianLength> medSorted_ {};
    int medCount_ { 0 };
    int medWrite_ { 0 };
};
