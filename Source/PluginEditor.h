#pragma once
#include <JuceHeader.h>
#include <functional>
#include <array>
#include "PluginProcessor.h"

//==============================================================================
// A plain container whose painting is supplied by a lambda — lets one editor
// class own several tab pages without a class per page.
class PanelComp : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
};

//==============================================================================
class TsengoEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit TsengoEditor (TsengoProcessor&);
    ~TsengoEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    enum Tab { TabPlay = 0, TabTriggers, TabKey, TabChords, TabAssign, TabSetup, kNumTabs };

private:
    void timerCallback() override;
    void refreshDevices();
    void setTab (int tab);
    void syncFromProcessor();
    void updateTriggerUI();

    void buildPlayPage();
    void buildTriggerPage();
    void buildKeyPage();
    void buildChordPage();
    void buildAssignPage();
    void buildSetupPage();
    void buildCalibrationOverlay();
    void refreshMidiOutputs();

    void layoutPlayPage    (juce::Rectangle<int> r);
    void layoutTriggerPage (juce::Rectangle<int> r);
    void layoutKeyPage     (juce::Rectangle<int> r);
    void layoutChordPage   (juce::Rectangle<int> r);
    void layoutAssignPage  (juce::Rectangle<int> r);
    void layoutSetupPage   (juce::Rectangle<int> r);

    void paintPlayPage (juce::Graphics& g);

    TsengoProcessor& p_;

    //==========================================================================
    // Tabs
    std::array<juce::TextButton, kNumTabs> tabBtns_;
    std::array<PanelComp,        kNumTabs> pages_;
    int currentTab_ { TabPlay };

    //==========================================================================
    // PLAY page
    juce::Label      lblDevice_;
    juce::ComboBox   cmbDevice_;
    juce::TextButton btnRefresh_ { "\xE2\x86\xBA" };
    juce::TextButton btnConnect_ { "CONNECT" };

    juce::Slider sldThreshold_, sldSmoothing_, sldGain_;
    juce::Label  lblThr_, lblSmo_, lblGain_;

    juce::Label lblNote_, lblHz_, lblConf_;

    juce::ComboBox   cmbOctave_;
    juce::Label      lblOctave_, lblStick_;
    juce::Slider     sldStickiness_;
    juce::TextButton btnMonitor_   { "MONITOR" };
    juce::Slider     sldMonitorVol_;
    juce::Label      lblMonVol_;
    juce::TextButton btnCalibrate_ { "CALIBRER LE MICRO" };

    juce::Label lblVowelTitle_, lblAaa_, lblEee_, lblOoo_, lblEnv_, lblFormants_;

    float dispMic_ = 0.f, dispMidi_ = 0.f;
    float dispAaa_ = 0.f, dispEee_ = 0.f, dispOoo_ = 0.f, dispEnv_ = 0.f;

    //==========================================================================
    // TRIGGERS page
    juce::TextButton btnTriggers_ { "TRIGGERS ON" };
    juce::Slider     sldTrigSens_, sldTrigStrict_;
    juce::Label      lblTrigSens_, lblTrigStrict_, lblTrigHint_, lblTrigStatus_;

    std::array<juce::Label,      TriggerEngine::kNumPads> padLabels_;
    std::array<juce::ComboBox,   TriggerEngine::kNumPads> padNoteCombos_;
    std::array<juce::TextButton, TriggerEngine::kNumPads> padTrainBtns_;
    std::array<juce::TextButton, TriggerEngine::kNumPads> padClearBtns_;
    std::array<juce::Label,      TriggerEngine::kNumPads> padCountLabels_;

    int   flashPad_    = -1;
    float flashAmount_ = 0.f;
    int   lastHitSeen_ = 0;

    //==========================================================================
    // KEY page
    juce::ComboBox   cmbKeyRoot_, cmbScale_, cmbTimeDiv_, cmbBendRange_, cmbLatency_;
    juce::TextButton btnQuantize_ { "QUANTIZE" };
    juce::TextButton btnTimeQ_    { "TIME Q" };
    juce::TextButton btnBend_     { "PITCH BEND" };
    juce::Label      lblKeyTitle_, lblBendTitle_, lblTimeTitle_, lblLatencyTitle_, lblRange_;

    //==========================================================================
    // CHORDS page
    juce::TextButton btnChords_ { "CHORDS ON" };
    juce::ComboBox   cmbChordType_;
    juce::Label      lblChordTitle_, lblChordInfo_;

    //==========================================================================
    // ASSIGN page
    juce::Label    lblAssignTitle_, lblCcAaa_, lblCcEee_, lblCcOoo_, lblCcEnv_;
    juce::ComboBox cmbCcAaa_, cmbCcEee_, cmbCcOoo_, cmbCcEnv_;
    juce::Label    lblChanTitle_, lblChanPitch_, lblChanTrig_;
    juce::ComboBox cmbChanPitch_, cmbChanTrig_;

    //==========================================================================
    // SETUP page — input routing, mic device, external MIDI port
    juce::Label      lblSourceTitle_, lblSourceState_, lblMicTitle_, lblMidiOutTitle_,
                     lblSetupHint_;
    juce::ComboBox   cmbSource_, cmbMidiOut_;
    juce::TextButton btnDisconnect_    { "DECONNECTER" };
    juce::TextButton btnMidiRefresh_   { "\xE2\x86\xBA" };
    juce::TextButton btnMidiConnect_   { "OUVRIR" };

    //==========================================================================
    // Calibration overlay
    PanelComp        calOverlay_;
    juce::Label      lblCalStep_, lblCalHint_;
    juce::TextButton btnCalCancel_ { "ANNULER" };
    float dispCalProgress_ = 0.f;

    int lastStateRev_ = -1;

    //==========================================================================
    static void styleToggle (juce::TextButton&, juce::Colour);
    static juce::String noteLabel (int midiNote);

    static const char* noteName (int m)
    {
        static const char* n[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return m >= 0 ? n[m % 12] : "--";
    }

    static juce::Colour cyan()   { return juce::Colour (0xFF00E5FF); }
    static juce::Colour dark()   { return juce::Colour (0xFF080D18); }
    static juce::Colour panel()  { return juce::Colour (0xFF0F1929); }
    static juce::Colour surf()   { return juce::Colour (0xFF172236); }
    static juce::Colour green()  { return juce::Colour (0xFF00C853); }
    static juce::Colour orange() { return juce::Colour (0xFFFF6D00); }
    static juce::Colour purple() { return juce::Colour (0xFF7C4DFF); }
    static juce::Colour yellow() { return juce::Colour (0xFFFFD600); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoEditor)
};
