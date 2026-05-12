#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TsengoProcessor::TsengoProcessor()
    : AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{}

TsengoProcessor::~TsengoProcessor()
{
    closeDevice();
}

//==============================================================================
juce::StringArray TsengoProcessor::getInputDevices()
{
    micMgr_.initialise (1, 0, nullptr, true);
    auto* t = micMgr_.getCurrentDeviceTypeObject();
    if (t) return t->getDeviceNames (true);
    return {};
}

void TsengoProcessor::openDevice (const juce::String& name)
{
    closeDevice();
    micMgr_.initialise (1, 0, nullptr, true);

    juce::AudioDeviceManager::AudioDeviceSetup s;
    micMgr_.getAudioDeviceSetup (s);
    s.inputDeviceName  = name;
    s.outputDeviceName = "";
    s.inputChannels    = 1;
    s.outputChannels   = 0;
    s.sampleRate       = sampleRate_;
    s.bufferSize       = 512;
    micMgr_.setAudioDeviceSetup (s, true);
    micMgr_.addAudioCallback (this);
    currentDevice_ = name;
    deviceOpen_    = true;
}

void TsengoProcessor::closeDevice()
{
    if (deviceOpen_)
    {
        micMgr_.removeAudioCallback (this);
        micMgr_.closeAudioDevice();
        deviceOpen_ = false;
    }
}

//==============================================================================
void TsengoProcessor::prepareToPlay (double sr, int)
{
    sampleRate_ = sr;
    medBuf_.clear();
    ringWrite_ = 0;
}

void TsengoProcessor::releaseResources()
{
    closeDevice();
}

//==============================================================================
// Mic callback — writes into ring buffer
void TsengoProcessor::audioDeviceIOCallbackWithContext (
    const float* const* in, int numIn,
    float* const*, int,
    int N,
    const juce::AudioIODeviceCallbackContext&)
{
    if (numIn < 1 || in[0] == nullptr) return;

    int   wp  = ringWrite_.load();
    float lv  = 0.f;

    for (int i = 0; i < N; ++i)
    {
        float s      = in[0][i] * gain.load();
        ring_[wp]    = s;
        wp           = (wp + 1) & (RING - 1);
        lv           = std::max (lv, std::abs (s));
    }

    ringWrite_.store (wp);
    micLevel_.store (lv);
}

//==============================================================================
// Parabolic interpolation for sub-sample accuracy
float TsengoProcessor::parabolicInterp (const juce::Array<float>& d, int tau)
{
    if (tau <= 0 || tau >= d.size() - 1) return (float)tau;
    float s0 = d[tau - 1], s1 = d[tau], s2 = d[tau + 1];
    float denom = s0 - 2.f * s1 + s2;
    if (std::abs (denom) < 1e-8f) return (float)tau;
    return (float)tau + 0.5f * (s0 - s2) / denom;
}

//==============================================================================
// YIN algorithm — haute précision
float TsengoProcessor::yinDetect (const float* buf, int N, float sr)
{
    const int   W     = N / 2;
    const float THR   = threshold.load();

    juce::Array<float> d;
    d.resize (W);

    // Step 1: difference function
    d.set (0, 0.f);
    for (int tau = 1; tau < W; ++tau)
    {
        float sum = 0.f;
        for (int j = 0; j < W; ++j)
        {
            float diff = buf[j] - buf[j + tau];
            sum += diff * diff;
        }
        d.set (tau, sum);
    }

    // Step 2: cumulative mean normalised difference
    juce::Array<float> dnorm;
    dnorm.resize (W);
    dnorm.set (0, 1.f);
    float runSum = 0.f;
    for (int tau = 1; tau < W; ++tau)
    {
        runSum += d[tau];
        dnorm.set (tau, runSum > 0.f ? d[tau] * (float)tau / runSum : 1.f);
    }

    // Step 3: absolute threshold
    int bestTau = -1;
    for (int tau = 2; tau < W; ++tau)
    {
        if (dnorm[tau] < THR)
        {
            // Local minimum search
            while (tau + 1 < W && dnorm[tau + 1] < dnorm[tau])
                ++tau;
            bestTau = tau;
            confidence_.store (1.f - dnorm[tau]);
            break;
        }
    }

    if (bestTau < 2)
    {
        // Fallback: global minimum
        bestTau = 2;
        float minVal = dnorm[2];
        for (int tau = 3; tau < W; ++tau)
            if (dnorm[tau] < minVal) { minVal = dnorm[tau]; bestTau = tau; }
        confidence_.store (std::max (0.f, 1.f - minVal));
    }

    // Step 4: parabolic interpolation
    float tauF = parabolicInterp (dnorm, bestTau);
    if (tauF < 1.f) return -1.f;

    return sr / tauF;
}

//==============================================================================
void TsengoProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int N  = 2048;
    const int wp = ringWrite_.load();

    // Read N samples from ring buffer
    float lv = micLevel_.load();
    if (lv < threshold.load() * 0.5f)
    {
        // Silence — send noteOff if needed
        if (lastMidiNote_ >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, lastMidiNote_, (juce::uint8)0), 0);
            lastMidiNote_ = -1;
            currentNote_.store (-1);
        }
        midiLevel_.store (0.f);
        return;
    }

    // Fill analysis window
    for (int i = 0; i < N; ++i)
        yinBuf_[i] = ring_[(wp - N + i + RING) & (RING - 1)];

    float hz = yinDetect (yinBuf_, N, (float)sampleRate_);

    // Median filter — stabilise pitch
    medBuf_.push_back (hz);
    if ((int)medBuf_.size() > MED) medBuf_.pop_front();

    std::vector<float> sorted (medBuf_.begin(), medBuf_.end());
    std::sort (sorted.begin(), sorted.end());
    float medHz = sorted[sorted.size() / 2];

    currentHz_.store (medHz);

    // Hz → MIDI note
    int note = -1;
    if (medHz > 20.f && medHz < 5000.f)
    {
        float raw  = 69.f + 12.f * std::log2 (medHz / 440.f);
        note       = juce::roundToInt (raw);
        note       = juce::jlimit (0, 127, note);
    }

    // Smoothing — hold to avoid flicker
    if (note == lastMidiNote_)
    {
        noteHoldCount_ = 0;
    }
    else
    {
        ++noteHoldCount_;
        if (noteHoldCount_ < HOLD_FRAMES)
            note = lastMidiNote_; // keep old note
    }

    // Send MIDI
    if (note != lastMidiNote_)
    {
        if (lastMidiNote_ >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (1, lastMidiNote_, (juce::uint8)0), 0);
        if (note >= 0)
            midi.addEvent (juce::MidiMessage::noteOn  (1, note, (juce::uint8)100), 0);

        lastMidiNote_ = note;
        currentNote_.store (note);
        noteHoldCount_ = 0;
    }

    midiLevel_.store (note >= 0 ? 1.f : 0.f);
}

//==============================================================================
juce::AudioProcessorEditor* TsengoProcessor::createEditor()
{
    return new TsengoEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TsengoProcessor();
}
