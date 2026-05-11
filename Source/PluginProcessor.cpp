#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
TsengoVoiceSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "threshold", 1 }, "YIN Threshold",
        juce::NormalisableRange<float> (0.05f, 0.40f, 0.01f), 0.12f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack (ms)",
        juce::NormalisableRange<float> (5.0f, 200.0f, 1.0f), 25.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release (ms)",
        juce::NormalisableRange<float> (50.0f, 2000.0f, 1.0f), 250.0f));

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
TsengoVoiceSynthProcessor::TsengoVoiceSynthProcessor()
    : AudioProcessor (BusesProperties()
        // Sidechain bus — user routes mic from FL Studio Mixer here
        .withInput  ("Mic Sidechain", juce::AudioChannelSet::mono(), false)
        // Stereo output — required by FL Studio Channel Rack instruments
        .withOutput ("Output",        juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      yin_  (kYinBuf)
{
    pThreshold = apvts.getRawParameterValue ("threshold");
    pVolume    = apvts.getRawParameterValue ("volume");
    pAttack    = apvts.getRawParameterValue ("attack");
    pRelease   = apvts.getRawParameterValue ("release");
}

TsengoVoiceSynthProcessor::~TsengoVoiceSynthProcessor() {}

// ─────────────────────────────────────────────────────────────────────────────
bool TsengoVoiceSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Output must be mono or stereo
    const auto& outSet = layouts.getMainOutputChannelSet();
    if (outSet != juce::AudioChannelSet::mono() &&
        outSet != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain: accept mono or disabled (FL Studio may not always enable it)
    const auto& scSet = layouts.getChannelSet (true, 1);
    if (!scSet.isDisabled() &&
        scSet != juce::AudioChannelSet::mono() &&
        scSet != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    yin_.prepare (static_cast<float> (sampleRate), kYinBuf);
    currentNote_    = -1;
    candidateNote_  = -1;
    candidateCount_ = 0;
    releaseCounter_ = 0;
    juce::ignoreUnused (samplesPerBlock);
}

void TsengoVoiceSynthProcessor::releaseResources()
{
    // Send noteOff for any hanging note
    currentNote_   = -1;
    candidateNote_ = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
int TsengoVoiceSynthProcessor::attackSamples() const noexcept
{
    return static_cast<int> ((*pAttack / 1000.0f) * (float)getSampleRate());
}

int TsengoVoiceSynthProcessor::releaseSamples() const noexcept
{
    return static_cast<int> ((*pRelease / 1000.0f) * (float)getSampleRate());
}

void TsengoVoiceSynthProcessor::emitNoteOn (int note, uint8_t velocity,
                                             juce::MidiBuffer& midi, int offset)
{
    midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), offset);
}

void TsengoVoiceSynthProcessor::emitNoteOff (int note,
                                              juce::MidiBuffer& midi, int offset)
{
    midi.addEvent (juce::MidiMessage::noteOff (1, note, (juce::uint8)0), offset);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear incoming MIDI — we are the producer
    midiMessages.clear();

    const int totalSamples = buffer.getNumSamples();
    const int numOutputCh  = getTotalNumOutputChannels();

    // ── Read audio from sidechain bus (mic input) ─────────────────────────
    // FL Studio routes the mic track to our sidechain bus.
    // Bus index 0 = main output, Bus index 1 = sidechain input.
    const bool hasSidechain = getBusCount (true) > 0 &&
                              !getBus (true, 0)->getCurrentLayout().isDisabled();

    float inputPeak = 0.0f;
    const float* micData = nullptr;

    if (hasSidechain)
    {
        // Get the sidechain buffer using JUCE bus helper
        auto scBuffer = getBusBuffer (buffer, true, 0);
        if (scBuffer.getNumChannels() > 0 && scBuffer.getNumSamples() > 0)
        {
            micData   = scBuffer.getReadPointer (0);
            inputPeak = scBuffer.getMagnitude (0, 0, scBuffer.getNumSamples());
        }
    }

    // Clear output (this is an instrument — it outputs silence unless pass-through)
    for (int ch = 0; ch < numOutputCh; ++ch)
        buffer.clear (ch, 0, totalSamples);

    // ── YIN pitch detection ───────────────────────────────────────────────
    YinPitchDetector::Result result;

    if (micData != nullptr && inputPeak > 0.001f)
    {
        result = yin_.analyseBlock (micData, totalSamples);
    }

    // Confidence threshold: correct direction (confidence >= 1 - threshold)
    const float thresh = *pThreshold;
    const bool  hasPitch = result.midiNote >= 0 &&
                           result.confidence >= (1.0f - thresh);

    const int detectedNote = hasPitch ? result.midiNote : -1;

    // ── MIDI state machine ─────────────────────────────────────────────────
    const int atkSamp = attackSamples();
    const int relSamp = releaseSamples();

    if (detectedNote < 0)
    {
        candidateNote_  = -1;
        candidateCount_ = 0;

        if (currentNote_ >= 0)
        {
            releaseCounter_ += totalSamples;
            if (releaseCounter_ >= relSamp)
            {
                emitNoteOff (currentNote_, midiMessages, totalSamples - 1);
                currentNote_    = -1;
                releaseCounter_ = 0;
            }
        }
    }
    else
    {
        releaseCounter_ = 0;

        if (detectedNote == candidateNote_)
            candidateCount_ += totalSamples;
        else
        {
            candidateNote_  = detectedNote;
            candidateCount_ = totalSamples;
        }

        if (candidateCount_ >= atkSamp)
        {
            if (currentNote_ != detectedNote)
            {
                // Dynamic velocity from input level (40–110)
                const uint8_t vel = static_cast<uint8_t> (
                    juce::jlimit (40, 110,
                                  (int)(40.0f + inputPeak * 70.0f / 0.5f)));

                if (currentNote_ >= 0)
                    emitNoteOff (currentNote_, midiMessages, 0);

                emitNoteOn (detectedNote, vel, midiMessages, 0);
                currentNote_    = detectedNote;
                candidateCount_ = 0;
            }
        }
    }

    // ── Update shared state for UI ─────────────────────────────────────────
    {
        juce::SpinLock::ScopedLockType lock (stateLock_);
        sharedState_.frequency    = result.frequency;
        sharedState_.confidence   = result.confidence;
        sharedState_.midiNote     = currentNote_;
        sharedState_.midiCents    = result.midiCents;
        sharedState_.inputLevel   = inputPeak;
        sharedState_.micConnected = hasSidechain;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
TsengoVoiceSynthProcessor::DetectionState
TsengoVoiceSynthProcessor::getDetectionState() const noexcept
{
    juce::SpinLock::ScopedLockType lock (stateLock_);
    return sharedState_;
}

juce::AudioProcessorEditor* TsengoVoiceSynthProcessor::createEditor()
{
    return new TsengoVoiceSynthEditor (*this);
}

void TsengoVoiceSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void TsengoVoiceSynthProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TsengoVoiceSynthProcessor();
}
