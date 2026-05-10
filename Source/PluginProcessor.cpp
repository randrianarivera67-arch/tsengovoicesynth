#include "PluginProcessor.h"
#include "PluginEditor.h"

VoiceSynthProcessor::VoiceSynthProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    micBuffer.setSize(1, MIC_BUFFER_SIZE);
    micBuffer.clear();
}

VoiceSynthProcessor::~VoiceSynthProcessor()
{
    micDeviceManager.removeAudioCallback(this);
    micDeviceManager.closeAudioDevice();
}

void VoiceSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    micBuffer.setSize(1, MIC_BUFFER_SIZE);
    micBuffer.clear();
    micWritePos = 0;
    currentMidiNote = -1;
    currentPitchRatio = 1.0f;

    // Sokafy mic mivantana (WASAPI) — indray mandeha monja
    if (!micInitialized)
    {
        micDeviceManager.initialise(1, 0, nullptr, true);
        micDeviceManager.addAudioCallback(this);
        micInitialized = true;
    }
}

void VoiceSynthProcessor::releaseResources()
{
    micDeviceManager.removeAudioCallback(this);
    micDeviceManager.closeAudioDevice();
    micInitialized = false;
}

// === Mic avy WASAPI → Ring buffer ===
void VoiceSynthProcessor::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData, int numInputChannels,
    float* const* /*outputChannelData*/,  int /*numOutputChannels*/,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    if (numInputChannels < 1 || inputChannelData[0] == nullptr) return;

    auto* writeBuf = micBuffer.getWritePointer(0);
    int   writePos = micWritePos.load();
    float level    = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float s = inputChannelData[0][i];
        writeBuf[writePos] = s;
        writePos = (writePos + 1) % MIC_BUFFER_SIZE;
        level = std::max(level, std::abs(s));
    }

    micWritePos.store(writePos);
    inputLevel.store(level);
}

void VoiceSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numOutputCh = getTotalNumOutputChannels();

    // MIDI
    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            currentMidiNote.store(msg.getNoteNumber());
            currentPitchRatio = midiNoteToFreq(msg.getNoteNumber())
                              / midiNoteToFreq(BASE_NOTE);
        }
        else if (msg.isNoteOff() && msg.getNoteNumber() == currentMidiNote.load())
            currentMidiNote.store(-1);
    }

    for (int ch = 0; ch < numOutputCh; ++ch)
        buffer.clear(ch, 0, numSamples);

    float outLevel = 0.0f;

    if (currentMidiNote.load() >= 0)
    {
        auto* micData = micBuffer.getReadPointer(0);
        auto* outL    = buffer.getWritePointer(0);
        auto* outR    = numOutputCh > 1 ? buffer.getWritePointer(1) : nullptr;

        float readHead = (float)((micWritePos.load() - numSamples
                                  + MIC_BUFFER_SIZE) % MIC_BUFFER_SIZE);

        for (int i = 0; i < numSamples; ++i)
        {
            int   idx0   = (int)readHead % MIC_BUFFER_SIZE;
            int   idx1   = (idx0 + 1) % MIC_BUFFER_SIZE;
            float frac   = readHead - std::floor(readHead);
            float sample = micData[idx0] * (1.0f - frac) + micData[idx1] * frac;

            outL[i] = sample;
            if (outR) outR[i] = sample;
            outLevel = std::max(outLevel, std::abs(sample));

            readHead += currentPitchRatio;
            if (readHead >= MIC_BUFFER_SIZE) readHead -= MIC_BUFFER_SIZE;
        }
    }

    outputLevel.store(outLevel);
}

juce::AudioProcessorEditor* VoiceSynthProcessor::createEditor()
{
    return new VoiceSynthEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceSynthProcessor();
}
