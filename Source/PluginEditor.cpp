#include "PluginEditor.h"

static void styleKnob (juce::Slider& s, juce::Colour c)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId,    c);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, c.withAlpha (0.2f));
    s.setColour (juce::Slider::backgroundColourId,          juce::Colour (0xFF080D18));
}

static void styleLabel (juce::Label& l, const juce::String& t,
                         float size, juce::Colour c,
                         juce::Justification j = juce::Justification::centred)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (juce::Font ("Courier New", size, juce::Font::plain));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (j);
}

//==============================================================================
TsengoEditor::TsengoEditor (TsengoProcessor& p)
    : AudioProcessorEditor (&p), p_ (p)
{
    setSize (480, 400);

    // Device selector
    styleLabel (lblDevice_, "MICROPHONE INPUT", 10.f,
                cyan().withAlpha (0.6f), juce::Justification::left);

    cmbDevice_.setColour (juce::ComboBox::backgroundColourId, surf());
    cmbDevice_.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    cmbDevice_.setColour (juce::ComboBox::outlineColourId,    cyan().withAlpha (0.3f));
    cmbDevice_.setColour (juce::ComboBox::arrowColourId,      cyan());
    addAndMakeVisible (cmbDevice_);
    addAndMakeVisible (lblDevice_);

    btnRefresh_.setColour (juce::TextButton::buttonColourId,  surf());
    btnRefresh_.setColour (juce::TextButton::textColourOffId, cyan());
    btnRefresh_.onClick = [this] { refreshDevices(); };
    addAndMakeVisible (btnRefresh_);

    btnConnect_.setColour (juce::TextButton::buttonColourId,  cyan().withAlpha (0.15f));
    btnConnect_.setColour (juce::TextButton::textColourOffId, cyan());
    btnConnect_.onClick = [this]
    {
        auto name = cmbDevice_.getText();
        if (name.isNotEmpty()) p_.openDevice (name);
    };
    addAndMakeVisible (btnConnect_);

    // Knobs
    sldThreshold_.setRange (0.01, 0.5,  0.01); sldThreshold_.setValue (0.10);
    sldSmoothing_.setRange (0.01, 0.5,  0.01); sldSmoothing_.setValue (0.15);
    sldGain_     .setRange (0.5,  4.0,  0.1);  sldGain_     .setValue (1.0);
    styleKnob (sldThreshold_, orange());
    styleKnob (sldSmoothing_, purple());
    styleKnob (sldGain_,      green());
    sldThreshold_.onValueChange = [this] { p_.threshold = (float)sldThreshold_.getValue(); };
    sldSmoothing_.onValueChange = [this] { p_.smoothing = (float)sldSmoothing_.getValue(); };
    sldGain_     .onValueChange = [this] { p_.gain      = (float)sldGain_     .getValue(); };
    addAndMakeVisible (sldThreshold_);
    addAndMakeVisible (sldSmoothing_);
    addAndMakeVisible (sldGain_);

    styleLabel (lblThr_,  "THRESHOLD", 9.f, juce::Colours::white.withAlpha (0.4f));
    styleLabel (lblSmo_,  "SMOOTH",    9.f, juce::Colours::white.withAlpha (0.4f));
    styleLabel (lblGain_, "GAIN",      9.f, juce::Colours::white.withAlpha (0.4f));
    addAndMakeVisible (lblThr_);
    addAndMakeVisible (lblSmo_);
    addAndMakeVisible (lblGain_);

    // Display labels
    styleLabel (lblNote_, "--",    36.f, cyan());
    styleLabel (lblHz_,   "0 Hz",  12.f, juce::Colours::white.withAlpha (0.6f));
    styleLabel (lblConf_, "0%",    11.f, green());
    addAndMakeVisible (lblNote_);
    addAndMakeVisible (lblHz_);
    addAndMakeVisible (lblConf_);

    refreshDevices();
    startTimerHz (30);
}

TsengoEditor::~TsengoEditor() { stopTimer(); }

//==============================================================================
void TsengoEditor::refreshDevices()
{
    cmbDevice_.clear();
    auto devs = p_.getInputDevices();
    for (int i = 0; i < devs.size(); ++i)
        cmbDevice_.addItem (devs[i], i + 1);
    if (cmbDevice_.getNumItems() > 0)
        cmbDevice_.setSelectedItemIndex (0);
}

//==============================================================================
void TsengoEditor::timerCallback()
{
    // Smooth meters
    dispMic_  += (p_.getMicLevel()  - dispMic_)  * 0.2f;
    dispMidi_ += (p_.getMidiLevel() - dispMidi_) * 0.15f;

    // Note display
    int   note = p_.getNote();
    float hz   = p_.getPitchHz();
    float conf = p_.getConfidence();

    if (note >= 0)
    {
        int oct = note / 12 - 1;
        lblNote_.setText (juce::String (noteName (note)) + juce::String (oct),
                          juce::dontSendNotification);
        lblNote_.setColour (juce::Label::textColourId, cyan());
        lblHz_  .setText (juce::String (hz, 1) + " Hz", juce::dontSendNotification);
        lblConf_.setText (juce::String (juce::roundToInt (conf * 100)) + "%",
                          juce::dontSendNotification);
    }
    else
    {
        lblNote_.setText ("--", juce::dontSendNotification);
        lblNote_.setColour (juce::Label::textColourId, juce::Colours::grey);
        lblHz_  .setText ("0 Hz", juce::dontSendNotification);
        lblConf_.setText ("0%",   juce::dontSendNotification);
    }

    repaint();
}

//==============================================================================
void TsengoEditor::paint (juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();

    // Background
    g.fillAll (dark());

    // Header
    g.setColour (panel());
    g.fillRect (0, 0, W, 46);
    g.setColour (cyan().withAlpha (0.12f));
    g.fillRect (0, 45, W, 1);

    // Title
    g.setFont (juce::Font ("Courier New", 15.f, juce::Font::bold));
    g.setColour (cyan());
    g.drawText ("TSENGO  VOICE  SYNTH  v3.0", 0, 0, W, 46,
                juce::Justification::centred);

    // Dot
    g.setColour (cyan());
    g.fillEllipse (14.f, 18.f, 9.f, 9.f);
    g.setColour (cyan().withAlpha (0.35f));
    g.drawEllipse (12.f, 16.f, 13.f, 13.f, 1.2f);

    // Note box
    g.setColour (panel());
    g.fillRoundedRectangle (12.f, 54.f, 200.f, 82.f, 7.f);
    g.setColour (cyan().withAlpha (0.15f));
    g.drawRoundedRectangle (12.f, 54.f, 200.f, 82.f, 7.f, 1.f);
    g.setFont (juce::Font ("Courier New", 9.f, juce::Font::plain));
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawText ("NOTA DETEKTEE", 20, 57, 180, 13, juce::Justification::left);

    // Meters
    const int mx = 370, my = 54, mw = 16, mh = 82;
    g.setColour (juce::Colour (0xFF050A12));
    g.fillRoundedRectangle ((float)mx,      (float)my, (float)mw, (float)mh, 4.f);
    g.fillRoundedRectangle ((float)mx + 22, (float)my, (float)mw, (float)mh, 4.f);

    float micH  = juce::jlimit (0.f, (float)mh, dispMic_  * mh);
    float midiH = juce::jlimit (0.f, (float)mh, dispMidi_ * mh);
    g.setColour (green());
    if (micH  > 0) g.fillRoundedRectangle ((float)mx,      my + mh - micH,  (float)mw, micH,  3.f);
    g.setColour (cyan());
    if (midiH > 0) g.fillRoundedRectangle ((float)mx + 22, my + mh - midiH, (float)mw, midiH, 3.f);

    g.setFont (juce::Font ("Courier New", 8.f, juce::Font::plain));
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawText ("MIC",  mx,      my + mh + 3, mw,   10, juce::Justification::centred);
    g.drawText ("MIDI", mx + 22, my + mh + 3, mw+4, 10, juce::Justification::centred);

    // Knob section background
    g.setColour (panel());
    g.fillRoundedRectangle (12.f, 200.f, (float)W - 24.f, 80.f, 7.f);
    g.setColour (cyan().withAlpha (0.1f));
    g.drawRoundedRectangle (12.f, 200.f, (float)W - 24.f, 80.f, 7.f, 1.f);
    g.setFont (juce::Font ("Courier New", 9.f, juce::Font::plain));
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawText ("PARAMÈTRES", 22, 203, 140, 12, juce::Justification::left);
}

//==============================================================================
void TsengoEditor::resized()
{
    // Header → device
    lblDevice_.setBounds (12, 54 + 82 + 10, 200, 14);
    cmbDevice_.setBounds (12, 54 + 82 + 26, 300, 26);
    btnRefresh_.setBounds (316, 54 + 82 + 26, 30,  26);
    btnConnect_.setBounds (350, 54 + 82 + 26, 90,  26);

    // Note display inside note box
    lblNote_.setBounds (20, 66, 120, 50);
    lblHz_  .setBounds (140, 70, 65, 18);
    lblConf_.setBounds (140, 90, 65, 18);

    // Knobs
    const int ky = 210, kw = 56;
    sldThreshold_.setBounds (20,         ky, kw, kw);
    sldSmoothing_.setBounds (20 + kw+10, ky, kw, kw);
    sldGain_     .setBounds (20+(kw+10)*2, ky, kw, kw);
    lblThr_ .setBounds (20,           ky + kw,     kw, 14);
    lblSmo_ .setBounds (20 + kw+10,   ky + kw,     kw, 14);
    lblGain_.setBounds (20+(kw+10)*2, ky + kw,     kw, 14);
}
