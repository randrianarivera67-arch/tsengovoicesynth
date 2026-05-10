#include "PluginEditor.h"

//==============================================================================
VoiceSynthEditor::VoiceSynthEditor(VoiceSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 280);

    // Title
    titleLabel.setText("TSENGO VOICE SYNTH", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF00E5FF));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Note courante
    noteLabel.setText("Tsindrio Key...", juce::dontSendNotification);
    noteLabel.setFont(juce::Font(18.0f));
    noteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    noteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(noteLabel);

    // Level labels
    inLevelLabel.setText("MIC: ---", juce::dontSendNotification);
    inLevelLabel.setFont(juce::Font(13.0f));
    inLevelLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(inLevelLabel);

    outLevelLabel.setText("OUT: ---", juce::dontSendNotification);
    outLevelLabel.setFont(juce::Font(13.0f));
    outLevelLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFFF9800));
    addAndMakeVisible(outLevelLabel);

    startTimerHz(30); // refresh 30fps
}

VoiceSynthEditor::~VoiceSynthEditor()
{
    stopTimer();
}

//==============================================================================
void VoiceSynthEditor::timerCallback()
{
    // Manavao level meters sy nota
    displayInputLevel  = processor.getInputLevel();
    displayOutputLevel = processor.getOutputLevel();

    int note = processor.getCurrentNote();
    if (note >= 0)
    {
        int octave = note / 12 - 1;
        juce::String txt = juce::String(noteName(note)) + juce::String(octave)
                         + "  (MIDI " + juce::String(note) + ")";
        noteLabel.setText(txt, juce::dontSendNotification);
        noteLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF00E5FF));
    }
    else
    {
        noteLabel.setText("Tsindrio Key...", juce::dontSendNotification);
        noteLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }

    inLevelLabel.setText("MIC: " + juce::String(int(displayInputLevel * 100)) + "%",
                         juce::dontSendNotification);
    outLevelLabel.setText("OUT: " + juce::String(int(displayOutputLevel * 100)) + "%",
                          juce::dontSendNotification);

    repaint();
}

//==============================================================================
void VoiceSynthEditor::paint(juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient bg(juce::Colour(0xFF0A0A1A), 0, 0,
                            juce::Colour(0xFF0D2040), (float)getWidth(), (float)getHeight(),
                            false);
    g.setGradientFill(bg);
    g.fillAll();

    // Border
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.3f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4), 10.0f, 1.5f);

    // ---- MIC level bar ----
    int barY   = 160;
    int barX   = 30;
    int barW   = 160;
    int barH   = 18;

    g.setColour(juce::Colours::darkgrey.darker());
    g.fillRoundedRectangle((float)barX, (float)barY, (float)barW, (float)barH, 4.0f);

    g.setColour(juce::Colours::lightgreen);
    g.fillRoundedRectangle((float)barX, (float)barY,
                           (float)(barW * displayInputLevel), (float)barH, 4.0f);

    // ---- OUTPUT level bar ----
    int bar2X = 210;
    g.setColour(juce::Colours::darkgrey.darker());
    g.fillRoundedRectangle((float)bar2X, (float)barY, (float)barW, (float)barH, 4.0f);

    g.setColour(juce::Colour(0xFFFF9800));
    g.fillRoundedRectangle((float)bar2X, (float)barY,
                           (float)(barW * displayOutputLevel), (float)barH, 4.0f);

    // ---- Piano keys (déco) ----
    int keyY  = 210;
    int keyH  = 50;
    int keyW  = 28;
    int startX = 20;
    int noteActive = processor.getCurrentNote();

    // White keys: C D E F G A B
    int whiteNotes[] = {60,62,64,65,67,69,71,72,74,76,77,79,81,83};
    for (int k = 0; k < 14; ++k)
    {
        bool active = (whiteNotes[k] == noteActive);
        g.setColour(active ? juce::Colour(0xFF00E5FF) : juce::Colours::white);
        g.fillRoundedRectangle((float)(startX + k * (keyW+2)), (float)keyY,
                               (float)keyW, (float)keyH, 3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawRoundedRectangle((float)(startX + k * (keyW+2)), (float)keyY,
                               (float)keyW, (float)keyH, 3.0f, 1.0f);
    }

    // Black keys: C# D# F# G# A#
    int blackOffsets[] = {0,1,3,4,5, 7,8,10,11,12};
    int blackNotes[]   = {61,63,66,68,70, 73,75,78,80,82};
    for (int k = 0; k < 10; ++k)
    {
        bool active = (blackNotes[k] == noteActive);
        int bx = startX + blackOffsets[k] * (keyW+2) + keyW/2;
        g.setColour(active ? juce::Colour(0xFF0080FF) : juce::Colours::black);
        g.fillRoundedRectangle((float)bx, (float)keyY,
                               (float)(keyW * 0.65f), (float)(keyH * 0.6f), 2.0f);
    }
}

//==============================================================================
void VoiceSynthEditor::resized()
{
    titleLabel .setBounds(10,  10, getWidth()-20, 34);
    noteLabel  .setBounds(10,  52, getWidth()-20, 30);
    inLevelLabel .setBounds(30,  140, 160, 20);
    outLevelLabel.setBounds(210, 140, 160, 20);
}
