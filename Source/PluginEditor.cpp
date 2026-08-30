#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kWinW      = 540;
    constexpr int kWinH      = 620;
    constexpr int kHeaderH   = 46;
    constexpr int kTabH      = 30;
    constexpr int kContentX  = 12;
    constexpr int kContentY  = kHeaderH + kTabH + 8;   // 84

    void styleKnob (juce::Slider& s, juce::Colour c)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::rotarySliderFillColourId,    c);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, c.withAlpha (0.2f));
        s.setColour (juce::Slider::backgroundColourId,          juce::Colour (0xFF080D18));
    }

    void styleBar (juce::Slider& s, juce::Colour c)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::trackColourId, c);
        s.setColour (juce::Slider::thumbColourId, c);
    }

    void styleLabel (juce::Label& l, const juce::String& t,
                     float size, juce::Colour c,
                     juce::Justification j = juce::Justification::centred)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font ("Courier New", size, juce::Font::plain));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (j);
    }

    void styleCombo (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF172236));
        c.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
        c.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xFF00E5FF).withAlpha (0.3f));
        c.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xFF00E5FF));
    }
}

void TsengoEditor::styleToggle (juce::TextButton& b, juce::Colour c)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xFF172236));
    b.setColour (juce::TextButton::buttonOnColourId, c.withAlpha (0.35f));
    b.setColour (juce::TextButton::textColourOffId,  c.withAlpha (0.5f));
    b.setColour (juce::TextButton::textColourOnId,   c);
}

juce::String TsengoEditor::noteLabel (int midiNote)
{
    const int oct = midiNote / 12 - 1;
    return juce::String (noteName (midiNote)) + juce::String (oct)
            + " (" + juce::String (midiNote) + ")";
}

//==============================================================================
TsengoEditor::TsengoEditor (TsengoProcessor& p)
    : AudioProcessorEditor (&p), p_ (p)
{
    setSize (kWinW, kWinH);

    static const char* tabNames[kNumTabs] = { "PLAY", "TRIGGERS", "KEY", "CHORDS", "ASSIGN", "SETUP" };
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& b = tabBtns_[(size_t) i];
        b.setButtonText (tabNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (7001);
        b.setColour (juce::TextButton::buttonColourId,   panel());
        b.setColour (juce::TextButton::buttonOnColourId, cyan().withAlpha (0.25f));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha (0.45f));
        b.setColour (juce::TextButton::textColourOnId,   cyan());
        b.onClick = [this, i] { setTab (i); };
        addAndMakeVisible (b);

        addChildComponent (pages_[(size_t) i]);
    }

    buildPlayPage();
    buildTriggerPage();
    buildKeyPage();
    buildChordPage();
    buildAssignPage();
    buildSetupPage();
    buildCalibrationOverlay();

    pages_[(size_t) TabPlay].onPaint = [this] (juce::Graphics& g) { paintPlayPage (g); };

    tabBtns_[(size_t) TabPlay].setToggleState (true, juce::dontSendNotification);
    setTab (TabPlay);

    refreshDevices();
    syncFromProcessor();

    // Children are created after setSize(), so lay everything out once more.
    resized();
    startTimerHz (30);
}

TsengoEditor::~TsengoEditor()
{
    stopTimer();
    // Never leave a pad armed for training when the window closes
    if (p_.triggers().getTrainingPad() >= 0)
        p_.triggers().endTraining();
}

//==============================================================================
void TsengoEditor::setTab (int tab)
{
    currentTab_ = juce::jlimit (0, (int) kNumTabs - 1, tab);
    for (int i = 0; i < kNumTabs; ++i)
        pages_[(size_t) i].setVisible (i == currentTab_);
    repaint();
}

//==============================================================================
void TsengoEditor::buildPlayPage()
{
    auto& pg = pages_[(size_t) TabPlay];

    // Knobs
    sldThreshold_.setRange (0.005, 0.5, 0.005);
    sldSmoothing_.setRange (0.01,  0.5, 0.01);
    sldGain_     .setRange (0.5,   8.0, 0.1);
    styleKnob (sldThreshold_, orange());
    styleKnob (sldSmoothing_, purple());
    styleKnob (sldGain_,      green());
    sldThreshold_.onValueChange = [this] { p_.threshold = (float) sldThreshold_.getValue(); };
    sldSmoothing_.onValueChange = [this] { p_.smoothing = (float) sldSmoothing_.getValue(); };
    sldGain_     .onValueChange = [this] { p_.gain      = (float) sldGain_     .getValue(); };
    pg.addAndMakeVisible (sldThreshold_);
    pg.addAndMakeVisible (sldSmoothing_);
    pg.addAndMakeVisible (sldGain_);

    styleLabel (lblThr_,  "THRESHOLD", 9.f, juce::Colours::white.withAlpha (0.4f));
    styleLabel (lblSmo_,  "SMOOTH",    9.f, juce::Colours::white.withAlpha (0.4f));
    styleLabel (lblGain_, "GAIN",      9.f, juce::Colours::white.withAlpha (0.4f));
    pg.addAndMakeVisible (lblThr_);
    pg.addAndMakeVisible (lblSmo_);
    pg.addAndMakeVisible (lblGain_);

    styleLabel (lblNote_, "--",   36.f, cyan());
    styleLabel (lblHz_,   "0 Hz", 12.f, juce::Colours::white.withAlpha (0.6f));
    styleLabel (lblConf_, "0%",   11.f, green());
    pg.addAndMakeVisible (lblNote_);
    pg.addAndMakeVisible (lblHz_);
    pg.addAndMakeVisible (lblConf_);

    for (int i = -2; i <= 2; ++i)
        cmbOctave_.addItem ((i > 0 ? "+" : "") + juce::String (i) + " oct", i + 3);
    cmbOctave_.onChange = [this] { p_.octaveShift = cmbOctave_.getSelectedId() - 3; };
    styleCombo (cmbOctave_);
    pg.addAndMakeVisible (cmbOctave_);
    styleLabel (lblOctave_, "OCTAVE", 9.f, juce::Colours::white.withAlpha (0.35f),
                juce::Justification::left);
    pg.addAndMakeVisible (lblOctave_);

    sldStickiness_.setRange (1, 10, 1);
    styleBar (sldStickiness_, cyan());
    sldStickiness_.onValueChange = [this] { p_.stickiness = (int) sldStickiness_.getValue(); };
    pg.addAndMakeVisible (sldStickiness_);
    styleLabel (lblStick_, "STICKINESS", 9.f, juce::Colours::white.withAlpha (0.35f),
                juce::Justification::left);
    pg.addAndMakeVisible (lblStick_);

    styleToggle (btnMonitor_, green());
    btnMonitor_.onClick = [this] { p_.monitorSynthEnabled = btnMonitor_.getToggleState(); };
    pg.addAndMakeVisible (btnMonitor_);

    sldMonitorVol_.setRange (0.0, 1.0, 0.01);
    styleBar (sldMonitorVol_, green());
    sldMonitorVol_.onValueChange = [this] { p_.monitorVolume = (float) sldMonitorVol_.getValue(); };
    pg.addAndMakeVisible (sldMonitorVol_);
    styleLabel (lblMonVol_, "VOL", 9.f, juce::Colours::white.withAlpha (0.35f));
    pg.addAndMakeVisible (lblMonVol_);

    styleLabel (lblVowelTitle_, "VOYELLES  (FORMANTS)", 9.f,
                juce::Colours::white.withAlpha (0.25f), juce::Justification::left);
    styleLabel (lblAaa_, "aaa", 10.f, orange());
    styleLabel (lblEee_, "eee", 10.f, purple());
    styleLabel (lblOoo_, "ooo", 10.f, green());
    styleLabel (lblEnv_, "env", 10.f, yellow());
    styleLabel (lblFormants_, "F1 -- Hz   F2 -- Hz", 9.f,
                juce::Colours::white.withAlpha (0.4f), juce::Justification::right);
    pg.addAndMakeVisible (lblVowelTitle_);
    pg.addAndMakeVisible (lblAaa_);
    pg.addAndMakeVisible (lblEee_);
    pg.addAndMakeVisible (lblOoo_);
    pg.addAndMakeVisible (lblEnv_);
    pg.addAndMakeVisible (lblFormants_);
}

//==============================================================================
void TsengoEditor::buildTriggerPage()
{
    auto& pg = pages_[(size_t) TabTriggers];

    pg.onPaint = [this] (juce::Graphics& g)
    {
        auto& page = pages_[(size_t) TabTriggers];
        const int w = page.getWidth();

        g.setColour (panel());
        g.fillRoundedRectangle (0.f, 0.f, (float) w, 92.f, 7.f);

        for (int i = 0; i < TriggerEngine::kNumPads; ++i)
        {
            const float y = 100.f + (float) i * 40.f;
            const bool  trained = p_.triggers().isPadTrained (i);
            const bool  arming  = (p_.triggers().getTrainingPad() == i);

            juce::Colour bg = surf().withAlpha (0.55f);
            if (arming) bg = orange().withAlpha (0.30f);
            else if (flashPad_ == i && flashAmount_ > 0.01f)
                bg = cyan().withAlpha (0.10f + 0.35f * flashAmount_);

            g.setColour (bg);
            g.fillRoundedRectangle (0.f, y, (float) w, 34.f, 5.f);

            g.setColour (trained ? green() : juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (8.f, y + 13.f, 8.f, 8.f);
        }
    };

    styleToggle (btnTriggers_, orange());
    btnTriggers_.onClick = [this] { p_.triggers().enabled = btnTriggers_.getToggleState(); };
    pg.addAndMakeVisible (btnTriggers_);

    styleLabel (lblTrigStatus_, "", 10.f, cyan(), juce::Justification::right);
    pg.addAndMakeVisible (lblTrigStatus_);

    styleLabel (lblTrigSens_,   "SENSIBILITE", 9.f, juce::Colours::white.withAlpha (0.4f),
                juce::Justification::left);
    styleLabel (lblTrigStrict_, "PRECISION",   9.f, juce::Colours::white.withAlpha (0.4f),
                juce::Justification::left);
    pg.addAndMakeVisible (lblTrigSens_);
    pg.addAndMakeVisible (lblTrigStrict_);

    sldTrigSens_  .setRange (0.0, 1.0, 0.01);
    sldTrigStrict_.setRange (0.0, 1.0, 0.01);
    styleBar (sldTrigSens_,   orange());
    styleBar (sldTrigStrict_, purple());
    sldTrigSens_  .onValueChange = [this] { p_.triggers().sensitivity = (float) sldTrigSens_.getValue(); };
    sldTrigStrict_.onValueChange = [this] { p_.triggers().strictness  = (float) sldTrigStrict_.getValue(); };
    pg.addAndMakeVisible (sldTrigSens_);
    pg.addAndMakeVisible (sldTrigStrict_);

    styleLabel (lblTrigHint_,
                "TRAIN: tsindrio, avereno in-5 ka hatramin'ny in-10 ilay feo "
                "(bm / ts / psh...), dia tsindrio STOP. Ny pad tsy voaofana dia tsy "
                "mandefa MIDI mihitsy.",
                9.f, juce::Colours::white.withAlpha (0.45f), juce::Justification::topLeft);
    pg.addAndMakeVisible (lblTrigHint_);

    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
    {
        const size_t idx = (size_t) i;

        styleLabel (padLabels_[idx], TsengoProcessor::defaultPadName (i), 10.f,
                    juce::Colours::white.withAlpha (0.75f), juce::Justification::left);
        pg.addAndMakeVisible (padLabels_[idx]);

        for (int n = 24; n <= 96; ++n)
            padNoteCombos_[idx].addItem (noteLabel (n), n + 1);
        styleCombo (padNoteCombos_[idx]);
        padNoteCombos_[idx].onChange = [this, i, idx]
        {
            const int id = padNoteCombos_[idx].getSelectedId();
            if (id > 0) p_.padNote[i].store (id - 1);
        };
        pg.addAndMakeVisible (padNoteCombos_[idx]);

        padTrainBtns_[idx].setButtonText ("TRAIN");
        padTrainBtns_[idx].setColour (juce::TextButton::buttonColourId,  surf());
        padTrainBtns_[idx].setColour (juce::TextButton::textColourOffId, orange());
        padTrainBtns_[idx].onClick = [this, i]
        {
            auto& tr = p_.triggers();
            if (tr.getTrainingPad() == i) tr.endTraining();
            else                          tr.beginTraining (i);
            updateTriggerUI();
        };
        pg.addAndMakeVisible (padTrainBtns_[idx]);

        styleLabel (padCountLabels_[idx], "0 hits", 9.f,
                    juce::Colours::white.withAlpha (0.45f), juce::Justification::centred);
        pg.addAndMakeVisible (padCountLabels_[idx]);

        padClearBtns_[idx].setButtonText ("CLEAR");
        padClearBtns_[idx].setColour (juce::TextButton::buttonColourId,  surf());
        padClearBtns_[idx].setColour (juce::TextButton::textColourOffId,
                                       juce::Colours::white.withAlpha (0.5f));
        padClearBtns_[idx].onClick = [this, i]
        {
            p_.triggers().clearPad (i);
            updateTriggerUI();
        };
        pg.addAndMakeVisible (padClearBtns_[idx]);
    }
}

//==============================================================================
void TsengoEditor::buildKeyPage()
{
    auto& pg = pages_[(size_t) TabKey];

    styleLabel (lblKeyTitle_, "KEY / SCALE", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblKeyTitle_);

    static const char* roots[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    for (int i = 0; i < 12; ++i) cmbKeyRoot_.addItem (roots[i], i + 1);
    cmbKeyRoot_.onChange = [this] { p_.keyRoot = cmbKeyRoot_.getSelectedId() - 1; };
    styleCombo (cmbKeyRoot_);
    pg.addAndMakeVisible (cmbKeyRoot_);

    static const char* scales[] = { "Chromatic","Major","Minor","Maj Penta","Min Penta","Dorian" };
    for (int i = 0; i < 6; ++i) cmbScale_.addItem (scales[i], i + 1);
    cmbScale_.onChange = [this] { p_.scaleType = cmbScale_.getSelectedId() - 1; };
    styleCombo (cmbScale_);
    pg.addAndMakeVisible (cmbScale_);

    styleToggle (btnQuantize_, cyan());
    btnQuantize_.onClick = [this] { p_.quantizeToKey = btnQuantize_.getToggleState(); };
    pg.addAndMakeVisible (btnQuantize_);

    styleLabel (lblBendTitle_, "PITCH BEND  (vibrato / glissando)", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblBendTitle_);

    styleToggle (btnBend_, cyan());
    btnBend_.onClick = [this] { p_.pitchBendEnabled = btnBend_.getToggleState(); };
    pg.addAndMakeVisible (btnBend_);

    for (int i = 1; i <= 12; ++i)
        cmbBendRange_.addItem (juce::String (i) + " semi", i);
    cmbBendRange_.onChange = [this] { p_.pitchBendRangeSemitones = cmbBendRange_.getSelectedId(); };
    styleCombo (cmbBendRange_);
    pg.addAndMakeVisible (cmbBendRange_);

    styleLabel (lblTimeTitle_, "TIME QUANTIZE  (grille tempo)", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblTimeTitle_);

    styleToggle (btnTimeQ_, orange());
    btnTimeQ_.onClick = [this] { p_.timeQuantizeEnabled = btnTimeQ_.getToggleState(); };
    pg.addAndMakeVisible (btnTimeQ_);

    static const char* divs[] = { "1/4","1/8","1/16","1/32" };
    for (int i = 0; i < 4; ++i) cmbTimeDiv_.addItem (divs[i], i + 1);
    cmbTimeDiv_.onChange = [this]
    {
        static const int divVals[] = { 1, 2, 4, 8 };
        const int idx = juce::jlimit (0, 3, cmbTimeDiv_.getSelectedId() - 1);
        p_.timeQuantizeDivision = divVals[idx];
    };
    styleCombo (cmbTimeDiv_);
    pg.addAndMakeVisible (cmbTimeDiv_);

}

//==============================================================================
void TsengoEditor::buildChordPage()
{
    auto& pg = pages_[(size_t) TabChords];

    styleLabel (lblChordTitle_, "CHORDS", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblChordTitle_);

    styleToggle (btnChords_, purple());
    btnChords_.onClick = [this] { p_.chordsEnabled = btnChords_.getToggleState(); };
    pg.addAndMakeVisible (btnChords_);

    static const char* chordTypes[] = { "Major","Minor","Sus4","Maj7","Min7","Octave" };
    for (int i = 0; i < 6; ++i) cmbChordType_.addItem (chordTypes[i], i + 1);
    cmbChordType_.onChange = [this] { p_.chordType = cmbChordType_.getSelectedId() - 1; };
    styleCombo (cmbChordType_);
    pg.addAndMakeVisible (cmbChordType_);

    styleLabel (lblChordInfo_,
                "Ny nota hiraina no lasa fototra (root), dia ampiana ny nota hafa "
                "araka ny karazana akora voafidy.\n\n"
                "Major 0-4-7   Minor 0-3-7   Sus4 0-5-7\n"
                "Maj7 0-4-7-11   Min7 0-3-7-10   Octave 0-12\n\n"
                "Torohevitra: ampiasao miaraka amin'ny QUANTIZE ao amin'ny tab KEY "
                "mba ho ao anatin'ny lakile ny akora rehetra.",
                10.f, juce::Colours::white.withAlpha (0.5f), juce::Justification::topLeft);
    pg.addAndMakeVisible (lblChordInfo_);
}

//==============================================================================
void TsengoEditor::buildAssignPage()
{
    auto& pg = pages_[(size_t) TabAssign];

    styleLabel (lblAssignTitle_, "MACRO -> MIDI CC", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblAssignTitle_);

    styleLabel (lblCcAaa_, "aaa", 11.f, orange(), juce::Justification::left);
    styleLabel (lblCcEee_, "eee", 11.f, purple(), juce::Justification::left);
    styleLabel (lblCcOoo_, "ooo", 11.f, green(),  juce::Justification::left);
    styleLabel (lblCcEnv_, "env", 11.f, yellow(), juce::Justification::left);
    pg.addAndMakeVisible (lblCcAaa_);
    pg.addAndMakeVisible (lblCcEee_);
    pg.addAndMakeVisible (lblCcOoo_);
    pg.addAndMakeVisible (lblCcEnv_);

    auto fillCc = [] (juce::ComboBox& c)
    {
        for (int i = 1; i <= 119; ++i) c.addItem ("CC " + juce::String (i), i);
    };
    fillCc (cmbCcAaa_); fillCc (cmbCcEee_); fillCc (cmbCcOoo_); fillCc (cmbCcEnv_);
    cmbCcAaa_.onChange = [this] { p_.ccAaa = cmbCcAaa_.getSelectedId(); };
    cmbCcEee_.onChange = [this] { p_.ccEee = cmbCcEee_.getSelectedId(); };
    cmbCcOoo_.onChange = [this] { p_.ccOoo = cmbCcOoo_.getSelectedId(); };
    cmbCcEnv_.onChange = [this] { p_.ccEnv = cmbCcEnv_.getSelectedId(); };
    styleCombo (cmbCcAaa_); styleCombo (cmbCcEee_);
    styleCombo (cmbCcOoo_); styleCombo (cmbCcEnv_);
    pg.addAndMakeVisible (cmbCcAaa_);
    pg.addAndMakeVisible (cmbCcEee_);
    pg.addAndMakeVisible (cmbCcOoo_);
    pg.addAndMakeVisible (cmbCcEnv_);

    styleLabel (lblChanTitle_, "CANAUX MIDI", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblChanTitle_);

    styleLabel (lblChanPitch_, "Notes chantees", 10.f,
                juce::Colours::white.withAlpha (0.6f), juce::Justification::left);
    styleLabel (lblChanTrig_,  "Triggers / drums", 10.f,
                juce::Colours::white.withAlpha (0.6f), juce::Justification::left);
    pg.addAndMakeVisible (lblChanPitch_);
    pg.addAndMakeVisible (lblChanTrig_);

    for (int i = 1; i <= 16; ++i)
    {
        cmbChanPitch_.addItem ("Ch " + juce::String (i), i);
        cmbChanTrig_ .addItem ("Ch " + juce::String (i), i);
    }
    cmbChanPitch_.onChange = [this] { p_.pitchChannel   = cmbChanPitch_.getSelectedId(); };
    cmbChanTrig_ .onChange = [this] { p_.triggerChannel = cmbChanTrig_ .getSelectedId(); };
    styleCombo (cmbChanPitch_);
    styleCombo (cmbChanTrig_);
    pg.addAndMakeVisible (cmbChanPitch_);
    pg.addAndMakeVisible (cmbChanTrig_);
}

//==============================================================================
void TsengoEditor::buildSetupPage()
{
    auto& pg = pages_[(size_t) TabSetup];

    styleLabel (lblSourceTitle_, "SOURCE AUDIO", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblSourceTitle_);

    cmbSource_.addItem ("Auto",                 1);
    cmbSource_.addItem ("Entree de l'hote",     2);
    cmbSource_.addItem ("Micro interne",        3);
    cmbSource_.onChange = [this] { p_.inputSource = cmbSource_.getSelectedId() - 1; };
    styleCombo (cmbSource_);
    pg.addAndMakeVisible (cmbSource_);

    styleLabel (lblSourceState_, "", 10.f, cyan(), juce::Justification::left);
    pg.addAndMakeVisible (lblSourceState_);

    styleLabel (lblMicTitle_, "MICROPHONE INTERNE  (mode synth FL Studio)", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblMicTitle_);

    styleCombo (cmbDevice_);
    pg.addAndMakeVisible (cmbDevice_);

    btnRefresh_.setColour (juce::TextButton::buttonColourId,  surf());
    btnRefresh_.setColour (juce::TextButton::textColourOffId, cyan());
    btnRefresh_.onClick = [this] { refreshDevices(); };
    pg.addAndMakeVisible (btnRefresh_);

    btnConnect_.setColour (juce::TextButton::buttonColourId,  cyan().withAlpha (0.15f));
    btnConnect_.setColour (juce::TextButton::textColourOffId, cyan());
    btnConnect_.onClick = [this]
    {
        auto name = cmbDevice_.getText();
        if (name.isNotEmpty()) p_.openDevice (name);
    };
    pg.addAndMakeVisible (btnConnect_);

    btnDisconnect_.setColour (juce::TextButton::buttonColourId,  surf());
    btnDisconnect_.setColour (juce::TextButton::textColourOffId,
                               juce::Colours::white.withAlpha (0.55f));
    btnDisconnect_.onClick = [this] { p_.closeDevice(); };
    pg.addAndMakeVisible (btnDisconnect_);

    styleLabel (lblMidiOutTitle_, "SORTIE MIDI EXTERNE  (loopMIDI / IAC)", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblMidiOutTitle_);

    styleCombo (cmbMidiOut_);
    pg.addAndMakeVisible (cmbMidiOut_);

    btnMidiRefresh_.setColour (juce::TextButton::buttonColourId,  surf());
    btnMidiRefresh_.setColour (juce::TextButton::textColourOffId, cyan());
    btnMidiRefresh_.onClick = [this] { refreshMidiOutputs(); };
    pg.addAndMakeVisible (btnMidiRefresh_);

    btnMidiConnect_.setColour (juce::TextButton::buttonColourId,  cyan().withAlpha (0.15f));
    btnMidiConnect_.setColour (juce::TextButton::textColourOffId, cyan());
    btnMidiConnect_.onClick = [this]
    {
        if (p_.isMidiOutputOpen())
        {
            p_.closeMidiOutput();
        }
        else
        {
            const auto name = cmbMidiOut_.getText();
            if (name.isNotEmpty()) p_.openMidiOutput (name);
        }
    };
    pg.addAndMakeVisible (btnMidiConnect_);

    styleLabel (lblLatencyTitle_, "LATENCE / PRECISION", 9.f,
                juce::Colours::white.withAlpha (0.3f), juce::Justification::left);
    pg.addAndMakeVisible (lblLatencyTitle_);

    cmbLatency_.addItem ("Rapide  (1024)",   1);
    cmbLatency_.addItem ("Equilibre (2048)", 2);
    cmbLatency_.addItem ("Precis  (4096)",   3);
    cmbLatency_.onChange = [this] { p_.latencyMode = cmbLatency_.getSelectedId() - 1; };
    styleCombo (cmbLatency_);
    pg.addAndMakeVisible (cmbLatency_);

    btnCalibrate_.setColour (juce::TextButton::buttonColourId,  cyan().withAlpha (0.15f));
    btnCalibrate_.setColour (juce::TextButton::textColourOffId, cyan());
    btnCalibrate_.onClick = [this] { p_.startCalibration(); };
    pg.addAndMakeVisible (btnCalibrate_);

    styleLabel (lblRange_, "", 9.f, juce::Colours::white.withAlpha (0.45f),
                juce::Justification::left);
    pg.addAndMakeVisible (lblRange_);

    styleLabel (lblSetupHint_,
                "Auto: raha misy audio alefan'ny hote (piste effect na standalone) dia "
                "izay no raisina; raha tsy misy dia ny micro interne. Ny SORTIE MIDI "
                "EXTERNE dia ilaina rehefa mampiasa ny standalone: sokafy loopMIDI, "
                "dia raiso io port io ao amin'ny DAW.",
                9.f, juce::Colours::white.withAlpha (0.4f), juce::Justification::topLeft);
    pg.addAndMakeVisible (lblSetupHint_);

    refreshMidiOutputs();
}

//==============================================================================
void TsengoEditor::refreshMidiOutputs()
{
    cmbMidiOut_.clear (juce::dontSendNotification);
    auto outs = p_.getMidiOutputs();
    for (int i = 0; i < outs.size(); ++i)
        cmbMidiOut_.addItem (outs[i], i + 1);

    const auto saved = p_.getCurrentMidiOutput();
    if (saved.isNotEmpty())
    {
        for (int i = 0; i < cmbMidiOut_.getNumItems(); ++i)
            if (cmbMidiOut_.getItemText (i) == saved)
            {
                cmbMidiOut_.setSelectedItemIndex (i, juce::dontSendNotification);
                return;
            }
    }
    if (cmbMidiOut_.getNumItems() > 0)
        cmbMidiOut_.setSelectedItemIndex (0, juce::dontSendNotification);
}

//==============================================================================
void TsengoEditor::buildCalibrationOverlay()
{
    calOverlay_.onPaint = [this] (juce::Graphics& g)
    {
        const int w = calOverlay_.getWidth();
        const int h = calOverlay_.getHeight();

        g.fillAll (dark().withAlpha (0.94f));
        g.setColour (panel());
        g.fillRoundedRectangle (0.f, 0.f, (float) w, (float) h, 8.f);
        g.setColour (cyan().withAlpha (0.35f));
        g.drawRoundedRectangle (1.f, 1.f, (float) w - 2.f, (float) h - 2.f, 8.f, 1.2f);

        g.setFont (juce::Font ("Courier New", 13.f, juce::Font::bold));
        g.setColour (cyan());
        g.drawText ("CALIBRATION", 0, 24, w, 20, juce::Justification::centred);

        // progress bar
        const int bx = 40, bw = w - 80, by = h / 2 + 20, bh = 14;
        g.setColour (juce::Colour (0xFF050A12));
        g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 5.f);
        g.setColour (cyan());
        g.fillRoundedRectangle ((float) bx, (float) by,
                                 juce::jmax (2.f, (float) bw * dispCalProgress_),
                                 (float) bh, 5.f);

        // live input meter so the singer can see the mic is working
        const float lvl = juce::jlimit (0.f, 1.f, p_.getMicLevel());
        const int   my  = by + 26;
        g.setColour (juce::Colour (0xFF050A12));
        g.fillRoundedRectangle ((float) bx, (float) my, (float) bw, 8.f, 4.f);
        g.setColour (green());
        g.fillRoundedRectangle ((float) bx, (float) my, (float) bw * lvl, 8.f, 4.f);
    };

    addChildComponent (calOverlay_);

    styleLabel (lblCalStep_, "", 14.f, juce::Colours::white);
    calOverlay_.addAndMakeVisible (lblCalStep_);

    styleLabel (lblCalHint_,
                "Ny calibration dia mamaritra ny tabataba manodidina, ny gain, "
                "ary ny halavan'ny feonao (ambany <-> avo). Izany no mahatonga ny "
                "fitiliana nota ho marina kokoa.",
                9.f, juce::Colours::white.withAlpha (0.45f), juce::Justification::centredTop);
    calOverlay_.addAndMakeVisible (lblCalHint_);

    btnCalCancel_.setColour (juce::TextButton::buttonColourId,  surf());
    btnCalCancel_.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.6f));
    btnCalCancel_.onClick = [this] { p_.cancelCalibration(); };
    calOverlay_.addAndMakeVisible (btnCalCancel_);
}

//==============================================================================
void TsengoEditor::refreshDevices()
{
    cmbDevice_.clear (juce::dontSendNotification);
    auto devs = p_.getInputDevices();
    for (int i = 0; i < devs.size(); ++i)
        cmbDevice_.addItem (devs[i], i + 1);

    const auto saved = p_.getCurrentDevice();
    if (saved.isNotEmpty())
    {
        for (int i = 0; i < cmbDevice_.getNumItems(); ++i)
        {
            if (cmbDevice_.getItemText (i) == saved)
            {
                cmbDevice_.setSelectedItemIndex (i, juce::dontSendNotification);
                return;
            }
        }
    }
    if (cmbDevice_.getNumItems() > 0)
        cmbDevice_.setSelectedItemIndex (0, juce::dontSendNotification);
}

//==============================================================================
void TsengoEditor::syncFromProcessor()
{
    sldThreshold_ .setValue (p_.threshold.load(), juce::dontSendNotification);
    sldSmoothing_ .setValue (p_.smoothing.load(), juce::dontSendNotification);
    sldGain_      .setValue (p_.gain.load(),      juce::dontSendNotification);
    sldStickiness_.setValue (p_.stickiness.load(), juce::dontSendNotification);
    sldMonitorVol_.setValue (p_.monitorVolume.load(), juce::dontSendNotification);

    cmbOctave_.setSelectedId (p_.octaveShift.load() + 3, juce::dontSendNotification);
    cmbSource_.setSelectedId (juce::jlimit (0, 2, p_.inputSource.load()) + 1,
                               juce::dontSendNotification);
    btnMonitor_.setToggleState (p_.monitorSynthEnabled.load(), juce::dontSendNotification);

    btnTriggers_.setToggleState (p_.triggers().enabled.load(), juce::dontSendNotification);
    sldTrigSens_  .setValue (p_.triggers().sensitivity.load(), juce::dontSendNotification);
    sldTrigStrict_.setValue (p_.triggers().strictness.load(),  juce::dontSendNotification);

    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
        padNoteCombos_[(size_t) i].setSelectedId (p_.padNote[i].load() + 1,
                                                   juce::dontSendNotification);

    cmbKeyRoot_.setSelectedId (p_.keyRoot.load() + 1,   juce::dontSendNotification);
    cmbScale_  .setSelectedId (p_.scaleType.load() + 1, juce::dontSendNotification);
    btnQuantize_.setToggleState (p_.quantizeToKey.load(), juce::dontSendNotification);

    btnBend_.setToggleState (p_.pitchBendEnabled.load(), juce::dontSendNotification);
    cmbBendRange_.setSelectedId (juce::jlimit (1, 12, p_.pitchBendRangeSemitones.load()),
                                  juce::dontSendNotification);

    btnTimeQ_.setToggleState (p_.timeQuantizeEnabled.load(), juce::dontSendNotification);
    {
        const int d = p_.timeQuantizeDivision.load();
        const int id = d <= 1 ? 1 : (d == 2 ? 2 : (d == 4 ? 3 : 4));
        cmbTimeDiv_.setSelectedId (id, juce::dontSendNotification);
    }

    cmbLatency_.setSelectedId (juce::jlimit (0, 2, p_.latencyMode.load()) + 1,
                                juce::dontSendNotification);

    btnChords_.setToggleState (p_.chordsEnabled.load(), juce::dontSendNotification);
    cmbChordType_.setSelectedId (p_.chordType.load() + 1, juce::dontSendNotification);

    cmbCcAaa_.setSelectedId (juce::jlimit (1, 119, p_.ccAaa.load()), juce::dontSendNotification);
    cmbCcEee_.setSelectedId (juce::jlimit (1, 119, p_.ccEee.load()), juce::dontSendNotification);
    cmbCcOoo_.setSelectedId (juce::jlimit (1, 119, p_.ccOoo.load()), juce::dontSendNotification);
    cmbCcEnv_.setSelectedId (juce::jlimit (1, 119, p_.ccEnv.load()), juce::dontSendNotification);

    cmbChanPitch_.setSelectedId (juce::jlimit (1, 16, p_.pitchChannel.load()),
                                  juce::dontSendNotification);
    cmbChanTrig_ .setSelectedId (juce::jlimit (1, 16, p_.triggerChannel.load()),
                                  juce::dontSendNotification);

    lastStateRev_ = p_.stateRevision.load();
    updateTriggerUI();
}

//==============================================================================
void TsengoEditor::updateTriggerUI()
{
    auto& tr = p_.triggers();
    const int training = tr.getTrainingPad();

    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
    {
        const size_t idx = (size_t) i;
        const int    n   = tr.getExampleCount (i);

        padTrainBtns_[idx].setButtonText (training == i ? "STOP" : "TRAIN");
        padTrainBtns_[idx].setColour (juce::TextButton::textColourOffId,
                                       training == i ? juce::Colours::white : orange());

        padCountLabels_[idx].setText (juce::String (n) + " hits", juce::dontSendNotification);
        padCountLabels_[idx].setColour (juce::Label::textColourId,
                                         tr.isPadTrained (i) ? green()
                                                             : juce::Colours::white.withAlpha (0.35f));
    }

    if (training >= 0)
        lblTrigStatus_.setText ("APPRENTISSAGE  " + TsengoProcessor::defaultPadName (training)
                                 + " — avereno ilay feo",
                                 juce::dontSendNotification);
    else
        lblTrigStatus_.setText ({}, juce::dontSendNotification);
}

//==============================================================================
void TsengoEditor::timerCallback()
{
    if (p_.stateRevision.load() != lastStateRev_)
        syncFromProcessor();

    dispMic_  += (p_.getMicLevel()  - dispMic_)  * 0.2f;
    dispMidi_ += (p_.getMidiLevel() - dispMidi_) * 0.15f;
    dispAaa_  += (p_.getVowelAAA()  - dispAaa_)  * 0.3f;
    dispEee_  += (p_.getVowelEEE()  - dispEee_)  * 0.3f;
    dispOoo_  += (p_.getVowelOOO()  - dispOoo_)  * 0.3f;
    dispEnv_  += (p_.getVowelEnv()  - dispEnv_)  * 0.3f;

    // ---- PLAY read-outs ----
    const float f1 = p_.getFormant1(), f2 = p_.getFormant2();
    if (f1 > 0.f && f2 > 0.f)
        lblFormants_.setText ("F1 " + juce::String (juce::roundToInt (f1)) + " Hz   F2 "
                               + juce::String (juce::roundToInt (f2)) + " Hz",
                               juce::dontSendNotification);
    else
        lblFormants_.setText ("F1 -- Hz   F2 -- Hz", juce::dontSendNotification);

    const int   note = p_.getNote();
    const float hz   = p_.getPitchHz();
    const float conf = p_.getConfidence();

    if (note >= 0)
    {
        lblNote_.setText (juce::String (noteName (note)) + juce::String (note / 12 - 1),
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

    // ---- SETUP page read-outs ----
    {
        const bool host = p_.isUsingHostInput();
        const bool mic  = p_.isDeviceOpen();
        lblSourceState_.setText (host ? "actif : entree de l'hote"
                                      : (mic ? "actif : micro interne"
                                             : "aucune source — connectez un micro"),
                                  juce::dontSendNotification);
        lblSourceState_.setColour (juce::Label::textColourId,
                                    (host || mic) ? green() : orange());

        btnMidiConnect_.setButtonText (p_.isMidiOutputOpen() ? "FERMER" : "OUVRIR");
    }

    // ---- range read-out ----
    lblRange_.setText ("Etendue vocale : " + juce::String (juce::roundToInt (p_.getPitchMinHz()))
                        + " Hz  ->  " + juce::String (juce::roundToInt (p_.getPitchMaxHz()))
                        + " Hz     (bruit " + juce::String (p_.getNoiseFloor(), 3) + ")",
                        juce::dontSendNotification);

    // ---- TRIGGERS flash + counters ----
    const int hits = p_.triggers().getLastHitCounter();
    if (hits != lastHitSeen_)
    {
        lastHitSeen_ = hits;
        flashPad_    = p_.triggers().getLastPad();
        flashAmount_ = 1.f;
        updateTriggerUI();
    }
    else
    {
        flashAmount_ *= 0.82f;
    }

    if (p_.triggers().getTrainingPad() >= 0 && currentTab_ == TabTriggers)
        updateTriggerUI();

    // ---- Calibration overlay ----
    const int calStage = p_.getCalStage();
    const bool showCal = (calStage != TsengoProcessor::CalIdle);
    if (showCal != calOverlay_.isVisible())
    {
        calOverlay_.setVisible (showCal);
        if (showCal) calOverlay_.toFront (false);
    }
    if (showCal)
    {
        dispCalProgress_ += (p_.getCalProgress() - dispCalProgress_) * 0.4f;
        lblCalStep_.setText (p_.getCalInstruction(), juce::dontSendNotification);
    }
    else
    {
        dispCalProgress_ = 0.f;
    }

    repaint();
}

//==============================================================================
void TsengoEditor::paint (juce::Graphics& g)
{
    const int W = getWidth();

    g.fillAll (dark());

    // Header
    g.setColour (panel());
    g.fillRect (0, 0, W, kHeaderH);
    g.setColour (cyan().withAlpha (0.12f));
    g.fillRect (0, kHeaderH - 1, W, 1);

    g.setFont (juce::Font ("Courier New", 15.f, juce::Font::bold));
    g.setColour (cyan());
    g.drawText ("TSENGO  VOICE  SYNTH  v4.0", 0, 0, W, kHeaderH, juce::Justification::centred);

    const bool connected = p_.isDeviceOpen();
    g.setColour (connected ? green() : juce::Colours::grey);
    g.fillEllipse (14.f, 18.f, 9.f, 9.f);
    g.setColour ((connected ? green() : juce::Colours::grey).withAlpha (0.35f));
    g.drawEllipse (12.f, 16.f, 13.f, 13.f, 1.2f);
}

//==============================================================================
void TsengoEditor::paintPlayPage (juce::Graphics& g)
{
    auto& pg = pages_[(size_t) TabPlay];
    const int w = pg.getWidth();

    // Note box
    g.setColour (panel());
    g.fillRoundedRectangle (0.f, 0.f, 230.f, 80.f, 7.f);
    g.setColour (cyan().withAlpha (0.15f));
    g.drawRoundedRectangle (0.f, 0.f, 230.f, 80.f, 7.f, 1.f);
    g.setFont (juce::Font ("Courier New", 9.f, juce::Font::plain));
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawText ("NOTA DETEKTEE", 8, 3, 180, 13, juce::Justification::left);

    // Meters
    const int mx = w - 60, my = 0, mw = 16, mh = 80;
    g.setColour (juce::Colour (0xFF050A12));
    g.fillRoundedRectangle ((float) mx,      (float) my, (float) mw, (float) mh, 4.f);
    g.fillRoundedRectangle ((float) mx + 24, (float) my, (float) mw, (float) mh, 4.f);

    const float micH  = juce::jlimit (0.f, (float) mh, dispMic_  * (float) mh);
    const float midiH = juce::jlimit (0.f, (float) mh, dispMidi_ * (float) mh);
    g.setColour (green());
    if (micH  > 0.f) g.fillRoundedRectangle ((float) mx, (float) (my + mh) - micH, (float) mw, micH, 3.f);
    g.setColour (cyan());
    if (midiH > 0.f) g.fillRoundedRectangle ((float) mx + 24, (float) (my + mh) - midiH, (float) mw, midiH, 3.f);

    g.setFont (juce::Font ("Courier New", 8.f, juce::Font::plain));
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawText ("MIC",  mx,      my + mh + 3, mw,     10, juce::Justification::centred);
    g.drawText ("MIDI", mx + 22, my + mh + 3, mw + 4, 10, juce::Justification::centred);

    // Settings panel
    g.setColour (panel());
    g.fillRoundedRectangle (0.f, 88.f, (float) w, 110.f, 7.f);
    g.setColour (cyan().withAlpha (0.1f));
    g.drawRoundedRectangle (0.f, 88.f, (float) w, 110.f, 7.f, 1.f);

    // Vowel panel
    const float vy = 248.f, vh = 158.f;
    g.setColour (panel());
    g.fillRoundedRectangle (0.f, vy, (float) w, vh, 7.f);
    g.setColour (cyan().withAlpha (0.1f));
    g.drawRoundedRectangle (0.f, vy, (float) w, vh, 7.f, 1.f);

    const int barW = 30, barH = 90, gap = 34;
    const int barY = (int) vy + 30;
    const int totalW = barW * 4 + gap * 3;
    const int barX0  = (w - totalW) / 2;

    const juce::Colour barCols[4] = { orange(), purple(), green(), yellow() };
    const float        barVals[4] = { dispAaa_, dispEee_, dispOoo_, dispEnv_ };

    for (int i = 0; i < 4; ++i)
    {
        const int bx = barX0 + i * (barW + gap);
        g.setColour (juce::Colour (0xFF050A12));
        g.fillRoundedRectangle ((float) bx, (float) barY, (float) barW, (float) barH, 5.f);

        const float h = juce::jlimit (0.f, (float) barH, barVals[i] * (float) barH);
        if (h > 0.f)
        {
            g.setColour (barCols[i]);
            g.fillRoundedRectangle ((float) bx, (float) (barY + barH) - h,
                                     (float) barW, h, 4.f);
        }
    }
}

//==============================================================================
void TsengoEditor::resized()
{
    const int W = getWidth(), H = getHeight();

    const int tabW = (W - 8) / kNumTabs;
    for (int i = 0; i < kNumTabs; ++i)
        tabBtns_[(size_t) i].setBounds (4 + i * tabW, kHeaderH + 2, tabW - 2, kTabH - 4);

    const juce::Rectangle<int> content (kContentX, kContentY,
                                         W - kContentX * 2, H - kContentY - 12);

    for (int i = 0; i < kNumTabs; ++i)
        pages_[(size_t) i].setBounds (content);

    calOverlay_.setBounds (content);
    {
        const int w = content.getWidth(), h = content.getHeight();
        lblCalStep_.setBounds (20, h / 2 - 40, w - 40, 30);
        lblCalHint_.setBounds (30, h / 2 - 4, w - 60, 54);
        btnCalCancel_.setBounds (w / 2 - 60, h - 60, 120, 28);
    }

    const juce::Rectangle<int> local (0, 0, content.getWidth(), content.getHeight());
    layoutPlayPage    (local);
    layoutTriggerPage (local);
    layoutKeyPage     (local);
    layoutChordPage   (local);
    layoutAssignPage  (local);
    layoutSetupPage   (local);
}

//==============================================================================
void TsengoEditor::layoutPlayPage (juce::Rectangle<int> r)
{
    const int w = r.getWidth();

    lblNote_.setBounds (8,   14, 130, 48);
    lblHz_  .setBounds (140, 18, 84,  18);
    lblConf_.setBounds (140, 38, 84,  18);

    const int ky = 102, kw = 56;
    sldThreshold_.setBounds (14,             ky, kw, kw);
    sldSmoothing_.setBounds (14 + kw + 14,   ky, kw, kw);
    sldGain_     .setBounds (14 + (kw+14)*2, ky, kw, kw);
    lblThr_ .setBounds (14,             ky + kw, kw, 12);
    lblSmo_ .setBounds (14 + kw + 14,   ky + kw, kw, 12);
    lblGain_.setBounds (14 + (kw+14)*2, ky + kw, kw, 12);

    const int rx = 240;
    cmbOctave_    .setBounds (rx,       100, 96, 24);
    lblOctave_    .setBounds (rx + 102, 100, 80, 24);
    lblStick_     .setBounds (rx,       130, 96, 20);
    sldStickiness_.setBounds (rx + 102, 134, w - rx - 110, 14);
    btnMonitor_   .setBounds (rx,       158, 96, 24);
    sldMonitorVol_.setBounds (rx + 102, 162, w - rx - 148, 14);
    lblMonVol_    .setBounds (w - 40,   158, 40, 24);

    const int vy = 248, barW = 30, barH = 90, gap = 34;
    const int barY   = vy + 30;
    const int totalW = barW * 4 + gap * 3;
    const int barX0  = (w - totalW) / 2;

    lblVowelTitle_.setBounds (10, vy + 6, 220, 12);
    lblFormants_  .setBounds (w - 190, vy + 6, 180, 12);

    lblAaa_.setBounds (barX0,                     barY + barH + 4, barW, 14);
    lblEee_.setBounds (barX0 + (barW + gap),      barY + barH + 4, barW, 14);
    lblOoo_.setBounds (barX0 + (barW + gap) * 2,  barY + barH + 4, barW, 14);
    lblEnv_.setBounds (barX0 + (barW + gap) * 3,  barY + barH + 4, barW, 14);
}

//==============================================================================
void TsengoEditor::layoutTriggerPage (juce::Rectangle<int> r)
{
    const int w = r.getWidth();

    btnTriggers_  .setBounds (10, 8, 140, 28);
    lblTrigStatus_.setBounds (158, 8, w - 168, 28);

    lblTrigSens_  .setBounds (10,  42, 90, 16);
    sldTrigSens_  .setBounds (104, 44, 130, 14);
    lblTrigStrict_.setBounds (250, 42, 80, 16);
    sldTrigStrict_.setBounds (334, 44, w - 344, 14);

    lblTrigHint_.setBounds (10, 60, w - 20, 32);

    for (int i = 0; i < TriggerEngine::kNumPads; ++i)
    {
        const size_t idx = (size_t) i;
        const int    y   = 100 + i * 40;

        padLabels_[idx]     .setBounds (24,  y,     56, 34);
        padNoteCombos_[idx] .setBounds (84,  y + 4, 130, 26);
        padTrainBtns_[idx]  .setBounds (222, y + 4, 90,  26);
        padCountLabels_[idx].setBounds (318, y,     80,  34);
        padClearBtns_[idx]  .setBounds (w - 84, y + 4, 74, 26);
    }
}

//==============================================================================
void TsengoEditor::layoutKeyPage (juce::Rectangle<int> r)
{
    const int w = r.getWidth();

    lblKeyTitle_.setBounds (0, 0, 240, 14);
    cmbKeyRoot_ .setBounds (0,   18, 80,  26);
    cmbScale_   .setBounds (88,  18, 140, 26);
    btnQuantize_.setBounds (236, 18, 150, 26);

    lblBendTitle_.setBounds (0, 60, 300, 14);
    btnBend_     .setBounds (0,   78, 150, 26);
    cmbBendRange_.setBounds (158, 78, 120, 26);

    lblTimeTitle_.setBounds (0, 120, 300, 14);
    btnTimeQ_    .setBounds (0,   138, 150, 26);
    cmbTimeDiv_  .setBounds (158, 138, 100, 26);

    juce::ignoreUnused (w);
}

//==============================================================================
void TsengoEditor::layoutSetupPage (juce::Rectangle<int> r)
{
    const int w = r.getWidth();

    lblSourceTitle_.setBounds (0, 0, 240, 14);
    cmbSource_     .setBounds (0, 18, 200, 26);
    lblSourceState_.setBounds (210, 18, w - 210, 26);

    lblMicTitle_ .setBounds (0, 58, 360, 14);
    cmbDevice_   .setBounds (0,   76, 300, 26);
    btnRefresh_  .setBounds (306, 76, 30,  26);
    btnConnect_  .setBounds (342, 76, 100, 26);
    btnDisconnect_.setBounds (0, 108, 150, 24);

    lblMidiOutTitle_.setBounds (0, 146, 360, 14);
    cmbMidiOut_    .setBounds (0,   164, 300, 26);
    btnMidiRefresh_.setBounds (306, 164, 30,  26);
    btnMidiConnect_.setBounds (342, 164, 100, 26);

    lblLatencyTitle_.setBounds (0, 204, 300, 14);
    cmbLatency_     .setBounds (0, 222, 200, 26);

    btnCalibrate_.setBounds (0, 260, 230, 30);
    lblRange_    .setBounds (0, 298, w, 20);
    lblSetupHint_.setBounds (0, 326, w, 70);
}

//==============================================================================
void TsengoEditor::layoutChordPage (juce::Rectangle<int> r)
{
    const int w = r.getWidth();

    lblChordTitle_.setBounds (0, 0, 240, 14);
    btnChords_    .setBounds (0,   20, 150, 28);
    cmbChordType_ .setBounds (158, 20, 160, 28);
    lblChordInfo_ .setBounds (0,   68, w, 180);
}

//==============================================================================
void TsengoEditor::layoutAssignPage (juce::Rectangle<int> r)
{
    lblAssignTitle_.setBounds (0, 0, 240, 14);

    lblCcAaa_.setBounds (0, 20,  70, 26);  cmbCcAaa_.setBounds (78, 20,  140, 26);
    lblCcEee_.setBounds (0, 54,  70, 26);  cmbCcEee_.setBounds (78, 54,  140, 26);
    lblCcOoo_.setBounds (0, 88,  70, 26);  cmbCcOoo_.setBounds (78, 88,  140, 26);
    lblCcEnv_.setBounds (0, 122, 70, 26);  cmbCcEnv_.setBounds (78, 122, 140, 26);

    lblChanTitle_.setBounds (0, 168, 240, 14);
    lblChanPitch_.setBounds (0,   188, 150, 26);  cmbChanPitch_.setBounds (158, 188, 100, 26);
    lblChanTrig_ .setBounds (0,   222, 150, 26);  cmbChanTrig_ .setBounds (158, 222, 100, 26);

    juce::ignoreUnused (r);
}
