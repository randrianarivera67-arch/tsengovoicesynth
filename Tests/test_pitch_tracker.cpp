// Unit tests for the framework-free DSP core.
// Build:  cmake -B build-tests -DTSENGO_TESTS_ONLY=ON && cmake --build build-tests && ctest --test-dir build-tests
#include "PitchTracker.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what)
    {
        std::printf ("%-52s %s\n", what, ok ? "ok" : "FAILED");
        if (! ok) ++failures;
    }

    constexpr double kPi = 3.14159265358979323846;

    // A voice-like tone: fundamental plus two harmonics, with a little noise.
    void fillTone (float* buf, int n, float hz, float sr, float noiseAmp, std::mt19937& rng)
    {
        std::normal_distribution<float> noise (0.f, noiseAmp);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / (double) sr;
            buf[i] = (float) (0.60 * std::sin (2.0 * kPi * hz * t)
                            + 0.30 * std::sin (4.0 * kPi * hz * t)
                            + 0.15 * std::sin (6.0 * kPi * hz * t))
                     + noise (rng);
        }
    }

    float centsError (float measured, float target)
    {
        if (measured <= 0.f) return 1.0e9f;
        return 1200.f * std::log2 (measured / target);
    }
}

int main()
{
    const float sr = 44100.f;
    const int   N  = 2048;
    std::vector<float> buf ((size_t) N);
    std::mt19937 rng (1234);

    PitchTracker pt;
    pt.prepare ((double) sr, 4096);
    pt.setRange (70.f, 1100.f);

    // ---- accuracy across the singing range ------------------------------
    const float notes[] = { 82.41f, 110.f, 146.83f, 220.f, 329.63f, 440.f, 587.33f, 880.f };
    bool allAccurate = true;
    for (float f : notes)
    {
        fillTone (buf.data(), N, f, sr, 0.02f, rng);
        const auto r = pt.analyse (buf.data(), N);
        const float err = centsError (r.hz, f);
        if (std::fabs (err) >= 15.f)
        {
            allAccurate = false;
            std::printf ("  %.2f Hz -> %.2f Hz (%.1f cents)\n", f, r.hz, err);
        }
    }
    check (allAccurate, "pitch within 15 cents, 82 Hz - 880 Hz");

    // ---- no octave errors on a low, harmonic-rich tone -------------------
    fillTone (buf.data(), N, 98.f, sr, 0.05f, rng);
    {
        const auto r = pt.analyse (buf.data(), N);
        check (std::fabs (centsError (r.hz, 98.f)) < 20.f, "no octave error on 98 Hz");
    }

    // ---- material outside the configured range must be rejected ---------
    pt.setRange (200.f, 400.f);
    fillTone (buf.data(), N, 90.f, sr, 0.01f, rng);
    {
        const auto r = pt.analyse (buf.data(), N);
        check (r.hz < 0.f, "90 Hz rejected when range is 200-400 Hz");
    }

    // ---- noise must not be reported as a note ---------------------------
    {
        std::normal_distribution<float> n (0.f, 0.3f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = n (rng);
        pt.setRange (70.f, 1100.f);
        const auto r = pt.analyse (buf.data(), N);
        check (r.hz < 0.f || r.confidence < 0.6f, "white noise is not a confident note");
    }

    // ---- median filter rides over single-frame dropouts ------------------
    {
        PitchTracker m;
        m.prepare ((double) sr, 2048);
        float last = 0.f;
        const float seq[] = { 220.f, 220.f, 220.f, -1.f, 220.f, 221.f, 219.f };
        for (float v : seq) last = m.medianFiltered (v);
        check (std::fabs (last - 220.f) < 2.f, "median ignores a one-frame dropout");
    }

    // ---- range setter keeps min < max ------------------------------------
    {
        PitchTracker m;
        m.prepare ((double) sr, 2048);
        m.setRange (500.f, 100.f);          // deliberately inverted
        check (m.getMaxHz() > m.getMinHz(), "inverted range is corrected");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
