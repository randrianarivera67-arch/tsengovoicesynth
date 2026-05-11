#include "PluginEditor.h"

using juce::Colour;
using juce::Rectangle;
using juce::Graphics;
using juce::String;

static constexpr uint32_t kCyan    = 0xFF00D4FF;
static constexpr uint32_t kBg      = 0xFF05080F;
static constexpr uint32_t kBgMid   = 0xFF090E1C;
static constexpr uint32_t kText    = 0xFFC8EEFF;
static constexpr uint32_t kGreen   = 0xFF00E676;
static constexpr uint32_t kOrange  = 0xFFFF6D00;

// ═════════════════════════════════════════════════════════════════════════════
// CrystalPanel
// ═════════════════════════════════════════════════════════════════════════════
CrystalPanel::CrystalPanel (const String& title) : title_ (title) {}

void CrystalPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (kBgMid));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (Colour (kCyan).withAlpha (0.13f));
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 0.8f);

    if (title_.isNotEmpty())
    {
        g.setColour (Colour (kCyan).withAlpha (0.35f));
        g.setFont (juce::Font ("Courier New", 8.0f, juce::Font::plain));
        g.drawText (title_, 10, 6, getWidth() - 12, 10,
                    juce::Justification::left);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// WaveformDisplay
// ═════════════════════════════════════════════════════════════════════════════
WaveformDisplay::WaveformDisplay() { startTimerHz (30); }

void WaveformDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (kBg));
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (Colour (kCyan).withAlpha (0.08f));
    g.drawRoundedRectangle (b.reduced (0.4f), 3.0f, 0.7f);

    // Grid
    g.setColour (Colour (kCyan).withAlpha (0.05f));
    const float cy = b.getCentreY();
    g.drawHorizontalLine ((int)cy, b.getX(), b.getRight());

    // Animate waveform
    const float target = level_.load() * 0.9f;
    waveAmp_ += (target - waveAmp_) * 0.08f;
    phase_ += 0.09f;

    const float W = b.getWidth(), H = b.getHeight();
    const float fr = frequency_.load();
    const float freqMul = fr > 0 ? (fr / 220.0f) * 2.5f : 1.0f;

    juce::Path wave;
    bool first = true;
    for (float i = 0; i < W; i += 1.0f)
    {
        const float s = std::sin (phase_ + i * 0.1f * freqMul) * 0.7f
                      + std::sin (phase_ * 2.1f + i * 0.05f) * 0.2f;
        const float y = cy - s * waveAmp_ * (H * 0.4f);
        if (first) { wave.startNewSubPath (b.getX() + i, y); first = false; }
        else        wave.lineTo (b.getX() + i, y);
    }
    g.setColour (Colour (kCyan).withAlpha (waveAmp_ > 0.02f ? 0.85f : 0.3f));
    g.strokePath (wave, juce::PathStrokeType (1.5f));
}

// ═════════════════════════════════════════════════════════════════════════════
// LevelMeter
// ═════════════════════════════════════════════════════════════════════════════
LevelMeter::LevelMeter (Colour c, const String& label)
    : meterColour_ (c), label_ (label) {}

void LevelMeter::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto track = b.removeFromBottom (b.getHeight() - 14.0f);
    // Label
    g.setColour (Colour (kCyan).withAlpha (0.3f));
    g.setFont (juce::Font ("Courier New", 8.0f, juce::Font::plain));
    g.drawText (label_, b.toNearestInt(), juce::Justification::centred);

    // Track bg
    g.setColour (Colour (kCyan).withAlpha (0.06f));
    g.fillRoundedRectangle (track, 2.0f);

    // Fill
    const float fillH = track.getHeight() * value_;
    auto fill = track.removeFromBottom (fillH);
    const juce::Colour col = meterColour_ == LevelMeter::Colour::Green
                             ? juce::Colour (kGreen) : juce::Colour (kOrange);
    g.setColour (col.withAlpha (0.85f));
    g.fillRoundedRectangle (fill, 2.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
// PianoStrip
// ═════════════════════════════════════════════════════════════════════════════
static const bool kIsBlack[12] = {false,true,false,true,false,false,true,false,true,false,true,false};
static const int  kWOffset[12] = {0,-1,1,-1,2,3,-1,4,-1,5,-1,6}; // white index offset

PianoStrip::PianoStrip()
{
    setInterceptsMouseClicks (false, false);
}

void PianoStrip::buildKeys (int totalWidth)
{
    keys_.clear();
    int whiteCount = 0;
    for (int m = kFirst; m <= kLast; ++m)
        if (!kIsBlack[m % 12]) ++whiteCount;

    const float ww = static_cast<float> (totalWidth) / whiteCount;
    int wIdx = 0;
    for (int m = kFirst; m <= kLast; ++m)
    {
        if (!kIsBlack[m % 12])
        {
            keys_.push_back ({ m, false, wIdx * ww, ww - 0.5f, (float)getHeight() });
            ++wIdx;
        }
    }
    wIdx = 0;
    for (int m = kFirst; m <= kLast; ++m)
    {
        if (!kIsBlack[m % 12]) { ++wIdx; continue; }
        // Place black key between the previous white and the next
        const float bx = (wIdx - 1) * ww + ww * 0.62f;
        keys_.push_back ({ m, true, bx, ww * 0.58f, (float)getHeight() * 0.6f });
    }
}

void PianoStrip::paint (Graphics& g)
{
    if (keys_.empty()) buildKeys (getWidth());

    g.setColour (Colour (kBg));
    g.fillAll();

    // White keys
    for (auto& k : keys_)
    {
        if (k.black) continue;
        const bool active = k.midi == activeNote_;
        g.setColour (active ? Colour (kCyan) : Colour (0xFFDDEEFF));
        g.fillRect (k.x, 0.0f, k.w, k.h);
        g.setColour (Colour (kBg).withAlpha (0.5f));
        g.drawRect (k.x, 0.0f, k.w, k.h, 0.5f);

        if (k.midi % 12 == 0)
        {
            g.setColour (active ? Colour (kBg) : Colour (0xFF888888));
            g.setFont (juce::Font ("Courier New", 7.0f, juce::Font::plain));
            g.drawText ("C" + String (k.midi / 12 - 1),
                        (int)k.x, (int)k.h - 12, (int)k.w, 10,
                        juce::Justification::centred);
        }
    }
    // Black keys
    for (auto& k : keys_)
    {
        if (!k.black) continue;
        const bool active = k.midi == activeNote_;
        g.setColour (active ? Colour (0xFF0055CC) : Colour (kBgMid));
        g.fillRect (k.x, 0.0f, k.w, k.h);
        if (active)
        {
            g.setColour (Colour (kCyan).withAlpha (0.7f));
            g.drawRect (k.x, 0.0f, k.w, k.h, 0.8f);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Editor
// ═════════════════════════════════════════════════════════════════════════════
const String TsengoVoiceSynthEditor::noteNames[12]
    = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

String TsengoVoiceSynthEditor::midiNoteName (int note)
{
    if (note < 0) return "—";
    return noteNames[note % 12] + String (note / 12 - 1) + " (" + String (note) + ")";
}

// ─────────────────────────────────────────────────────────────────────────────
TsengoVoiceSynthEditor::TsengoVoiceSynthEditor (TsengoVoiceSynthProcessor& p)
    : AudioProcessorEditor (&p),
      proc_ (p),
      leftPanel_   ("MICROPHONE INPUT"),
      centerPanel_ ("PARAMETERS"),
      rightPanel_  ("MIDI OUTPUT")
{
    setLookAndFeel (&laf_);
    setSize (620, 420);

    // ── Header labels ──────────────────────────────────────────────────────
    titleLabel_.setText ("TSENGO  VOICE  SYNTH", juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, Colour (kCyan));
    titleLabel_.setFont (juce::Font ("Courier New", 13.0f, juce::Font::plain).boldened());
    addAndMakeVisible (titleLabel_);

    versionLabel_.setText ("MIC \xe2\x86\x92 MIDI  \xc2\xb7  VST3  \xc2\xb7  v2.0",
                           juce::dontSendNotification);
    versionLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.3f));
    versionLabel_.setFont (juce::Font ("Courier New", 8.0f, juce::Font::plain));
    versionLabel_.setJustificationType (juce::Justification::right);
    addAndMakeVisible (versionLabel_);

    // ── Left panel ─────────────────────────────────────────────────────────
    addAndMakeVisible (leftPanel_);

    micLabel_.setText ("PÉRIPHÉRIQUE", juce::dontSendNotification);
    addAndMakeVisible (micLabel_);

    deviceCombo_.addItem ("Microphone intégré", 1);
    deviceCombo_.addItem ("USB Audio Device",   2);
    deviceCombo_.addItem ("Focusrite Scarlett",  3);
    deviceCombo_.addItem ("Rode NT-USB",         4);
    deviceCombo_.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (deviceCombo_);

    addAndMakeVisible (waveform_);

    statusLabel_.setText ("\xe2\x97\x89  ÉCOUTE ACTIVE", juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::textColourId, Colour (kGreen).withAlpha (0.8f));
    statusLabel_.setFont (juce::Font ("Courier New", 8.0f, juce::Font::plain));
    addAndMakeVisible (statusLabel_);

    noteLabel_.setText ("—", juce::dontSendNotification);
    noteLabel_.setColour (juce::Label::textColourId, Colour (kCyan));
    noteLabel_.setFont (juce::Font ("Courier New", 30.0f, juce::Font::plain).boldened());
    noteLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (noteLabel_);

    freqLabel_.setText ("— Hz", juce::dontSendNotification);
    freqLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.5f));
    freqLabel_.setFont (juce::Font ("Courier New", 9.0f, juce::Font::plain));
    freqLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (freqLabel_);

    confLabel_.setText ("YIN CONFIDENCE", juce::dontSendNotification);
    addAndMakeVisible (confLabel_);

    confValLabel_.setText ("0%", juce::dontSendNotification);
    confValLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.6f));
    confValLabel_.setFont (juce::Font ("Courier New", 8.0f, juce::Font::plain));
    confValLabel_.setJustificationType (juce::Justification::right);
    addAndMakeVisible (confValLabel_);

    // ── Center panel — sliders ──────────────────────────────────────────────
    addAndMakeVisible (centerPanel_);

    auto configKnob = [&] (juce::Slider& s, const String& paramId,
                           juce::Label& lbl, const String& name,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (s);
        lbl.setText (name, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                  (proc_.apvts, paramId, s);
    };

    configKnob (threshSlider_, "threshold", threshLbl_,  "THRESHOLD", threshAtt_);
    configKnob (volumeSlider_, "volume",    volumeLbl_,  "VOLUME",    volumeAtt_);
    configKnob (attackSlider_, "attack",    attackLbl_,  "ATTACK",    attackAtt_);
    configKnob (releaseSlider_,"release",   releaseLbl_, "RELEASE",   releaseAtt_);

    // ── Right panel — MIDI display ──────────────────────────────────────────
    addAndMakeVisible (rightPanel_);

    midiNoteLabel_.setText ("—", juce::dontSendNotification);
    midiNoteLabel_.setColour (juce::Label::textColourId, Colour (kCyan));
    midiNoteLabel_.setFont (juce::Font ("Courier New", 20.0f, juce::Font::plain).boldened());
    midiNoteLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (midiNoteLabel_);

    midiNumLabel_.setText ("MIDI CH 1", juce::dontSendNotification);
    midiNumLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.35f));
    midiNumLabel_.setFont (juce::Font ("Courier New", 8.0f, juce::Font::plain));
    midiNumLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (midiNumLabel_);

    midiChLabel_.setText ("— NOTE", juce::dontSendNotification);
    addAndMakeVisible (midiChLabel_);

    // ── Meters ────────────────────────────────────────────────────────────
    addAndMakeVisible (inMeter_);
    addAndMakeVisible (outMeter_);

    // ── Piano ─────────────────────────────────────────────────────────────
    addAndMakeVisible (piano_);

    startTimerHz (30);
}

TsengoVoiceSynthEditor::~TsengoVoiceSynthEditor()
{
    setLookAndFeel (nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthEditor::resized()
{
    auto area = getLocalBounds();

    // Header bar
    auto header = area.removeFromTop (44);
    titleLabel_.setBounds  (header.removeFromLeft (280).reduced (10, 12));
    versionLabel_.setBounds (header.reduced (10, 14));

    // Piano at bottom
    auto pianoArea = area.removeFromBottom (64);
    piano_.setBounds (pianoArea);

    // Body — 3 columns + meter strip on the right
    auto body = area.reduced (6, 4);
    auto meterStrip = body.removeFromRight (44);

    // Meters
    const int mY = meterStrip.getY() + 4;
    const int mH = meterStrip.getHeight() - 8;
    inMeter_ .setBounds (meterStrip.getX() + 4,  mY, 16, mH);
    outMeter_.setBounds (meterStrip.getX() + 24, mY, 16, mH);

    // Left panel ~180px
    auto leftArea = body.removeFromLeft (180);
    leftPanel_.setBounds (leftArea);
    auto lInner = leftArea.reduced (8, 18);
    micLabel_   .setBounds (lInner.removeFromTop (10));
    deviceCombo_.setBounds (lInner.removeFromTop (24).reduced (0, 2));
    lInner.removeFromTop (4);
    waveform_   .setBounds (lInner.removeFromTop (52));
    lInner.removeFromTop (3);
    statusLabel_.setBounds (lInner.removeFromTop (12));
    lInner.removeFromTop (4);
    noteLabel_  .setBounds (lInner.removeFromTop (38));
    freqLabel_  .setBounds (lInner.removeFromTop (14));
    lInner.removeFromTop (4);
    auto confRow = lInner.removeFromTop (10);
    confLabel_   .setBounds (confRow.removeFromLeft (100));
    confValLabel_.setBounds (confRow);

    body.removeFromLeft (6);

    // Right panel ~140px
    auto rightArea = body.removeFromRight (140);
    rightPanel_.setBounds (rightArea);
    auto rInner = rightArea.reduced (8, 18);
    rInner.removeFromTop (10);
    midiChLabel_  .setBounds (rInner.removeFromTop (10));
    midiNoteLabel_.setBounds (rInner.removeFromTop (44));
    midiNumLabel_ .setBounds (rInner.removeFromTop (12));

    body.removeFromRight (6);

    // Center panel — knobs
    centerPanel_.setBounds (body);
    auto cInner = body.reduced (8, 18);
    const int kw = cInner.getWidth() / 2;
    const int kh = cInner.getHeight() / 2;
    auto row1 = cInner.removeFromTop (kh);
    auto row2 = cInner;

    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, Rectangle<int> cell)
    {
        l.setBounds (cell.removeFromBottom (14));
        s.setBounds (cell.reduced (4));
    };
    placeKnob (threshSlider_, threshLbl_,  row1.removeFromLeft (kw));
    placeKnob (volumeSlider_, volumeLbl_,  row1);
    placeKnob (attackSlider_, attackLbl_,  row2.removeFromLeft (kw));
    placeKnob (releaseSlider_,releaseLbl_, row2);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthEditor::paint (Graphics& g)
{
    g.fillAll (Colour (kBg));

    // Header gradient
    juce::ColourGradient grad (Colour (kBgMid), 0, 0,
                               Colour (kBg),    0, 44, false);
    g.setGradientFill (grad);
    g.fillRect (0, 0, getWidth(), 44);

    // Header border
    g.setColour (Colour (kCyan).withAlpha (0.12f));
    g.drawHorizontalLine (43, 0, (float)getWidth());

    // Blinking header dot
    const float a = 0.5f + 0.5f * std::sin (blinkPhase_);
    g.setColour (Colour (kCyan).withAlpha (a));
    g.fillEllipse (8.0f, 18.0f, 7.0f, 7.0f);

    // Confidence bar (manual, inside left panel area)
    const auto det = proc_.getDetectionState();
    const float conf = det.confidence;
    const auto leftBounds = leftPanel_.getBounds();
    const int barY = leftBounds.getBottom() - 20;
    const int barX = leftBounds.getX() + 16;
    const int barW = leftBounds.getWidth() - 32;
    g.setColour (Colour (kCyan).withAlpha (0.07f));
    g.fillRoundedRectangle ((float)barX, (float)barY, (float)barW, 2.0f, 1.0f);
    g.setColour (Colour (kCyan).withAlpha (0.7f));
    g.fillRoundedRectangle ((float)barX, (float)barY, barW * conf, 2.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthEditor::timerCallback()
{
    blinkPhase_ += 0.12f;
    if (blinkPhase_ > juce::MathConstants<float>::twoPi)
        blinkPhase_ -= juce::MathConstants<float>::twoPi;

    const auto state = proc_.getDetectionState();

    waveform_.setLevel (state.inputLevel);
    waveform_.setFrequency (state.frequency);

    // Update labels
    if (state.midiNote >= 0)
    {
        noteLabel_.setText  (noteNames[state.midiNote % 12]
                             + String (state.midiNote / 12 - 1),
                             juce::dontSendNotification);
        freqLabel_.setText  (String (state.frequency, 1) + " Hz",
                             juce::dontSendNotification);
        midiNoteLabel_.setText (noteNames[state.midiNote % 12]
                                + String (state.midiNote / 12 - 1),
                                juce::dontSendNotification);
        midiChLabel_.setText (String (state.midiNote) + "  \xe2\x80\x94  MIDI NOTE",
                              juce::dontSendNotification);
    }
    else
    {
        noteLabel_.setText     ("—", juce::dontSendNotification);
        freqLabel_.setText     ("— Hz", juce::dontSendNotification);
        midiNoteLabel_.setText ("—", juce::dontSendNotification);
        midiChLabel_.setText   ("— NOTE", juce::dontSendNotification);
    }

    confValLabel_.setText (String (juce::roundToInt (state.confidence * 100)) + "%",
                           juce::dontSendNotification);

    inMeter_ .setValue (state.inputLevel);
    outMeter_.setValue (state.midiNote >= 0 ? state.inputLevel * 0.85f : 0.0f);

    piano_.setActiveNote (state.midiNote);

    repaint();
}
