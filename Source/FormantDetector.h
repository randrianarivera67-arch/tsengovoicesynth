#pragma once
#include <JuceHeader.h>
#include <vector>

// Estimates the first two vocal formants (F1, F2) from a mono audio buffer
// using LPC (Linear Predictive Coding), then maps the (F1, F2) point to three
// continuous "vowel" blend weights — aaa (open, e.g. /a/), eee (front, e.g.
// /i/ /e/), ooo (back/round, e.g. /u/ /o/) — the same three macro controls
// shown on the Dubler 2 "Play" pad. The weights sum to ~1 and can be sent
// out as continuous MIDI CCs to automate FX/parameters, exactly like Dubler.
class FormantDetector
{
public:
    struct Result
    {
        float f1 = 0.f;
        float f2 = 0.f;
        float confidence = 0.f;   // 0..1 — how voiced/reliable this frame is
        float aaa = 0.f;          // open vowel weight
        float eee = 0.f;          // front vowel weight
        float ooo = 0.f;          // back vowel weight
    };

    explicit FormantDetector (double sampleRate = 44100.0);

    void setSampleRate (double sampleRate);

    // buf: N mono samples (the same analysis window used for pitch tracking
    // works fine here). Returns confidence == 0 on silence / unvoiced frames.
    Result analyse (const float* buf, int N);

private:
    double sampleRate_ { 44100.0 };

    // Formants live below ~3.5 kHz, so we decimate down to ~10 kHz before
    // LPC — this keeps the filter order (and CPU cost) low and well-behaved.
    static constexpr int kDecimatedSR = 10000;
    static constexpr int kLpcOrder    = 12;

    std::vector<float> decim_;
    std::vector<float> autocorr_;
    std::vector<float> lpc_;

    void  decimate       (const float* in, int N, std::vector<float>& out, int& outSR);
    void  hammingWindow  (std::vector<float>& v);
    void  autocorrelate  (const std::vector<float>& v, std::vector<float>& r, int maxLag);
    bool  levinsonDurbin (const std::vector<float>& r, int order, std::vector<float>& a);
    void  findFormants   (const std::vector<float>& a, int order, int sr, float& f1, float& f2);

    // Reference formant targets (Hz), average adult voice — used as the
    // triangle vertices for the barycentric aaa/eee/ooo blend.
    static constexpr float kAaaF1 = 750.f, kAaaF2 = 1200.f;
    static constexpr float kEeeF1 = 300.f, kEeeF2 = 2300.f;
    static constexpr float kOooF1 = 320.f, kOooF2 = 800.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormantDetector)
};
