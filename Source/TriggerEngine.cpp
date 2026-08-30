#include "TriggerEngine.h"
#include <cmath>
#include <algorithm>

//==============================================================================
namespace
{
    // Frequency axis is handled in log space: 60 Hz .. 12 kHz mapped to 0..1.
    inline float logFreqNorm (float hz)
    {
        if (hz < 20.f) hz = 20.f;
        return juce::jlimit (0.f, 1.f, std::log (hz / 60.f) / std::log (200.f));
    }
}

//==============================================================================
TriggerEngine::TriggerEngine()
{
    for (int i = 0; i < kNumFeatures; ++i)
        invStd_[(size_t) i].store (5.f);

    for (int i = 0; i < kFftSize; ++i)
        window_[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                       * (float) i / (float) (kFftSize - 1));
}

void TriggerEngine::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void TriggerEngine::reset()
{
    ring_.fill (0.f);
    writePos_     = 0;
    hopFill_      = 0;
    envSlow_      = 0.f;
    blockGap_     = 0;
    pendingCount_ = -1;
    onsetPos_     = 0;
    fifoRead_.store (fifoWrite_.load());
}

//==============================================================================
// Mic thread
void TriggerEngine::processAudio (const float* in, int numSamples)
{
    if (in == nullptr) return;

    const bool active = enabled.load() || trainingPad_.load() >= 0;

    for (int i = 0; i < numSamples; ++i)
    {
        ring_[(size_t) writePos_] = in[i];
        writePos_ = (writePos_ + 1) & (kRingSize - 1);

        if (pendingCount_ > 0)
        {
            if (--pendingCount_ == 0)
            {
                pendingCount_ = -1;
                handleOnsetWindow();
            }
        }

        if (++hopFill_ >= kHop)
        {
            hopFill_ = 0;

            // ---- hop-rate onset detection ----
            float sum = 0.f;
            for (int j = 0; j < kHop; ++j)
            {
                const float s = ring_[(size_t) ((writePos_ - kHop + j + kRingSize) & (kRingSize - 1))];
                sum += s * s;
            }
            const float rms = std::sqrt (sum / (float) kHop);
            onsetLevel_.store (rms);

            if (blockGap_ > 0) --blockGap_;

            if (active && blockGap_ == 0 && pendingCount_ < 0)
            {
                const float sens   = juce::jlimit (0.f, 1.f, sensitivity.load());
                const float absThr = juce::jmax (noiseFloor.load() * 3.f, 0.006f) * (1.6f - sens);
                const float ratio  = 2.2f - sens * 1.2f;

                if (rms > absThr && rms > envSlow_ * ratio)
                {
                    // Start the analysis window one hop *before* the onset so
                    // the attack transient is inside it.
                    onsetPos_     = (writePos_ - kHop * 2 + kRingSize) & (kRingSize - 1);
                    pendingCount_ = kFftSize - kHop * 2;
                    blockGap_     = juce::jmax (4, (int) (0.075 * sampleRate_ / (double) kHop));
                }
            }

            envSlow_ = envSlow_ * 0.85f + rms * 0.15f;
        }
    }
}

//==============================================================================
void TriggerEngine::handleOnsetWindow()
{
    for (int i = 0; i < kFftSize; ++i)
        timeBuf_[(size_t) i] = ring_[(size_t) ((onsetPos_ + i) & (kRingSize - 1))];

    FeatureVec f {};
    float peak = 0.f;
    extractFeatures (timeBuf_.data(), kFftSize, f, peak);

    const int trainPad = trainingPad_.load();

    if (trainPad >= 0 && trainPad < kNumPads)
    {
        auto& pad = pads_[(size_t) trainPad];
        const int c = pad.count.load (std::memory_order_relaxed);
        if (c < kMaxExamples)
        {
            pad.examples[(size_t) c] = f;
            pad.count.store (c + 1, std::memory_order_release);
        }
        lastPad_.store (trainPad);
        lastConfidence_.store (1.f);
        hitCounter_.fetch_add (1);
        return;
    }

    if (! enabled.load()) return;

    float conf = 0.f;
    const int pad = classify (f, conf);
    if (pad < 0) return;

    Event e;
    e.pad        = pad;
    e.confidence = conf;
    e.velocity   = juce::jlimit (1, 127, juce::roundToInt (juce::jmap (
                        juce::jlimit (0.f, 1.f, peak * 2.5f), 0.f, 1.f, 45.f, 127.f)));

    lastPad_.store (pad);
    lastConfidence_.store (conf);
    hitCounter_.fetch_add (1);
    pushEvent (e);
}

//==============================================================================
void TriggerEngine::extractFeatures (const float* w, int n, FeatureVec& out, float& peak)
{
    out.fill (0.f);
    peak = 0.f;

    // ---- time domain ----
    float sumSq = 0.f;
    int   zc    = 0;
    for (int i = 0; i < n; ++i)
    {
        const float s = w[i];
        sumSq += s * s;
        peak = juce::jmax (peak, std::abs (s));
        if (i > 0 && ((s >= 0.f) != (w[i - 1] >= 0.f))) ++zc;
    }
    const float rms   = std::sqrt (sumSq / (float) n);
    const float zcr   = (float) zc / (float) n;
    const float crest = rms > 1e-7f ? peak / rms : 0.f;

    // ---- spectrum ----
    std::fill (fftBuf_.begin(), fftBuf_.end(), 0.f);
    for (int i = 0; i < kFftSize; ++i)
        fftBuf_[(size_t) i] = w[i] * window_[(size_t) i];

    fft_.performFrequencyOnlyForwardTransform (fftBuf_.data());

    const int   numBins = kFftSize / 2;
    const float binHz   = (float) (sampleRate_ / (double) kFftSize);

    double e = 0.0, eLf = 0.0, logSum = 0.0;
    int    counted = 0;

    for (int i = 1; i < numBins; ++i)
    {
        const float mag = fftBuf_[(size_t) i];
        const float p   = mag * mag;
        e   += p;
        eLf += (double) p * (double) logFreqNorm ((float) i * binHz);
        logSum += std::log ((double) p + 1e-12);
        ++counted;
    }

    if (e < 1e-12 || counted == 0)
    {
        out[0] = zcr;
        return;
    }

    const float centroid = (float) (eLf / e);

    double spreadAcc = 0.0;
    double cum       = 0.0;
    float  rolloff   = 1.f;
    bool   gotRoll   = false;
    const double target = e * 0.85;

    for (int i = 1; i < numBins; ++i)
    {
        const float mag = fftBuf_[(size_t) i];
        const float p   = mag * mag;
        const float lf  = logFreqNorm ((float) i * binHz);
        spreadAcc += (double) p * (double) ((lf - centroid) * (lf - centroid));

        cum += p;
        if (! gotRoll && cum >= target) { rolloff = lf; gotRoll = true; }
    }

    const float spread   = (float) std::sqrt (spreadAcc / e);
    const float geoMean  = (float) std::exp (logSum / (double) counted);
    const float ariMean  = (float) (e / (double) counted);
    const float flatness = ariMean > 1e-12f
                             ? juce::jlimit (0.f, 1.f, (std::log10 (geoMean / ariMean) + 6.f) / 6.f)
                             : 0.f;

    // ---- 8 log-spaced band energies, loudness-invariant ----
    const float loHz = 60.f;
    const float hiHz = juce::jmin (12000.f, (float) (sampleRate_ * 0.45));
    double bands[8] = { 0,0,0,0,0,0,0,0 };

    for (int i = 1; i < numBins; ++i)
    {
        const float hz = (float) i * binHz;
        if (hz < loHz || hz > hiHz) continue;
        const float t = std::log (hz / loHz) / std::log (hiHz / loHz);
        const int   b = juce::jlimit (0, 7, (int) (t * 8.f));
        const float mag = fftBuf_[(size_t) i];
        bands[b] += (double) mag * (double) mag;
    }

    out[0] = zcr;
    out[1] = centroid;
    out[2] = juce::jlimit (0.f, 1.f, spread * 2.f);
    out[3] = rolloff;
    out[4] = flatness;
    out[5] = juce::jlimit (0.f, 1.f, crest / 12.f);

    for (int b = 0; b < 8; ++b)
    {
        const float r = (float) (bands[b] / e);
        out[6 + b] = juce::jlimit (0.f, 1.f, (std::log10 (r + 1e-5f) + 5.f) / 5.f);
    }
}

//==============================================================================
int TriggerEngine::classify (const FeatureVec& f, float& confidenceOut) const
{
    confidenceOut = 0.f;

    float bestDist = 1.0e9f;
    int   bestPad  = -1;

    for (int p = 0; p < kNumPads; ++p)
    {
        const int c = pads_[(size_t) p].count.load (std::memory_order_acquire);
        if (c < 2) continue;   // untrained pads never fire

        for (int ex = 0; ex < c; ++ex)
        {
            const FeatureVec& e = pads_[(size_t) p].examples[(size_t) ex];
            float d = 0.f;
            for (int k = 0; k < kNumFeatures; ++k)
            {
                const float diff = (f[(size_t) k] - e[(size_t) k]) * invStd_[(size_t) k].load();
                d += diff * diff;
            }
            d /= (float) kNumFeatures;

            if (d < bestDist) { bestDist = d; bestPad = p; }
        }
    }

    if (bestPad < 0) return -1;

    const float strict = juce::jlimit (0.f, 1.f, strictness.load());
    const float reject = juce::jmap (strict, 0.f, 1.f, 8.f, 1.f);
    if (bestDist > reject) return -1;

    confidenceOut = juce::jlimit (0.f, 1.f, std::exp (-bestDist * 0.5f));
    return bestPad;
}

//==============================================================================
void TriggerEngine::pushEvent (const Event& e)
{
    const int w    = fifoWrite_.load (std::memory_order_relaxed);
    const int next = (w + 1) % kFifoSize;
    if (next == fifoRead_.load (std::memory_order_acquire)) return;   // full, drop

    fifo_[(size_t) w] = e;
    fifoWrite_.store (next, std::memory_order_release);
}

bool TriggerEngine::popEvent (Event& e)
{
    const int r = fifoRead_.load (std::memory_order_relaxed);
    if (r == fifoWrite_.load (std::memory_order_acquire)) return false;

    e = fifo_[(size_t) r];
    fifoRead_.store ((r + 1) % kFifoSize, std::memory_order_release);
    return true;
}

//==============================================================================
// UI thread
void TriggerEngine::beginTraining (int pad)
{
    if (pad < 0 || pad >= kNumPads) return;
    trainingPad_.store (pad);
}

void TriggerEngine::endTraining()
{
    trainingPad_.store (-1);
    recomputeStats();
}

int TriggerEngine::getExampleCount (int pad) const
{
    if (pad < 0 || pad >= kNumPads) return 0;
    return pads_[(size_t) pad].count.load (std::memory_order_acquire);
}

void TriggerEngine::clearPad (int pad)
{
    if (pad < 0 || pad >= kNumPads) return;
    pads_[(size_t) pad].count.store (0, std::memory_order_release);
    recomputeStats();
}

void TriggerEngine::clearAll()
{
    for (int p = 0; p < kNumPads; ++p)
        pads_[(size_t) p].count.store (0, std::memory_order_release);
    recomputeStats();
}

//==============================================================================
// Per-feature spread across every stored example — used to z-score distances so
// one wide-ranging feature can't dominate the match.
void TriggerEngine::recomputeStats()
{
    double sum[kNumFeatures]   = {};
    double sumSq[kNumFeatures] = {};
    int    total = 0;

    for (int p = 0; p < kNumPads; ++p)
    {
        const int c = pads_[(size_t) p].count.load (std::memory_order_acquire);
        for (int ex = 0; ex < c; ++ex)
        {
            const FeatureVec& e = pads_[(size_t) p].examples[(size_t) ex];
            for (int k = 0; k < kNumFeatures; ++k)
            {
                sum[k]   += e[(size_t) k];
                sumSq[k] += (double) e[(size_t) k] * (double) e[(size_t) k];
            }
            ++total;
        }
    }

    if (total < 2)
    {
        for (int k = 0; k < kNumFeatures; ++k)
            invStd_[(size_t) k].store (5.f);
        return;
    }

    for (int k = 0; k < kNumFeatures; ++k)
    {
        const double mean = sum[k] / (double) total;
        const double var  = juce::jmax (0.0, sumSq[k] / (double) total - mean * mean);
        const float  sd   = juce::jmax (0.02f, (float) std::sqrt (var));
        invStd_[(size_t) k].store (juce::jlimit (0.5f, 50.f, 1.f / sd));
    }
}

//==============================================================================
// State persistence — training survives saving/reloading the project.
juce::String TriggerEngine::saveToString() const
{
    juce::String s ("TVSTRIG1");

    for (int p = 0; p < kNumPads; ++p)
    {
        const int c = pads_[(size_t) p].count.load (std::memory_order_acquire);
        s << "|" << juce::String (c);
        for (int ex = 0; ex < c; ++ex)
        {
            const FeatureVec& e = pads_[(size_t) p].examples[(size_t) ex];
            for (int k = 0; k < kNumFeatures; ++k)
                s << "," << juce::String (e[(size_t) k], 5);
        }
    }
    return s;
}

void TriggerEngine::loadFromString (const juce::String& str)
{
    clearAll();
    if (! str.startsWith ("TVSTRIG1")) return;

    auto padChunks = juce::StringArray::fromTokens (str, "|", "");
    // padChunks[0] is the header tag
    for (int p = 0; p < kNumPads && p + 1 < padChunks.size(); ++p)
    {
        auto nums = juce::StringArray::fromTokens (padChunks[p + 1], ",", "");
        if (nums.size() < 1) continue;

        const int c = juce::jlimit (0, kMaxExamples, nums[0].getIntValue());
        int idx = 1;
        int stored = 0;

        for (int ex = 0; ex < c; ++ex)
        {
            if (idx + kNumFeatures > nums.size()) break;
            FeatureVec f {};
            for (int k = 0; k < kNumFeatures; ++k)
                f[(size_t) k] = nums[idx + k].getFloatValue();
            idx += kNumFeatures;
            pads_[(size_t) p].examples[(size_t) stored] = f;
            ++stored;
        }
        pads_[(size_t) p].count.store (stored, std::memory_order_release);
    }

    recomputeStats();
}
