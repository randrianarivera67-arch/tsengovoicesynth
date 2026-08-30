#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <memory>
#include <algorithm>

//==============================================================================
TsengoProcessor::TsengoProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // General-MIDI-ish default drum mapping for the six trainable pads
    static const int defaults[TriggerEngine::kNumPads] = { 36, 38, 42, 46, 49, 39 };
    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
        padNote[i].store (defaults[i]);

    pitch_.prepare (44100.0, MAX_WIN);
    calPitches_.reserve (2048);
    activeExtraNotes_.reserve (8);
    hostMono_.resize (8192, 0.f);
}

TsengoProcessor::~TsengoProcessor()
{
    closeDevice();
    closeMidiOutput();
}

bool TsengoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto& in = layouts.getMainInputChannelSet();
    if (in.isDisabled()) return true;                     // synth slot: no input at all
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

juce::String TsengoProcessor::defaultPadName (int pad)
{
    switch (pad)
    {
        case 0:  return "PAD 1";
        case 1:  return "PAD 2";
        case 2:  return "PAD 3";
        case 3:  return "PAD 4";
        case 4:  return "PAD 5";
        default: return "PAD 6";
    }
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
    s.bufferSize       = 256;
    micMgr_.setAudioDeviceSetup (s, true);

    // The device may not honour our requested rate — analyse at its real one.
    if (auto* dev = micMgr_.getCurrentAudioDevice())
    {
        const double devRate = dev->getCurrentSampleRate();
        if (devRate > 8000.0)
        {
            formant_.setSampleRate (devRate);
            trig_.prepare (devRate);
        }
    }

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
    pitch_.prepare (sr, MAX_WIN);
    ringWrite_ = 0;
    formant_.setSampleRate (sr);
    trig_.prepare (sr);
    trig_.noiseFloor.store (noiseFloor_.load());
    voicePhase_.clear();
    activeExtraNotes_.clear();
    pendingNote_   = -2;
    lastMidiNote_  = -1;
    lastBendValue_ = 8192;

    for (auto& a : trigActive_) { a.note = -1; a.samplesLeft = 0; }
}

void TsengoProcessor::releaseResources()
{
    closeDevice();
}

//==============================================================================
// Mic callback — writes into the ring buffer and feeds the trigger engine
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
        ring_[(size_t) wp] = s;
        wp           = (wp + 1) & (RING - 1);
        lv           = std::max (lv, std::abs (s));
    }

    ringWrite_.store (wp);
    micLevel_.store (lv);

    // Triggers are detected here, at mic-callback resolution, so hit timing
    // doesn't get smeared to the DAW's block size.
    trig_.processAudio (in[0], N);
}

//==============================================================================
// Key / Scale quantize
int TsengoProcessor::quantizeNoteToScale (int note) const
{
    static const int scaleMajor[]   = { 0, 2, 4, 5, 7, 9, 11 };
    static const int scaleMinor[]   = { 0, 2, 3, 5, 7, 8, 10 };
    static const int scaleMajPent[] = { 0, 2, 4, 7, 9 };
    static const int scaleMinPent[] = { 0, 3, 5, 7, 10 };
    static const int scaleDorian[]  = { 0, 2, 3, 5, 7, 9, 10 };

    const int root = ((keyRoot.load() % 12) + 12) % 12;
    const int type = scaleType.load();

    if (type == 0) return note; // Chromatic — no snapping

    const int* scale = scaleMajor;
    int        len   = 7;
    switch (type)
    {
        case 1: scale = scaleMajor;   len = 7; break;
        case 2: scale = scaleMinor;   len = 7; break;
        case 3: scale = scaleMajPent; len = 5; break;
        case 4: scale = scaleMinPent; len = 5; break;
        case 5: scale = scaleDorian;  len = 7; break;
        default: break;
    }

    const int rel = ((note - root) % 12 + 12) % 12;
    int best = scale[0], bestDist = 99;
    for (int i = 0; i < len; ++i)
    {
        int d = std::abs (rel - scale[i]);
        d = std::min (d, 12 - d);
        if (d < bestDist) { bestDist = d; best = scale[i]; }
    }

    const int octaveBase = note - rel;
    return juce::jlimit (0, 127, octaveBase + best);
}

void TsengoProcessor::getChordIntervals (int type, std::vector<int>& intervals) const
{
    switch (type)
    {
        case 0:  intervals = { 0, 4, 7 };     break; // Major
        case 1:  intervals = { 0, 3, 7 };     break; // Minor
        case 2:  intervals = { 0, 5, 7 };     break; // Sus4
        case 3:  intervals = { 0, 4, 7, 11 }; break; // Maj7
        case 4:  intervals = { 0, 3, 7, 10 }; break; // Min7
        default: intervals = { 0, 12 };       break; // Octave
    }
}

//==============================================================================
void TsengoProcessor::allNotesOff (juce::MidiBuffer& midi)
{
    const int ch = juce::jlimit (1, 16, pitchChannel.load());

    if (lastMidiNote_ >= 0)
        midi.addEvent (juce::MidiMessage::noteOff (ch, lastMidiNote_, (juce::uint8) 0), 0);
    for (int n : activeExtraNotes_)
        midi.addEvent (juce::MidiMessage::noteOff (ch, n, (juce::uint8) 0), 0);
    activeExtraNotes_.clear();
    voicePhase_.clear();

    if (lastBendValue_ != 8192)
    {
        midi.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), 0);
        lastBendValue_ = 8192;
    }

    lastMidiNote_ = -1;
    pendingNote_  = -2;
    currentNote_.store (-1);
}

void TsengoProcessor::sendNoteChange (juce::MidiBuffer& midi, int newNote, int velocity, int sampleOffset)
{
    const int ch = juce::jlimit (1, 16, pitchChannel.load());

    if (lastMidiNote_ >= 0)
        midi.addEvent (juce::MidiMessage::noteOff (ch, lastMidiNote_, (juce::uint8) 0), sampleOffset);
    for (int n : activeExtraNotes_)
        midi.addEvent (juce::MidiMessage::noteOff (ch, n, (juce::uint8) 0), sampleOffset);
    activeExtraNotes_.clear();

    if (newNote >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOn (ch, newNote, (juce::uint8) velocity), sampleOffset);

        if (chordsEnabled.load())
        {
            std::vector<int> intervals;
            getChordIntervals (chordType.load(), intervals);
            for (size_t i = 1; i < intervals.size(); ++i)
            {
                int n = juce::jlimit (0, 127, newNote + intervals[i]);
                midi.addEvent (juce::MidiMessage::noteOn (ch, n, (juce::uint8) velocity), sampleOffset);
                activeExtraNotes_.push_back (n);
            }
        }
    }

    // A fresh note starts centred — no stale bend carried over
    midi.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), sampleOffset);
    lastBendValue_ = 8192;
}

//==============================================================================
bool TsengoProcessor::nextGridOffsetInBlock (int blockSize, int& sampleOffset)
{
    auto* ph = getPlayHead();
    if (ph == nullptr) return false;

    auto posInfo = ph->getPosition();
    if (! posInfo.hasValue()) return false;

    auto ppqOpt = posInfo->getPpqPosition();
    auto bpmOpt = posInfo->getBpm();
    if (! ppqOpt.hasValue() || ! bpmOpt.hasValue()) return false;

    const double ppq = *ppqOpt;
    const double bpm = *bpmOpt > 1.0 ? *bpmOpt : 120.0;

    const double div            = (double) juce::jmax (1, timeQuantizeDivision.load());
    const double gridBeats      = 1.0 / div;
    const double secPerBeat     = 60.0 / bpm;
    const double samplesPerBeat = secPerBeat * sampleRate_;

    double beatsToNextGrid = gridBeats - std::fmod (ppq, gridBeats);
    if (beatsToNextGrid >= gridBeats - 1e-9) beatsToNextGrid = 0.0;

    const double samplesToNextGrid = beatsToNextGrid * samplesPerBeat;

    if (samplesToNextGrid < (double) blockSize)
    {
        sampleOffset = juce::jlimit (0, blockSize - 1, (int) std::round (samplesToNextGrid));
        return true;
    }
    return false;
}

//==============================================================================
// Trigger pads — drain whatever the (trained) engine detected on the mic
// thread and turn it into MIDI. Untrained pads never fire.
void TsengoProcessor::serviceTriggers (juce::MidiBuffer& midi, int blockSize)
{
    const int ch = juce::jlimit (1, 16, triggerChannel.load());

    for (auto& a : trigActive_)
    {
        if (a.note >= 0)
        {
            a.samplesLeft -= blockSize;
            if (a.samplesLeft <= 0)
            {
                const int off = juce::jlimit (0, juce::jmax (0, blockSize - 1),
                                              blockSize + a.samplesLeft);
                midi.addEvent (juce::MidiMessage::noteOff (ch, a.note, (juce::uint8) 0), off);
                a.note = -1;
            }
        }
    }

    TriggerEngine::Event e;
    int guard = 0;
    while (trig_.popEvent (e) && ++guard <= 8)
    {
        if (e.pad < 0 || e.pad >= TriggerEngine::kNumPads) continue;

        const int note = juce::jlimit (0, 127, padNote[e.pad].load());
        auto& a = trigActive_[(size_t) e.pad];

        if (a.note >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (ch, a.note, (juce::uint8) 0), 0);
            a.note = -1;
        }

        midi.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) e.velocity), 0);
        a.note        = note;
        a.samplesLeft = (int) (0.06 * sampleRate_);
    }
}

//==============================================================================
// External MIDI output
juce::StringArray TsengoProcessor::getMidiOutputs()
{
    juce::StringArray names;
    for (const auto& d : juce::MidiOutput::getAvailableDevices())
        names.add (d.name);
    return names;
}

void TsengoProcessor::openMidiOutput (const juce::String& name)
{
    std::unique_ptr<juce::MidiOutput> opened;

    for (const auto& d : juce::MidiOutput::getAvailableDevices())
    {
        if (d.name == name)
        {
            opened = juce::MidiOutput::openDevice (d.identifier);
            break;
        }
    }

    if (opened != nullptr)
        opened->startBackgroundThread();

    {
        const juce::SpinLock::ScopedLockType sl (midiOutLock_);
        midiOut_ = std::move (opened);
        midiOutOpen_.store (midiOut_ != nullptr);
    }

    midiOutName_  = midiOutOpen_.load() ? name : juce::String();
    midiOutClock_ = 0.0;
}

void TsengoProcessor::closeMidiOutput()
{
    std::unique_ptr<juce::MidiOutput> old;
    {
        const juce::SpinLock::ScopedLockType sl (midiOutLock_);
        old = std::move (midiOut_);
        midiOutOpen_.store (false);
    }
    if (old != nullptr) old->stopBackgroundThread();
    midiOutName_ = {};
}

// Called on the audio thread — never blocks: if the message thread is busy
// swapping the device we simply skip this block rather than stall.
void TsengoProcessor::forwardToMidiOutput (const juce::MidiBuffer& midi, int blockSize)
{
    if (! midiOutOpen_.load() || midi.isEmpty()) return;

    const juce::SpinLock::ScopedTryLockType sl (midiOutLock_);
    if (! sl.isLocked() || midiOut_ == nullptr) return;

    midiOut_->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes(), sampleRate_);
    juce::ignoreUnused (blockSize);
}

//==============================================================================
// Host audio capture — the effect and standalone builds get the voice from the
// plugin's input bus instead of opening a device of their own. Same ring
// buffer, same analysis, so everything downstream is identical.
void TsengoProcessor::captureHostInput (const juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumInputChannels());
    const int n     = buffer.getNumSamples();
    if (numCh < 1 || n < 1) return;

    if ((int) hostMono_.size() < n) hostMono_.resize ((size_t) n, 0.f);

    const float g = gain.load();
    const float* l = buffer.getReadPointer (0);
    const float* r = numCh > 1 ? buffer.getReadPointer (1) : nullptr;

    int   wp = ringWrite_.load();
    float lv = 0.f;

    for (int i = 0; i < n; ++i)
    {
        const float s = (r != nullptr ? 0.5f * (l[i] + r[i]) : l[i]) * g;
        hostMono_[(size_t) i] = s;
        ring_[(size_t) wp] = s;
        wp = (wp + 1) & (RING - 1);
        lv = std::max (lv, std::abs (s));
    }

    ringWrite_.store (wp);
    micLevel_.store (lv);
    hostLevel_.store (lv);

    trig_.processAudio (hostMono_.data(), n);
}

//==============================================================================
// Calibration wizard
void TsengoProcessor::startCalibration()
{
    calElapsed_ = 0.0;
    calTarget_  = 2.5;
    calAccum_   = 0.f;
    calCount_   = 0;
    calPeak_    = 0.f;
    calPitches_.clear();
    calProgress_.store (0.f);
    calStage_.store (CalAmbient);
}

void TsengoProcessor::cancelCalibration()
{
    calStage_.store (CalIdle);
    calProgress_.store (0.f);
}

void TsengoProcessor::setPitchRange (float lo, float hi)
{
    lo = juce::jlimit (40.f, 800.f, lo);
    hi = juce::jlimit (lo * 1.5f, 2000.f, hi);
    pitchMinHz_.store (lo);
    pitchMaxHz_.store (hi);
}

juce::String TsengoProcessor::getCalInstruction() const
{
    switch (calStage_.load())
    {
        case CalAmbient:  return "1/4  MANGINA — aza miteny mihitsy";
        case CalLevel:    return "2/4  MIHIRA amin'ny feo mahazatra";
        case CalLowNote:  return "3/4  MIHIRA ny NOTA AMBANY indrindra";
        case CalHighNote: return "4/4  MIHIRA ny NOTA AVO indrindra";
        case CalDone:     return "VITA — voaomana ny mikro";
        default:          return "";
    }
}

void TsengoProcessor::serviceCalibration (float level, float hz, float conf, int blockSize)
{
    const int stage = calStage_.load();
    if (stage == CalIdle) return;

    calElapsed_ += (double) blockSize / juce::jmax (1.0, sampleRate_);
    calProgress_.store ((float) juce::jlimit (0.0, 1.0, calElapsed_ / juce::jmax (0.1, calTarget_)));

    switch (stage)
    {
        case CalAmbient:
            calAccum_ += level;
            ++calCount_;
            calPeak_ = juce::jmax (calPeak_, level);
            break;

        case CalLevel:
            calPeak_ = juce::jmax (calPeak_, level);
            break;

        case CalLowNote:
        case CalHighNote:
            if (hz > 45.f && hz < 2000.f && conf > 0.35f
                && calPitches_.size() < calPitches_.capacity())
                calPitches_.push_back (hz);
            break;

        default: break;
    }

    if (calElapsed_ >= calTarget_)
        advanceCalStage();
}

void TsengoProcessor::advanceCalStage()
{
    const int stage = calStage_.load();

    auto percentile = [this] (float p) -> float
    {
        if (calPitches_.empty()) return -1.f;
        std::sort (calPitches_.begin(), calPitches_.end());
        const int idx = juce::jlimit (0, (int) calPitches_.size() - 1,
                                      (int) ((float) (calPitches_.size() - 1) * p));
        return calPitches_[(size_t) idx];
    };

    int nextStage = CalIdle;
    double nextTarget = 2.5;

    switch (stage)
    {
        case CalAmbient:
        {
            const float nf = calCount_ > 0 ? calAccum_ / (float) calCount_ : 0.01f;
            noiseFloor_.store (juce::jlimit (0.0005f, 0.3f, nf));
            threshold.store  (juce::jlimit (0.015f, 0.5f, nf * 3.f + 0.015f));
            trig_.noiseFloor.store (noiseFloor_.load());
            nextStage  = CalLevel;
            nextTarget = 3.0;
            break;
        }

        case CalLevel:
        {
            if (calPeak_ > 0.02f)
            {
                const float g = gain.load() * (0.70f / calPeak_);
                gain.store (juce::jlimit (0.5f, 8.f, g));
            }
            nextStage  = CalLowNote;
            nextTarget = 3.5;
            break;
        }

        case CalLowNote:
        {
            const float lo = percentile (0.10f);
            if (lo > 45.f)
                pitchMinHz_.store (juce::jlimit (45.f, 600.f, lo * 0.92f));
            nextStage  = CalHighNote;
            nextTarget = 3.5;
            break;
        }

        case CalHighNote:
        {
            const float hi = percentile (0.90f);
            if (hi > 60.f)
                pitchMaxHz_.store (juce::jlimit (pitchMinHz_.load() * 1.5f, 2000.f, hi * 1.08f));
            nextStage  = CalDone;
            nextTarget = 1.5;
            break;
        }

        default:
            nextStage = CalIdle;
            break;
    }

    calElapsed_ = 0.0;
    calTarget_  = nextTarget;
    calAccum_   = 0.f;
    calCount_   = 0;
    calPeak_    = 0.f;
    calPitches_.clear();
    calProgress_.store (0.f);
    calStage_.store (nextStage);
}

//==============================================================================
void TsengoProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int blockSize = buffer.getNumSamples();

    // ---- input routing -------------------------------------------------
    // The internal microphone device always wins when the user has explicitly
    // connected one; otherwise we take whatever the host feeds our input bus.
    const int  src      = inputSource.load();
    const bool hasHostIn = getTotalNumInputChannels() > 0;
    const bool useHost  = (src == SourceHost) ? hasHostIn
                        : (src == SourceMic)  ? false
                                              : (hasHostIn && ! deviceOpen_);
    usingHost_.store (useHost);

    if (useHost)
        captureHostInput (buffer);     // must happen before we clear the buffer

    buffer.clear();

    // Analysis window size — the "latency" trade-off (short = snappy,
    // long = more accurate on low notes).
    static const int winSizes[3] = { 1024, 2048, 4096 };
    const int N  = winSizes[juce::jlimit (0, 2, latencyMode.load())];
    const int wp = ringWrite_.load();

    const float lv = micLevel_.load();

    // Triggers are serviced first so percussive hits still fire during the
    // gaps between sung notes.
    serviceTriggers (midi, blockSize);

    // Fill analysis window
    for (int i = 0; i < N; ++i)
        yinBuf_[i] = ring_[(size_t) ((wp - N + i + RING) & (RING - 1))];

    const int   calStage = calStage_.load();
    const bool  calibrating = (calStage != CalIdle);
    const bool  gateOpen = lv >= threshold.load() * 0.5f;

    float rawHz = -1.f;
    if (gateOpen || calibrating)
    {
        // While hunting for the singer's range, search wide open.
        const bool  wide = (calStage == CalLowNote || calStage == CalHighNote);
        const float fLo  = wide ? 55.f   : pitchMinHz_.load();
        const float fHi  = wide ? 1600.f : pitchMaxHz_.load();

        pitch_.setRange (fLo, fHi);
        const auto res = pitch_.analyse (yinBuf_, N);
        rawHz = res.hz;
        confidence_.store (res.confidence);
    }

    if (calibrating)
        serviceCalibration (lv, rawHz, confidence_.load(), blockSize);

    if (! gateOpen)
    {
        // Silence — release everything cleanly (root + chord notes + bend)
        allNotesOff (midi);
        midiLevel_.store (0.f);
        smAaa_ *= 0.9f; smEee_ *= 0.9f; smOoo_ *= 0.9f; smEnv_ *= 0.9f;
        vowelAAA_.store (smAaa_);
        vowelEEE_.store (smEee_);
        vowelOOO_.store (smOoo_);
        vowelEnv_.store (smEnv_);
        forwardToMidiOutput (midi, blockSize);
        return;
    }

    // Vowel / formant / envelope analysis — reuses the same analysis window
    // as pitch tracking. Produces four continuous macros (aaa/eee/ooo/env),
    // sent out as assignable MIDI CCs so they can automate FX/parameters in
    // the DAW — the same role Dubler 2's four vowel/env pads play.
    {
        const int ccCh = juce::jlimit (1, 16, pitchChannel.load());

        auto sendCC = [&] (int cc, float v, int& last)
        {
            int v127 = juce::jlimit (0, 127, juce::roundToInt (v * 127.f));
            if (std::abs (v127 - last) >= 2)
            {
                midi.addEvent (juce::MidiMessage::controllerEvent (ccCh, cc, v127), 0);
                last = v127;
            }
        };

        const float sm = smoothing.load();

        // Envelope — overall vocal loudness, independent of vowel shape.
        smEnv_ += (juce::jlimit (0.f, 1.f, lv * 3.f) - smEnv_) * (1.f - sm);
        vowelEnv_.store (smEnv_);
        sendCC (ccEnv.load(), smEnv_, lastCcEnv_);

        auto fRes = formant_.analyse (yinBuf_, N);
        if (fRes.confidence > 0.05f)
        {
            smAaa_ += (fRes.aaa - smAaa_) * (1.f - sm);
            smEee_ += (fRes.eee - smEee_) * (1.f - sm);
            smOoo_ += (fRes.ooo - smOoo_) * (1.f - sm);

            f1_.store (fRes.f1);
            f2_.store (fRes.f2);
            vowelAAA_.store (smAaa_);
            vowelEEE_.store (smEee_);
            vowelOOO_.store (smOoo_);

            sendCC (ccAaa.load(), smAaa_, lastCcAaa_);
            sendCC (ccEee.load(), smEee_, lastCcEee_);
            sendCC (ccOoo.load(), smOoo_, lastCcOoo_);
        }
    }

    // Median filter — stabilise pitch (allocation-free, see PitchTracker)
    const float medHz = pitch_.medianFiltered (rawHz);
    currentHz_.store (medHz);

    // Hz → chromatic MIDI note (unquantized — kept for pitch-bend maths)
    int   chromaticNote = -1;
    float rawSemis      = 0.f;
    if (medHz > 20.f && medHz < 5000.f)
    {
        rawSemis      = 69.f + 12.f * std::log2 (medHz / 440.f);
        chromaticNote = juce::jlimit (0, 127, juce::roundToInt (rawSemis));
    }

    // Key / Scale quantize — snaps the note actually sent to the DAW
    int note = chromaticNote;
    if (note >= 0 && quantizeToKey.load())
        note = quantizeNoteToScale (note);

    // Octave shift
    if (note >= 0)
        note = juce::jlimit (0, 127, note + octaveShift.load() * 12);

    // Stickiness — hold N frames before accepting a new note, avoids flicker
    if (note == lastMidiNote_)
    {
        noteHoldCount_ = 0;
    }
    else
    {
        ++noteHoldCount_;
        if (noteHoldCount_ < juce::jmax (1, stickiness.load()))
            note = lastMidiNote_;
    }

    // Velocity from mic level (already gain-scaled), not a fixed value
    const int velocity = juce::jlimit (1, 127, juce::roundToInt (juce::jmap (
        juce::jlimit (0.f, 1.f, lv * 4.f), 0.f, 1.f, 20.f, 127.f)));

    // Track the latest desired note; time-quantize (if on) delays *when*
    // this is actually sent, not *what* is sent.
    if (note != lastMidiNote_)
        pendingNote_ = note;

    if (pendingNote_ != -2 && pendingNote_ != lastMidiNote_)
    {
        int  offset = 0;
        bool fire   = true;
        if (timeQuantizeEnabled.load())
            fire = nextGridOffsetInBlock (blockSize, offset);

        if (fire)
        {
            sendNoteChange (midi, pendingNote_, velocity, offset);
            lastMidiNote_ = pendingNote_;
            currentNote_.store (pendingNote_);
            noteHoldCount_ = 0;
        }
    }

    // Pitch bend — carries the singer's fine pitch/vibrato under whichever
    // note actually sounds (quantized or not), so expression isn't lost.
    if (pitchBendEnabled.load() && lastMidiNote_ >= 0 && chromaticNote >= 0)
    {
        float bendSemis = rawSemis - (float) chromaticNote;
        float range     = (float) juce::jmax (1, pitchBendRangeSemitones.load());
        float norm      = juce::jlimit (-1.f, 1.f, bendSemis / range);
        int   val14     = juce::jlimit (0, 16383, 8192 + juce::roundToInt (norm * 8191.f));

        if (std::abs (val14 - lastBendValue_) >= 32)
        {
            midi.addEvent (juce::MidiMessage::pitchWheel (
                juce::jlimit (1, 16, pitchChannel.load()), val14), 0);
            lastBendValue_ = val14;
        }
    }

    midiLevel_.store (lastMidiNote_ >= 0 ? 1.f : 0.f);

    // Monitor synth — simple built-in tone (sine) for the root + any chord
    // notes currently sounding, so there's audible feedback while singing.
    if (monitorSynthEnabled.load())
    {
        std::vector<int> active;
        if (lastMidiNote_ >= 0) active.push_back (lastMidiNote_);
        for (int n : activeExtraNotes_) active.push_back (n);

        for (auto it = voicePhase_.begin(); it != voicePhase_.end(); )
        {
            if (std::find (active.begin(), active.end(), it->first) == active.end())
                it = voicePhase_.erase (it);
            else
                ++it;
        }
        for (int n : active)
            voicePhase_.emplace (n, 0.0);

        if (! active.empty())
        {
            const float vol      = monitorVolume.load();
            const float perVoice = vol / (float) active.size();
            auto*       L        = buffer.getWritePointer (0);
            auto*       R        = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

            for (int i = 0; i < blockSize; ++i)
            {
                float s = 0.f;
                for (auto& entry : voicePhase_)
                {
                    const int    n    = entry.first;
                    double&      ph   = entry.second;
                    const double freq = 440.0 * std::pow (2.0, (n - 69) / 12.0);

                    s += (float) std::sin (ph) * perVoice;
                    ph += juce::MathConstants<double>::twoPi * freq / sampleRate_;
                    if (ph > juce::MathConstants<double>::twoPi)
                        ph -= juce::MathConstants<double>::twoPi;
                }
                L[i] += s;
                if (R) R[i] += s;
            }
        }
    }

    forwardToMidiOutput (midi, blockSize);
}

//==============================================================================
// State — every control plus the trigger training data is saved with the
// project, so a trained kit survives closing FL Studio.
void TsengoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("TSENGO_VOICE_SYNTH");
    xml.setAttribute ("version", 4);

    xml.setAttribute ("threshold",   (double) threshold.load());
    xml.setAttribute ("smoothing",   (double) smoothing.load());
    xml.setAttribute ("gain",        (double) gain.load());
    xml.setAttribute ("latencyMode", latencyMode.load());

    xml.setAttribute ("keyRoot",       keyRoot.load());
    xml.setAttribute ("scaleType",     scaleType.load());
    xml.setAttribute ("quantizeToKey", quantizeToKey.load() ? 1 : 0);

    xml.setAttribute ("chordsEnabled", chordsEnabled.load() ? 1 : 0);
    xml.setAttribute ("chordType",     chordType.load());

    xml.setAttribute ("bendOn",    pitchBendEnabled.load() ? 1 : 0);
    xml.setAttribute ("bendRange", pitchBendRangeSemitones.load());

    xml.setAttribute ("timeQOn",  timeQuantizeEnabled.load() ? 1 : 0);
    xml.setAttribute ("timeQDiv", timeQuantizeDivision.load());

    xml.setAttribute ("monitorOn",  monitorSynthEnabled.load() ? 1 : 0);
    xml.setAttribute ("monitorVol", (double) monitorVolume.load());

    xml.setAttribute ("octaveShift", octaveShift.load());
    xml.setAttribute ("stickiness",  stickiness.load());

    xml.setAttribute ("ccAaa", ccAaa.load());
    xml.setAttribute ("ccEee", ccEee.load());
    xml.setAttribute ("ccOoo", ccOoo.load());
    xml.setAttribute ("ccEnv", ccEnv.load());

    xml.setAttribute ("pitchChannel",   pitchChannel.load());
    xml.setAttribute ("triggerChannel", triggerChannel.load());

    xml.setAttribute ("triggersOn",  trig_.enabled.load() ? 1 : 0);
    xml.setAttribute ("trigSens",    (double) trig_.sensitivity.load());
    xml.setAttribute ("trigStrict",  (double) trig_.strictness.load());

    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
        xml.setAttribute ("padNote" + juce::String (i), padNote[i].load());

    xml.setAttribute ("noiseFloor", (double) noiseFloor_.load());
    xml.setAttribute ("pitchMin",   (double) pitchMinHz_.load());
    xml.setAttribute ("pitchMax",   (double) pitchMaxHz_.load());
    xml.setAttribute ("device",      currentDevice_);
    xml.setAttribute ("inputSource", inputSource.load());
    xml.setAttribute ("midiOut",     midiOutName_);

    xml.setAttribute ("training", trig_.saveToString());

    copyXmlToBinary (xml, destData);
}

void TsengoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName ("TSENGO_VOICE_SYNTH")) return;

    threshold.store   ((float) xml->getDoubleAttribute ("threshold", threshold.load()));
    smoothing.store   ((float) xml->getDoubleAttribute ("smoothing", smoothing.load()));
    gain.store        ((float) xml->getDoubleAttribute ("gain",      gain.load()));
    latencyMode.store (xml->getIntAttribute ("latencyMode", latencyMode.load()));

    keyRoot.store       (xml->getIntAttribute ("keyRoot",   keyRoot.load()));
    scaleType.store     (xml->getIntAttribute ("scaleType", scaleType.load()));
    quantizeToKey.store (xml->getIntAttribute ("quantizeToKey", 0) != 0);

    chordsEnabled.store (xml->getIntAttribute ("chordsEnabled", 0) != 0);
    chordType.store     (xml->getIntAttribute ("chordType", chordType.load()));

    pitchBendEnabled.store        (xml->getIntAttribute ("bendOn", 1) != 0);
    pitchBendRangeSemitones.store (xml->getIntAttribute ("bendRange", pitchBendRangeSemitones.load()));

    timeQuantizeEnabled.store  (xml->getIntAttribute ("timeQOn", 0) != 0);
    timeQuantizeDivision.store (xml->getIntAttribute ("timeQDiv", timeQuantizeDivision.load()));

    monitorSynthEnabled.store (xml->getIntAttribute ("monitorOn", 0) != 0);
    monitorVolume.store       ((float) xml->getDoubleAttribute ("monitorVol", monitorVolume.load()));

    octaveShift.store (xml->getIntAttribute ("octaveShift", octaveShift.load()));
    stickiness.store  (xml->getIntAttribute ("stickiness",  stickiness.load()));

    ccAaa.store (xml->getIntAttribute ("ccAaa", ccAaa.load()));
    ccEee.store (xml->getIntAttribute ("ccEee", ccEee.load()));
    ccOoo.store (xml->getIntAttribute ("ccOoo", ccOoo.load()));
    ccEnv.store (xml->getIntAttribute ("ccEnv", ccEnv.load()));

    pitchChannel.store   (xml->getIntAttribute ("pitchChannel",   pitchChannel.load()));
    triggerChannel.store (xml->getIntAttribute ("triggerChannel", triggerChannel.load()));

    trig_.enabled.store     (xml->getIntAttribute ("triggersOn", 0) != 0);
    trig_.sensitivity.store ((float) xml->getDoubleAttribute ("trigSens",   trig_.sensitivity.load()));
    trig_.strictness.store  ((float) xml->getDoubleAttribute ("trigStrict", trig_.strictness.load()));

    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
        padNote[i].store (juce::jlimit (0, 127,
            xml->getIntAttribute ("padNote" + juce::String (i), padNote[i].load())));

    noiseFloor_.store ((float) xml->getDoubleAttribute ("noiseFloor", noiseFloor_.load()));
    pitchMinHz_.store ((float) xml->getDoubleAttribute ("pitchMin", pitchMinHz_.load()));
    pitchMaxHz_.store ((float) xml->getDoubleAttribute ("pitchMax", pitchMaxHz_.load()));
    trig_.noiseFloor.store (noiseFloor_.load());

    currentDevice_ = xml->getStringAttribute ("device", currentDevice_);
    inputSource.store (juce::jlimit (0, 2, xml->getIntAttribute ("inputSource", inputSource.load())));
    midiOutName_   = xml->getStringAttribute ("midiOut", midiOutName_);

    trig_.loadFromString (xml->getStringAttribute ("training", juce::String()));

    stateRevision.fetch_add (1);
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
