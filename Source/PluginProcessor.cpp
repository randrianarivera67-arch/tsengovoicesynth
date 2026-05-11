#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
TsengoVoiceSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "threshold", 1 }, "YIN Threshold",
        juce::NormalisableRange<float> (0.05f, 0.50f, 0.01f), 0.15f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack (ms)",
        juce::NormalisableRange<float> (5.0f, 200.0f, 1.0f), 30.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release (ms)",
        juce::NormalisableRange<float> (50.0f, 2000.0f, 1.0f), 300.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mincents", 1 }, "Min Cents",
        juce::NormalisableRange<float> (5.0f, 50.0f, 1.0f), 20.0f));

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
TsengoVoiceSynthProcessor::TsengoVoiceSynthProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),  true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      yin_  (kYinBuf)
{
    pThreshold = apvts.getRawParameterValue ("threshold");
    pVolume    = apvts.getRawParameterValue ("volume");
    pAttack    = apvts.getRawParameterValue ("attack");
    pRelease   = apvts.getRawParameterValue ("release");
    pMinCents  = apvts.getRawParameterValue ("mincents");
}

TsengoVoiceSynthProcessor::~TsengoVoiceSynthProcessor() {}

// ─────────────────────────────────────────────────────────────────────────────
bool TsengoVoiceSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept: mono input, mono or stereo output
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::mono()   &&
        layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()   &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

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
    currentNote_   = -1;
    candidateNote_ = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
inline int TsengoVoiceSynthProcessor::attackThresh() const noexcept
{
    // Convert ms → samples
    return static_cast<int> ((*pAttack / 1000.0f) * static_cast<float> (getSampleRate()));
}

inline int TsengoVoiceSynthProcessor::releaseThresh() const noexcept
{
    return static_cast<int> ((*pRelease / 1000.0f) * static_cast<float> (getSampleRate()));
}

void TsengoVoiceSynthProcessor::emitNoteOn (int note, juce::MidiBuffer& midi, int offset)
{
    midi.addEvent (juce::MidiMessage::noteOn  (1, note, (juce::uint8) 100), offset);
}

void TsengoVoiceSynthProcessor::emitNoteOff (int note, juce::MidiBuffer& midi, int offset)
{
    midi.addEvent (juce::MidiMessage::noteOff (1, note, (juce::uint8) 0), offset);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();   // we produce MIDI, not consume it

    const int  totalSamples = buffer.getNumSamples();
    const int  numInputCh   = getTotalNumInputChannels();
    const int  numOutputCh  = getTotalNumOutputChannels();

    // ── Level meter ───────────────────────────────────────────────────────
    float inputPeak = 0.0f;
    if (numInputCh > 0)
        inputPeak = buffer.getMagnitude (0, 0, totalSamples);

    // ── Copy mono input to output (pass-through) ──────────────────────────
    const float vol = *pVolume;
    for (int ch = 0; ch < numOutputCh; ++ch)
    {
        const int srcCh = ch < numInputCh ? ch : 0;
        buffer.copyFrom (ch, 0, buffer, srcCh, 0, totalSamples);
        buffer.applyGain (ch, 0, totalSamples, vol);
    }

    // ── YIN analysis (every block) ────────────────────────────────────────
    const float* srcData = buffer.getReadPointer (0);
    const float  thresh  = *pThreshold;
    const float  minCents= *pMinCents;

    YinPitchDetector::Result result = yin_.analyseBlock (srcData, totalSamples);

    // Reconfigure YIN threshold if changed
    // (YIN doesn't hold a threshold member directly — it's passed per-call;
    //  here we use the result's confidence against user threshold)
    const bool hasPitch = result.midiNote >= 0 &&
                          result.confidence >= (1.0f - thresh) &&
                          std::abs (result.midiCents) <= minCents + 30.0f;

    const int detectedNote = hasPitch ? result.midiNote : -1;

    // ── MIDI state machine ─────────────────────────────────────────────────
    //
    // States:
    //   currentNote_ == -1  → idle, waiting for stable pitch
    //   currentNote_ >= 0   → note active (noteOn emitted)
    //
    // Transitions:
    //   idle + same candidate N frames → noteOn
    //   active + different pitch       → noteOff + candidate reset
    //   active + no pitch N frames     → noteOff

    const int atkSamp = attackThresh();
    const int relSamp = releaseThresh();

    if (detectedNote == -1)
    {
        // No pitch detected
        candidateNote_  = -1;
        candidateCount_ = 0;

        if (currentNote_ >= 0)
        {
            releaseCounter_ += totalSamples;
            if (releaseCounter_ >= relSamp)
            {
                emitNoteOff (currentNote_, midiMessages, 0);
                currentNote_    = -1;
                releaseCounter_ = 0;
            }
        }
    }
    else
    {
        releaseCounter_ = 0;

        if (detectedNote == candidateNote_)
        {
            candidateCount_ += totalSamples;
        }
        else
        {
            candidateNote_  = detectedNote;
            candidateCount_ = totalSamples;
        }

        if (candidateCount_ >= atkSamp)
        {
            if (currentNote_ != detectedNote)
            {
                if (currentNote_ >= 0)
                    emitNoteOff (currentNote_, midiMessages, 0);
                emitNoteOn (detectedNote, midiMessages, 0);
                currentNote_    = detectedNote;
                candidateCount_ = 0;
            }
        }
    }

    // ── Update shared state for editor ────────────────────────────────────
    {
        juce::SpinLock::ScopedLockType lock (stateLock_);
        sharedState_.frequency  = result.frequency;
        sharedState_.confidence = result.confidence;
        sharedState_.midiNote   = currentNote_;
        sharedState_.midiCents  = result.midiCents;
        sharedState_.inputLevel = inputPeak;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
TsengoVoiceSynthProcessor::DetectionState
TsengoVoiceSynthProcessor::getDetectionState() const noexcept
{
    juce::SpinLock::ScopedLockType lock (stateLock_);
    return sharedState_;
}

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* TsengoVoiceSynthProcessor::createEditor()
{
    return new TsengoVoiceSynthEditor (*this);
}

void TsengoVoiceSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TsengoVoiceSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TsengoVoiceSynthProcessor();
}
