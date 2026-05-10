#include "PluginProcessor.h"
#include "PluginEditor.h"

VoiceSynthProcessor::VoiceSynthProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    micBuffer.setSize(1, MIC_BUF);
    micBuffer.clear();
}

VoiceSynthProcessor::~VoiceSynthProcessor()
{
    micDeviceManager.removeAudioCallback(this);
    micDeviceManager.closeAudioDevice();
}

juce::StringArray VoiceSynthProcessor::getAvailableInputDevices()
{
    micDeviceManager.initialise(1, 0, nullptr, true);
    auto* type = micDeviceManager.getCurrentDeviceTypeObject();
    if (type) return type->getDeviceNames(true);
    return {};
}

void VoiceSynthProcessor::openMicDevice(const juce::String& deviceName)
{
    micDeviceManager.initialise(1, 0, nullptr, true);
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    micDeviceManager.getAudioDeviceSetup(setup);
    setup.inputDeviceName  = deviceName;
    setup.outputDeviceName = "";
    setup.inputChannels    = 1;
    setup.outputChannels   = 0;
    setup.sampleRate       = sampleRate_;
    micDeviceManager.setAudioDeviceSetup(setup, true);
    micDeviceManager.addAudioCallback(this);
    micInitialized = true;
}

void VoiceSynthProcessor::prepareToPlay(double sampleRate, int)
{
    sampleRate_ = sampleRate;
    micBuffer.setSize(1, MIC_BUF);
    micBuffer.clear();
    micWritePos = 0;
    oscPhase    = 0.0;
    adsrState   = AState::IDLE;
    adsrLevel   = 0.0f;
}

void VoiceSynthProcessor::releaseResources()
{
    micDeviceManager.removeAudioCallback(this);
    micDeviceManager.closeAudioDevice();
    micInitialized = false;
}

void VoiceSynthProcessor::audioDeviceIOCallbackWithContext(
    const float* const* in, int numIn,
    float* const*, int,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    if (numIn < 1 || in[0] == nullptr) return;
    auto* buf = micBuffer.getWritePointer(0);
    int   pos = micWritePos.load();
    float lv  = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        buf[pos] = in[0][i];
        pos = (pos + 1) % MIC_BUF;
        lv  = std::max(lv, std::abs(in[0][i]));
    }
    micWritePos.store(pos);
    inputLevel.store(lv);
}

float VoiceSynthProcessor::nextOscSample()
{
    float s = 0.0f;
    switch (oscType)
    {
        case OscType::SINE:
            s = std::sin(oscPhase); break;
        case OscType::SAW:
            s = (float)(oscPhase / juce::MathConstants<double>::pi - 1.0); break;
        case OscType::SQUARE:
            s = oscPhase < juce::MathConstants<double>::pi ? 1.0f : -1.0f; break;
        case OscType::TRIANGLE:
            s = (float)(2.0 * std::abs(2.0 * (oscPhase / juce::MathConstants<double>::twoPi) - 1.0) - 1.0); break;
    }
    oscPhase += juce::MathConstants<double>::twoPi
              * midiToFreq(currentMidiNote.load()) / sampleRate_;
    if (oscPhase >= juce::MathConstants<double>::twoPi)
        oscPhase -= juce::MathConstants<double>::twoPi;
    return s;
}

void VoiceSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int N  = buffer.getNumSamples();
    const int nO = getTotalNumOutputChannels();

    for (const auto m : midi)
    {
        auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            currentMidiNote.store(note);
            pitchRatio = midiToFreq(note) / midiToFreq(BASE_NOTE);
            adsrState  = AState::ATTACK;
        }
        else if (msg.isNoteOff() && msg.getNoteNumber() == currentMidiNote.load())
        {
            adsrState = AState::RELEASE;
        }
    }

    for (int ch = 0; ch < nO; ++ch) buffer.clear(ch, 0, N);

    float outLv = 0.0f;
    auto* outL  = buffer.getWritePointer(0);
    auto* outR  = nO > 1 ? buffer.getWritePointer(1) : nullptr;

    const float attackStep  = 1.0f / (float)(attack  * sampleRate_);
    const float releaseStep = 1.0f / (float)(release * sampleRate_);

    for (int i = 0; i < N; ++i)
    {
        // ADSR envelope
        if      (adsrState == AState::ATTACK)  { adsrLevel += attackStep;  if (adsrLevel >= 1.0f) { adsrLevel = 1.0f; adsrState = AState::SUSTAIN; } }
        else if (adsrState == AState::RELEASE) { adsrLevel -= releaseStep; if (adsrLevel <= 0.0f) { adsrLevel = 0.0f; adsrState = AState::IDLE; currentMidiNote.store(-1); } }
        else if (adsrState == AState::IDLE)    { continue; }

        float sample = 0.0f;

        if (sourceMode == SourceMode::SYNTH)
        {
            sample = nextOscSample();
        }
        else // MIC
        {
            auto* mic = micBuffer.getReadPointer(0);
            float rh  = (float)((micWritePos.load() - N + i * pitchRatio
                                  + MIC_BUF * 4) % MIC_BUF);
            int   i0  = (int)rh % MIC_BUF;
            int   i1  = (i0 + 1) % MIC_BUF;
            float fr  = rh - std::floor(rh);
            sample    = mic[i0] * (1.0f - fr) + mic[i1] * fr;
        }

        sample *= adsrLevel * volume;
        outL[i] = sample;
        if (outR) outR[i] = sample;
        outLv = std::max(outLv, std::abs(sample));
    }

    outputLevel.store(outLv);
}

juce::AudioProcessorEditor* VoiceSynthProcessor::createEditor()
{ return new VoiceSynthEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{ return new VoiceSynthProcessor(); }
