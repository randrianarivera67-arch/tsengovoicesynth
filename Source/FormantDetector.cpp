#include "FormantDetector.h"
#include <cmath>
#include <algorithm>

FormantDetector::FormantDetector (double sampleRate) : sampleRate_ (sampleRate)
{
    decim_.reserve (4096);
    autocorr_.resize ((size_t) kLpcOrder + 1);
    lpc_.resize ((size_t) kLpcOrder + 1);
}

void FormantDetector::setSampleRate (double sampleRate) { sampleRate_ = sampleRate; }

//==============================================================================
void FormantDetector::decimate (const float* in, int N, std::vector<float>& out, int& outSR)
{
    int factor = juce::jmax (1, (int) std::round (sampleRate_ / (double) kDecimatedSR));
    outSR = (int) std::round (sampleRate_ / (double) factor);

    out.clear();
    // Box-car average as a crude anti-alias filter before decimating.
    for (int i = 0; i + factor <= N; i += factor)
    {
        float acc = 0.f;
        for (int j = 0; j < factor; ++j)
            acc += in[i + j];
        out.push_back (acc / (float) factor);
    }
}

void FormantDetector::hammingWindow (std::vector<float>& v)
{
    const int n = (int) v.size();
    if (n < 2) return;
    for (int i = 0; i < n; ++i)
    {
        float w = 0.54f - 0.46f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1));
        v[(size_t) i] *= w;
    }
}

void FormantDetector::autocorrelate (const std::vector<float>& v, std::vector<float>& r, int maxLag)
{
    const int n = (int) v.size();
    r.assign ((size_t) maxLag + 1, 0.f);
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i + lag < n; ++i)
            sum += (double) v[(size_t) i] * (double) v[(size_t) (i + lag)];
        r[(size_t) lag] = (float) sum;
    }
}

bool FormantDetector::levinsonDurbin (const std::vector<float>& r, int order, std::vector<float>& a)
{
    a.assign ((size_t) order + 1, 0.f);
    if (r[0] <= 0.f) return false;

    std::vector<float> tmp ((size_t) order + 1, 0.f);
    float err = r[0];
    a[0] = 1.f;

    for (int i = 1; i <= order; ++i)
    {
        float acc = r[(size_t) i];
        for (int j = 1; j < i; ++j)
            acc += a[(size_t) j] * r[(size_t) (i - j)];

        float k = (err > 1e-9f) ? (-acc / err) : 0.f;

        tmp = a;
        for (int j = 1; j < i; ++j)
            a[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
        a[(size_t) i] = k;

        err *= (1.f - k * k);
        if (err <= 0.f) err = 1e-9f;
    }
    return true;
}

void FormantDetector::findFormants (const std::vector<float>& a, int order, int sr, float& f1, float& f2)
{
    // Evaluate the LPC spectral envelope 1/|A(f)| across the formant range
    // and pick the two lowest, well-separated peaks as F1 and F2.
    const int   bins = 256;
    const float fMax = juce::jmin (3500.f, (float) sr * 0.5f);

    std::vector<float> mag ((size_t) bins, 0.f);
    for (int b = 0; b < bins; ++b)
    {
        float freq = (float) b / (float) bins * fMax;
        float w    = juce::MathConstants<float>::twoPi * freq / (float) sr;

        float re = 1.f, im = 0.f;
        for (int k = 1; k <= order; ++k)
        {
            re += a[(size_t) k] * std::cos (-w * (float) k);
            im += a[(size_t) k] * std::sin (-w * (float) k);
        }
        float denom = std::sqrt (re * re + im * im);
        mag[(size_t) b] = denom > 1e-6f ? 1.f / denom : 0.f;
    }

    std::vector<float> peakFreqs;
    for (int b = 2; b < bins - 2; ++b)
    {
        if (mag[(size_t) b] > mag[(size_t) (b - 1)] && mag[(size_t) b] > mag[(size_t) (b + 1)]
            && mag[(size_t) b] > mag[(size_t) (b - 2)] && mag[(size_t) b] > mag[(size_t) (b + 2)])
        {
            float freq = (float) b / (float) bins * fMax;
            if (freq > 150.f) peakFreqs.push_back (freq);
        }
    }
    std::sort (peakFreqs.begin(), peakFreqs.end());

    f1 = 0.f; f2 = 0.f;
    if (! peakFreqs.empty())
    {
        f1 = peakFreqs[0];
        for (size_t i = 1; i < peakFreqs.size(); ++i)
        {
            if (peakFreqs[i] - f1 > 200.f) { f2 = peakFreqs[i]; break; }
        }
        if (f2 == 0.f) f2 = f1 + 800.f; // only one clear peak — assume typical F1-F2 spacing
    }
}

//==============================================================================
FormantDetector::Result FormantDetector::analyse (const float* buf, int N)
{
    Result out;

    int decimSR = 0;
    decimate (buf, N, decim_, decimSR);
    if ((int) decim_.size() < kLpcOrder * 3 || decimSR <= 0)
        return out;

    // RMS gate — skip LPC work on near-silence (cheap, avoids noise-floor garbage).
    float rms = 0.f;
    for (float s : decim_) rms += s * s;
    rms = std::sqrt (rms / (float) decim_.size());
    if (rms < 1e-4f)
        return out;

    // Pre-emphasis boosts higher formants relative to the voice's natural rolloff.
    for (size_t i = decim_.size() - 1; i > 0; --i)
        decim_[i] -= 0.97f * decim_[i - 1];

    hammingWindow (decim_);
    autocorrelate (decim_, autocorr_, kLpcOrder);

    if (! levinsonDurbin (autocorr_, kLpcOrder, lpc_))
        return out;

    float f1 = 0.f, f2 = 0.f;
    findFormants (lpc_, kLpcOrder, decimSR, f1, f2);
    if (f1 <= 0.f || f2 <= 0.f)
        return out;

    out.f1 = f1;
    out.f2 = f2;
    out.confidence = juce::jlimit (0.f, 1.f, rms * 20.f);

    // Barycentric weights of (f1,f2) relative to the aaa/eee/ooo reference
    // triangle, worked out in log-frequency space to match pitch perception.
    auto logf = [] (float f) { return std::log (juce::jmax (f, 1.f)); };

    const float x  = logf (f1),     y  = logf (f2);
    const float xA = logf (kAaaF1), yA = logf (kAaaF2);
    const float xE = logf (kEeeF1), yE = logf (kEeeF2);
    const float xO = logf (kOooF1), yO = logf (kOooF2);

    const float det = (yE - yO) * (xA - xO) + (xO - xE) * (yA - yO);
    float wA = 0.f, wE = 0.f, wO = 0.f;
    if (std::abs (det) > 1e-6f)
    {
        wA = ((yE - yO) * (x - xO) + (xO - xE) * (y - yO)) / det;
        wE = ((yO - yA) * (x - xO) + (xA - xO) * (y - yO)) / det;
        wO = 1.f - wA - wE;
    }

    // Clip to [0,1] and renormalise so points outside the triangle still get
    // a sensible nearest-edge blend instead of negative weights.
    wA = juce::jlimit (0.f, 1.f, wA);
    wE = juce::jlimit (0.f, 1.f, wE);
    wO = juce::jlimit (0.f, 1.f, wO);
    const float sum = wA + wE + wO;
    if (sum > 1e-6f) { wA /= sum; wE /= sum; wO /= sum; }

    out.aaa = wA;
    out.eee = wE;
    out.ooo = wO;

    return out;
}
