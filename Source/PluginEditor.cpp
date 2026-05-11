#include "PluginEditor.h"

static constexpr uint32_t kCyan   = 0xFF00D4FF;
static constexpr uint32_t kBg     = 0xFF05080F;
static constexpr uint32_t kBgMid  = 0xFF090E1C;
static constexpr uint32_t kText   = 0xFFC8EEFF;
static constexpr uint32_t kGreen  = 0xFF00E676;
static constexpr uint32_t kOrange = 0xFFFF6D00;
static constexpr uint32_t kRed    = 0xFFFF3B3B;

using juce::Colour;

// ═════════════════════════════════════════════════════════════════════════════
CrystalPanel::CrystalPanel (const juce::String& title) : title_ (title) {}

void CrystalPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (kBgMid));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (Colour (kCyan).withAlpha (0.13f));
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 0.8f);
    if (title_.isNotEmpty())
    {
        g.setColour (Colour (kCyan).withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions().withName ("Courier New").withHeight (8.0f)));
        g.drawText (title_, 10, 6, getWidth() - 12, 10, juce::Justification::left);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
WaveformDisplay::WaveformDisplay() { startTimerHz (30); }

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (kBg));
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (Colour (kCyan).withAlpha (0.08f));
    g.drawRoundedRectangle (b.reduced (0.4f), 3.0f, 0.7f);

    const float cy = b.getCentreY();
    g.setColour (Colour (kCyan).withAlpha (0.08f));
    g.drawHorizontalLine ((int)cy, b.getX(), b.getRight());

    const float tgt = level_.load() * 0.85f;
    waveAmp_ += (tgt - waveAmp_) * 0.1f;
    phase_ += 0.09f;

    const float W = b.getWidth(), H = b.getHeight();
    const float fr = frequency_.load();
    const float fm = fr > 0.0f ? (fr / 220.0f) * 2.5f : 1.0f;

    juce::Path wave;
    bool first = true;
    for (float i = 0.0f; i < W; i += 1.0f)
    {
        const float s = std::sin (phase_ + i * 0.1f * fm) * 0.7f
                      + std::sin (phase_ * 2.1f + i * 0.05f) * 0.2f;
        const float y = cy - s * waveAmp_ * (H * 0.4f);
        if (first) { wave.startNewSubPath (b.getX() + i, y); first = false; }
        else        wave.lineTo           (b.getX() + i, y);
    }
    g.setColour (Colour (kCyan).withAlpha (waveAmp_ > 0.02f ? 0.85f : 0.25f));
    g.strokePath (wave, juce::PathStrokeType (1.5f));
}

// ═════════════════════════════════════════════════════════════════════════════
LevelMeter::LevelMeter (MeterColour c, const juce::String& lbl)
    : meterColour_ (c), label_ (lbl) {}

void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // Label at top
    g.setColour (Colour (kCyan).withAlpha (0.3f));
    g.setFont (juce::Font (juce::FontOptions().withName ("Courier New").withHeight (8.0f)));
    g.drawText (label_, 0, 0, getWidth(), 14, juce::Justification::centred);

    auto track = b.withTrimmedTop (16.0f);
    g.setColour (Colour (kCyan).withAlpha (0.06f));
    g.fillRoundedRectangle (track, 2.0f);

    const float fillH = track.getHeight() * value_;
    auto fill = track.removeFromBottom (fillH);
    const Colour col = meterColour_ == MeterColour::Green
                       ? Colour (kGreen) : Colour (kOrange);
    g.setColour (col.withAlpha (0.85f));
    g.fillRoundedRectangle (fill, 2.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
static const bool kIsBlack[12] = {false,true,false,true,false,false,true,false,true,false,true,false};

PianoStrip::PianoStrip() { setInterceptsMouseClicks (false, false); }

void PianoStrip::buildKeys (int totalWidth)
{
    keys_.clear();
    int whiteCount = 0;
    for (int m = kFirst; m <= kLast; ++m)
        if (!kIsBlack[m % 12]) ++whiteCount;

    const float ww = (float)totalWidth / whiteCount;
    int wi = 0;
    for (int m = kFirst; m <= kLast; ++m)
    {
        if (!kIsBlack[m % 12])
        {
            keys_.push_back ({ m, false, wi * ww, ww - 0.5f, (float)getHeight() });
            ++wi;
        }
    }
    wi = 0;
    for (int m = kFirst; m <= kLast; ++m)
    {
        if (!kIsBlack[m % 12]) { ++wi; continue; }
        keys_.push_back ({ m, true, (wi - 1) * ww + ww * 0.62f, ww * 0.58f,
                           (float)getHeight() * 0.62f });
    }
}

void PianoStrip::paint (juce::Graphics& g)
{
    if (keys_.empty() || (int)keys_.front().h != getHeight())
        buildKeys (getWidth());

    g.setColour (Colour (kBg));
    g.fillAll();

    for (auto& k : keys_)
    {
        if (k.black) continue;
        const bool a = k.midi == activeNote_;
        g.setColour (a ? Colour (kCyan) : Colour (0xFFDDEEFF));
        g.fillRect (k.x, 0.0f, k.w, k.h);
        g.setColour (Colour (kBg).withAlpha (0.5f));
        g.drawRect (k.x, 0.0f, k.w, k.h, 0.5f);
        if (k.midi % 12 == 0)
        {
            g.setColour (a ? Colour (kBg) : Colour (0xFF888888));
            g.setFont (juce::Font (juce::FontOptions().withName("Courier New").withHeight(7.0f)));
            g.drawText ("C" + juce::String (k.midi / 12 - 1),
                        (int)k.x, (int)k.h - 12, (int)k.w, 10,
                        juce::Justification::centred);
        }
    }
    for (auto& k : keys_)
    {
        if (!k.black) continue;
        const bool a = k.midi == activeNote_;
        g.setColour (a ? Colour (0xFF0055CC) : Colour (kBgMid));
        g.fillRect (k.x, 0.0f, k.w, k.h);
        if (a)
        {
            g.setColour (Colour (kCyan).withAlpha (0.7f));
            g.drawRect (k.x, 0.0f, k.w, k.h, 0.8f);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
const juce::String TsengoVoiceSynthEditor::kNoteNames[12]
    = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

juce::String TsengoVoiceSynthEditor::midiName (int note)
{
    if (note < 0) return "--";
    return kNoteNames[note % 12] + juce::String (note / 12 - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
TsengoVoiceSynthEditor::TsengoVoiceSynthEditor (TsengoVoiceSynthProcessor& p)
    : AudioProcessorEditor (&p), proc_ (p),
      leftPanel_   ("MICROPHONE"),
      centerPanel_ ("PARAMETERS"),
      rightPanel_  ("MIDI OUTPUT")
{
    setLookAndFeel (&laf_);
    setSize (640, 430);

    // ── Title bar ─────────────────────────────────────────────────────────
    titleLabel_.setText ("TSENGO  VOICE  SYNTH", juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, Colour (kCyan));
    titleLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (13.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    versionLabel_.setText ("MIC \xe2\x86\x92 MIDI  \xc2\xb7  Channel Rack  \xc2\xb7  v2.1",
                           juce::dontSendNotification);
    versionLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.3f));
    versionLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (8.0f)));
    versionLabel_.setJustificationType (juce::Justification::right);
    addAndMakeVisible (versionLabel_);

    // ── Sidechain status ──────────────────────────────────────────────────
    scStatusLabel_.setText ("! CONNECTER LE MIC : Mixer \xe2\x86\x92 Sidechain",
                            juce::dontSendNotification);
    scStatusLabel_.setColour (juce::Label::textColourId, Colour (kOrange));
    scStatusLabel_.setColour (juce::Label::backgroundColourId,
                              Colour (kOrange).withAlpha (0.1f));
    scStatusLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (8.5f)).boldened());
    scStatusLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scStatusLabel_);

    // ── Left panel ─────────────────────────────────────────────────────────
    addAndMakeVisible (leftPanel_);

    micLabel_.setText ("SIGNAL AUDIO", juce::dontSendNotification);
    addAndMakeVisible (micLabel_);

    addAndMakeVisible (waveform_);

    statusLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (8.0f)));
    addAndMakeVisible (statusLabel_);

    noteLabel_.setColour (juce::Label::textColourId, Colour (kCyan));
    noteLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (32.0f)).boldened());
    noteLabel_.setJustificationType (juce::Justification::centred);
    noteLabel_.setText ("--", juce::dontSendNotification);
    addAndMakeVisible (noteLabel_);

    freqLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.5f));
    freqLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (9.0f)));
    freqLabel_.setJustificationType (juce::Justification::centred);
    freqLabel_.setText ("-- Hz", juce::dontSendNotification);
    addAndMakeVisible (freqLabel_);

    confTitleLabel_.setText ("YIN CONFIDENCE", juce::dontSendNotification);
    addAndMakeVisible (confTitleLabel_);

    confValLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.6f));
    confValLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (8.0f)));
    confValLabel_.setJustificationType (juce::Justification::right);
    confValLabel_.setText ("0%", juce::dontSendNotification);
    addAndMakeVisible (confValLabel_);

    // ── Center knobs ───────────────────────────────────────────────────────
    addAndMakeVisible (centerPanel_);

    auto addKnob = [&] (juce::Slider& sl, juce::Label& lbl,
                        const juce::String& paramId, const juce::String& name,
                        std::unique_ptr<APVTS::SliderAttachment>& att)
    {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (sl);
        lbl.setText (name, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
        att = std::make_unique<APVTS::SliderAttachment> (proc_.apvts, paramId, sl);
    };

    addKnob (threshSlider_, threshLbl_, "threshold", "THRESHOLD", threshAtt_);
    addKnob (volumeSlider_, volumeLbl_, "volume",    "VOLUME",    volumeAtt_);
    addKnob (attackSlider_, attackLbl_, "attack",    "ATTACK",    attackAtt_);
    addKnob (releaseSlider_,releaseLbl_,"release",   "RELEASE",   releaseAtt_);

    // ── Right panel ────────────────────────────────────────────────────────
    addAndMakeVisible (rightPanel_);

    midiNoteLabel_.setColour (juce::Label::textColourId, Colour (kCyan));
    midiNoteLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (22.0f)).boldened());
    midiNoteLabel_.setJustificationType (juce::Justification::centred);
    midiNoteLabel_.setText ("--", juce::dontSendNotification);
    addAndMakeVisible (midiNoteLabel_);

    midiNumLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.35f));
    midiNumLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (8.0f)));
    midiNumLabel_.setJustificationType (juce::Justification::centred);
    midiNumLabel_.setText ("MIDI CH 1", juce::dontSendNotification);
    addAndMakeVisible (midiNumLabel_);

    midiChLabel_.setColour (juce::Label::textColourId, Colour (kCyan).withAlpha (0.4f));
    midiChLabel_.setFont (juce::Font (juce::FontOptions()
                    .withName ("Courier New").withHeight (8.0f)));
    midiChLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (midiChLabel_);

    addAndMakeVisible (inMeter_);
    addAndMakeVisible (outMeter_);
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

    // Header 44px
    auto hdr = area.removeFromTop (44);
    titleLabel_  .setBounds (hdr.removeFromLeft (300).reduced (10, 12));
    versionLabel_.setBounds (hdr.reduced (8, 14));

    // Sidechain status bar 20px
    scStatusLabel_.setBounds (area.removeFromTop (20).reduced (6, 2));

    // Piano at bottom 66px
    piano_.setBounds (area.removeFromBottom (66));

    // Body
    auto body = area.reduced (6, 4);
    auto meters = body.removeFromRight (46);
    inMeter_ .setBounds (meters.removeFromLeft (20).reduced (2, 4));
    outMeter_.setBounds (meters.reduced (2, 4));

    // Left ~180
    auto leftArea = body.removeFromLeft (180);
    leftPanel_.setBounds (leftArea);
    auto lIn = leftArea.reduced (8, 18);
    micLabel_       .setBounds (lIn.removeFromTop (10));
    waveform_       .setBounds (lIn.removeFromTop (54));
    lIn.removeFromTop (4);
    statusLabel_    .setBounds (lIn.removeFromTop (12));
    lIn.removeFromTop (4);
    noteLabel_      .setBounds (lIn.removeFromTop (40));
    freqLabel_      .setBounds (lIn.removeFromTop (14));
    lIn.removeFromTop (4);
    auto crow = lIn.removeFromTop (10);
    confTitleLabel_ .setBounds (crow.removeFromLeft (110));
    confValLabel_   .setBounds (crow);

    body.removeFromLeft (6);

    // Right ~140
    auto rightArea = body.removeFromRight (140);
    rightPanel_.setBounds (rightArea);
    auto rIn = rightArea.reduced (8, 18);
    rIn.removeFromTop (8);
    midiChLabel_  .setBounds (rIn.removeFromTop (10));
    midiNoteLabel_.setBounds (rIn.removeFromTop (46));
    midiNumLabel_ .setBounds (rIn.removeFromTop (12));

    body.removeFromRight (6);

    // Center — knobs
    centerPanel_.setBounds (body);
    auto cIn = body.reduced (8, 18);
    const int kw = cIn.getWidth()  / 2;
    const int kh = cIn.getHeight() / 2;

    auto placeKnob = [] (juce::Slider& sl, juce::Label& lb,
                         juce::Rectangle<int> cell)
    {
        lb.setBounds (cell.removeFromBottom (14));
        sl.setBounds (cell.reduced (4));
    };
    auto r1 = cIn.removeFromTop (kh);
    auto r2 = cIn;
    placeKnob (threshSlider_, threshLbl_,  r1.removeFromLeft (kw));
    placeKnob (volumeSlider_, volumeLbl_,  r1);
    placeKnob (attackSlider_, attackLbl_,  r2.removeFromLeft (kw));
    placeKnob (releaseSlider_,releaseLbl_, r2);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colour (kBg));

    // Header gradient
    juce::ColourGradient hg (Colour (kBgMid), 0, 0, Colour (kBg), 0, 44, false);
    g.setGradientFill (hg);
    g.fillRect (0, 0, getWidth(), 44);
    g.setColour (Colour (kCyan).withAlpha (0.12f));
    g.drawHorizontalLine (43, 0.0f, (float)getWidth());

    // Blinking dot
    const float a = 0.5f + 0.5f * std::sin (blinkPhase_);
    g.setColour (Colour (kCyan).withAlpha (a));
    g.fillEllipse (8.0f, 18.0f, 7.0f, 7.0f);

    // Confidence bar inside left panel
    const auto state = proc_.getDetectionState();
    const auto lb = leftPanel_.getBounds();
    const int bY  = lb.getBottom() - 22;
    const int bX  = lb.getX() + 16;
    const int bW  = lb.getWidth() - 32;
    g.setColour (Colour (kCyan).withAlpha (0.07f));
    g.fillRoundedRectangle ((float)bX, (float)bY, (float)bW, 2.0f, 1.0f);
    g.setColour (Colour (kCyan).withAlpha (0.7f));
    g.fillRoundedRectangle ((float)bX, (float)bY,
                            bW * state.confidence, 2.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
void TsengoVoiceSynthEditor::timerCallback()
{
    blinkPhase_ += 0.12f;
    if (blinkPhase_ > juce::MathConstants<float>::twoPi)
        blinkPhase_ -= juce::MathConstants<float>::twoPi;

    const auto s = proc_.getDetectionState();

    // Sidechain banner
    if (s.micConnected)
    {
        scStatusLabel_.setText ("\xe2\x9c\x93  MIC SIDECHAIN CONNECTE",
                                juce::dontSendNotification);
        scStatusLabel_.setColour (juce::Label::textColourId, Colour (kGreen));
        scStatusLabel_.setColour (juce::Label::backgroundColourId,
                                  Colour (kGreen).withAlpha (0.08f));
    }
    else
    {
        scStatusLabel_.setText ("! CONNECTER LE MIC : Mixer \xe2\x86\x92 Sidechain",
                                juce::dontSendNotification);
        scStatusLabel_.setColour (juce::Label::textColourId, Colour (kOrange));
        scStatusLabel_.setColour (juce::Label::backgroundColourId,
                                  Colour (kOrange).withAlpha (0.1f));
    }

    waveform_.setLevel     (s.inputLevel);
    waveform_.setFrequency (s.frequency);

    if (s.midiNote >= 0)
    {
        noteLabel_    .setText (kNoteNames[s.midiNote % 12]
                                + juce::String (s.midiNote / 12 - 1),
                                juce::dontSendNotification);
        freqLabel_    .setText (juce::String (s.frequency, 1) + " Hz",
                                juce::dontSendNotification);
        midiNoteLabel_.setText (kNoteNames[s.midiNote % 12]
                                + juce::String (s.midiNote / 12 - 1),
                                juce::dontSendNotification);
        midiChLabel_  .setText ("NOTE " + juce::String (s.midiNote),
                                juce::dontSendNotification);
    }
    else
    {
        noteLabel_    .setText ("--", juce::dontSendNotification);
        freqLabel_    .setText ("-- Hz", juce::dontSendNotification);
        midiNoteLabel_.setText ("--", juce::dontSendNotification);
        midiChLabel_  .setText ("--", juce::dontSendNotification);
    }

    confValLabel_.setText (juce::String (juce::roundToInt (s.confidence * 100)) + "%",
                           juce::dontSendNotification);

    statusLabel_.setText (s.inputLevel > 0.005f ? "\xe2\x97\x89  SIGNAL DETECTE"
                                                 : "\xe2\x97\x8b  EN ATTENTE...",
                          juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::textColourId,
                            s.inputLevel > 0.005f
                            ? Colour (kGreen).withAlpha (0.85f)
                            : Colour (kCyan).withAlpha (0.35f));

    inMeter_ .setValue (s.inputLevel);
    outMeter_.setValue (s.midiNote >= 0 ? s.inputLevel * 0.9f : 0.0f);
    piano_   .setActiveNote (s.midiNote);

    repaint();
}
