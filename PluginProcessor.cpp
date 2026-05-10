#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VoiceSynthProcessor::VoiceSynthProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Microphone",  juce::AudioChannelSet::mono(),   true)
        .withOutput("Output",      juce::AudioChannelSet::stereo(), true))
{
    micBuffer.setSize(1, MIC_BUFFER_SIZE);
    micBuffer.clear();
}

VoiceSynthProcessor::~VoiceSynthProcessor() {}

//==============================================================================
void VoiceSynthProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    micBuffer.setSize(1, MIC_BUFFER_SIZE);
    micBuffer.clear();
    micWritePos = 0;
    currentMidiNote = -1;
    currentPitchRatio = 1.0f;
}

void VoiceSynthProcessor::releaseResources() {}

//==============================================================================
void VoiceSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples     = buffer.getNumSamples();
    const int numInputCh     = getTotalNumInputChannels();
    const int numOutputCh    = getTotalNumOutputChannels();

    // ================================================================
    // 1. MANDRAY FEO MICROPHONE → ring buffer
    // ================================================================
    float inLevel = 0.0f;
    if (numInputCh > 0)
    {
        auto* micIn   = buffer.getReadPointer(0);
        auto* writeBuf = micBuffer.getWritePointer(0);
        int   writePos = micWritePos.load();

        for (int i = 0; i < numSamples; ++i)
        {
            float s = micIn[i];
            writeBuf[writePos] = s;
            writePos = (writePos + 1) % MIC_BUFFER_SIZE;
            inLevel = std::max(inLevel, std::abs(s));
        }
        micWritePos.store(writePos);
    }
    inputLevel.store(inLevel);

    // ================================================================
    // 2. MIDI — jerena ny keys voa tsindry
    // ================================================================
    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            currentMidiNote.store(note);
            float targetFreq = midiNoteToFreq(note);
            float baseFreq   = midiNoteToFreq(BASE_NOTE);
            currentPitchRatio = targetFreq / baseFreq;
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == currentMidiNote.load())
                currentMidiNote.store(-1);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            currentMidiNote.store(-1);
        }
    }

    // ================================================================
    // 3. MAMADIKA FEO → pitch-shifted output
    // ================================================================

    // Anadio output aloha
    for (int ch = 0; ch < numOutputCh; ++ch)
        buffer.clear(ch, 0, numSamples);

    float outLevel = 0.0f;

    if (currentMidiNote.load() >= 0)
    {
        auto* micData = micBuffer.getReadPointer(0);
        int   writePos = micWritePos.load();

        auto* outL = buffer.getWritePointer(0);
        auto* outR = (numOutputCh > 1) ? buffer.getWritePointer(1) : nullptr;

        // Read head: manomboka amin'ny faran'ny buffer voasoratra
        // ary mandeha amin'ny ratio
        float readHead = (float)((writePos - numSamples + MIC_BUFFER_SIZE) % MIC_BUFFER_SIZE);

        for (int i = 0; i < numSamples; ++i)
        {
            // Interpolation linéaire
            int   idx0 = (int)readHead % MIC_BUFFER_SIZE;
            int   idx1 = (idx0 + 1) % MIC_BUFFER_SIZE;
            float frac = readHead - std::floor(readHead);

            float sample = micData[idx0] * (1.0f - frac) + micData[idx1] * frac;

            outL[i] = sample;
            if (outR) outR[i] = sample;

            outLevel = std::max(outLevel, std::abs(sample));

            // Manakaiky arakaraka ny pitch ratio
            readHead += currentPitchRatio;
            if (readHead >= MIC_BUFFER_SIZE)
                readHead -= MIC_BUFFER_SIZE;
        }
    }

    outputLevel.store(outLevel);
}

//==============================================================================
juce::AudioProcessorEditor* VoiceSynthProcessor::createEditor()
{
    return new VoiceSynthEditor(*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceSynthProcessor();
}
