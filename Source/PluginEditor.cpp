#include "PluginEditor.h"

static void styleBtn(juce::TextButton& b, juce::Colour fg)
{
    b.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFF1C2640));
    b.setColour(juce::TextButton::buttonOnColourId, fg.withAlpha(0.25f));
    b.setColour(juce::TextButton::textColourOffId,  fg.withAlpha(0.5f));
    b.setColour(juce::TextButton::textColourOnId,   fg);
}

static void styleKnob(juce::Slider& s, juce::Colour ac)
{
    s.setColour(juce::Slider::rotarySliderFillColourId,   ac);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, ac.withAlpha(0.2f));
    s.setColour(juce::Slider::thumbColourId,               ac);
    s.setColour(juce::Slider::backgroundColourId,          juce::Colour(0xFF0B0F1A));
}

VoiceSynthEditor::VoiceSynthEditor(VoiceSynthProcessor& proc)
    : AudioProcessorEditor(&proc), p(proc)
{
    setSize(480, 420);

    // Mode
    styleBtn(btnSynth, accent()); btnSynth.setToggleable(true); btnSynth.setClickingTogglesState(false);
    styleBtn(btnMic,   accent()); btnMic  .setToggleable(true); btnMic  .setClickingTogglesState(false);
    btnSynth.setToggleState(true, juce::dontSendNotification);
    btnSynth.onClick = [this]{ p.sourceMode = SourceMode::SYNTH; btnSynth.setToggleState(true,juce::dontSendNotification); btnMic.setToggleState(false,juce::dontSendNotification); };
    btnMic  .onClick = [this]{ p.sourceMode = SourceMode::MIC;   btnMic  .setToggleState(true,juce::dontSendNotification); btnSynth.setToggleState(false,juce::dontSendNotification); refreshDeviceList(); };
    addAndMakeVisible(btnSynth); addAndMakeVisible(btnMic);

    // Osc
    for (auto* b : { &btnSine, &btnSaw, &btnSq, &btnTri })
    { styleBtn(*b, juce::Colour(0xFF7C4DFF)); b->setToggleable(true); b->setClickingTogglesState(false); addAndMakeVisible(*b); }
    btnSine.setToggleState(true, juce::dontSendNotification);
    btnSine.onClick = [this]{ p.oscType=OscType::SINE;     btnSine.setToggleState(true,juce::dontSendNotification); btnSaw.setToggleState(false,juce::dontSendNotification); btnSq.setToggleState(false,juce::dontSendNotification); btnTri.setToggleState(false,juce::dontSendNotification); };
    btnSaw .onClick = [this]{ p.oscType=OscType::SAW;      btnSaw .setToggleState(true,juce::dontSendNotification); btnSine.setToggleState(false,juce::dontSendNotification); btnSq.setToggleState(false,juce::dontSendNotification); btnTri.setToggleState(false,juce::dontSendNotification); };
    btnSq  .onClick = [this]{ p.oscType=OscType::SQUARE;   btnSq  .setToggleState(true,juce::dontSendNotification); btnSine.setToggleState(false,juce::dontSendNotification); btnSaw.setToggleState(false,juce::dontSendNotification); btnTri.setToggleState(false,juce::dontSendNotification); };
    btnTri .onClick = [this]{ p.oscType=OscType::TRIANGLE; btnTri .setToggleState(true,juce::dontSendNotification); btnSine.setToggleState(false,juce::dontSendNotification); btnSaw.setToggleState(false,juce::dontSendNotification); btnSq.setToggleState(false,juce::dontSendNotification); };

    // Device
    lblDevice.setFont(juce::Font(10.0f)); lblDevice.setColour(juce::Label::textColourId, accent().withAlpha(0.6f));
    cmbDevice.setColour(juce::ComboBox::backgroundColourId,    surface());
    cmbDevice.setColour(juce::ComboBox::textColourId,          juce::Colours::white);
    cmbDevice.setColour(juce::ComboBox::outlineColourId,       accent().withAlpha(0.3f));
    cmbDevice.setColour(juce::ComboBox::arrowColourId,         accent());
    cmbDevice.onChange = [this]{
        auto name = cmbDevice.getText();
        if (name.isNotEmpty()) p.openMicDevice(name);
    };
    styleBtn(btnRefresh, accent());
    btnRefresh.onClick = [this]{ refreshDeviceList(); };
    addAndMakeVisible(lblDevice); addAndMakeVisible(cmbDevice); addAndMakeVisible(btnRefresh);

    // Knobs
    sldVolume .setRange(0.0, 1.0, 0.01); sldVolume .setValue(0.8);  styleKnob(sldVolume,  accent());
    sldAttack .setRange(0.001, 2.0, 0.001); sldAttack .setValue(0.02); styleKnob(sldAttack,  juce::Colour(0xFF00BFA5));
    sldRelease.setRange(0.01, 4.0, 0.01);  sldRelease.setValue(0.4);  styleKnob(sldRelease, juce::Colour(0xFFFF6D00));
    sldVolume .onValueChange = [this]{ p.volume  = (float)sldVolume .getValue(); };
    sldAttack .onValueChange = [this]{ p.attack  = (float)sldAttack .getValue(); };
    sldRelease.onValueChange = [this]{ p.release = (float)sldRelease.getValue(); };
    addAndMakeVisible(sldVolume); addAndMakeVisible(sldAttack); addAndMakeVisible(sldRelease);

    for (auto* [l, t] : { std::pair{&lblVol,"VOLUME"}, {&lblAtk,"ATTACK"}, {&lblRel,"RELEASE"} })
    {
        l->setText(t, juce::dontSendNotification);
        l->setFont(juce::Font(9.0f, juce::Font::bold));
        l->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.4f));
        l->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*l);
    }

    // Note display
    lblNote.setFont(juce::Font(26.0f, juce::Font::bold));
    lblNote.setColour(juce::Label::textColourId, accent());
    lblNote.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblNote);

    buildPiano();
    startTimerHz(30);
}

VoiceSynthEditor::~VoiceSynthEditor() { stopTimer(); }

void VoiceSynthEditor::refreshDeviceList()
{
    cmbDevice.clear();
    auto devices = p.getAvailableInputDevices();
    for (int i = 0; i < devices.size(); ++i)
        cmbDevice.addItem(devices[i], i + 1);
    if (cmbDevice.getNumItems() > 0) cmbDevice.setSelectedItemIndex(0);
}

void VoiceSynthEditor::buildPiano()
{
    keys.clear();
    const bool blk[] = {0,1,0,1,0,0,1,0,1,0,1,0};
    const int start = 48, end = 84;
    float wx = 10.0f, ww = 22.0f, gap = 2.0f;
    float whiteX = wx;
    std::vector<std::pair<int,float>> whites;
    for (int m = start; m <= end; ++m)
        if (!blk[m%12]) { whites.push_back({m, whiteX}); whiteX += ww + gap; }
    for (auto& [m, x] : whites) keys.push_back({m, false, x, ww, 52.0f});
    for (int m = start; m <= end; ++m)
    {
        if (!blk[m%12]) continue;
        for (int k = 0; k < (int)whites.size(); ++k)
            if (whites[k].first == m - 1)
            { keys.push_back({m, true, whites[k].second + ww*0.6f, ww*0.6f, 32.0f}); break; }
    }
}

void VoiceSynthEditor::timerCallback()
{
    dispIn  += (p.inputLevel .load() - dispIn)  * 0.2f;
    dispOut += (p.outputLevel.load() - dispOut) * 0.2f;

    int note = p.currentMidiNote.load();
    if (note >= 0)
    {
        static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        lblNote.setText(juce::String(names[note%12]) + juce::String(note/12-1), juce::dontSendNotification);
        lblNote.setColour(juce::Label::textColourId, accent());
    }
    else
    {
        lblNote.setText("—", juce::dontSendNotification);
        lblNote.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
    repaint();
}

void VoiceSynthEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(bg());

    // Header band
    g.setColour(panel());
    g.fillRect(0, 0, getWidth(), 44);
    g.setColour(accent().withAlpha(0.15f));
    g.fillRect(0, 43, getWidth(), 1);

    // Title
    g.setFont(juce::Font("Courier New", 16.0f, juce::Font::bold));
    g.setColour(accent());
    g.drawText("TSENGO  VOICE  SYNTH", 0, 0, getWidth(), 44, juce::Justification::centred);

    // Version dot
    g.setColour(accent());
    g.fillEllipse(14, 17, 8, 8);
    g.setColour(accent().withAlpha(0.4f));
    g.drawEllipse(12, 15, 12, 12, 1.0f);

    // Section: Oscillator
    g.setColour(surface());
    g.fillRoundedRectangle(10, 54, 220, 54, 6);
    g.setColour(accent().withAlpha(0.15f));
    g.drawRoundedRectangle(10, 54, 220, 54, 6, 1.0f);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawText("OSCILLATEUR", 20, 56, 100, 12, juce::Justification::left);

    // Section: Mic device
    g.setColour(surface());
    g.fillRoundedRectangle(10, 116, 350, 50, 6);
    g.setColour(accent().withAlpha(0.15f));
    g.drawRoundedRectangle(10, 116, 350, 50, 6, 1.0f);

    // Meters
    int mx = 370, my = 54, mw = 16, mh = 112;
    g.setColour(juce::Colour(0xFF0B1220));
    g.fillRoundedRectangle((float)mx, (float)my, (float)mw, (float)mh, 4);
    g.setColour(juce::Colour(0xFF0B1220));
    g.fillRoundedRectangle((float)(mx+20), (float)my, (float)mw, (float)mh, 4);

    // Mic meter (green)
    float inH = dispIn * mh;
    g.setColour(juce::Colour(0xFF00C853));
    if (inH > 0) g.fillRoundedRectangle((float)mx, (float)(my + mh - inH), (float)mw, inH, 3);

    // Out meter (orange)
    float outH = dispOut * mh;
    g.setColour(juce::Colour(0xFFFF6D00));
    if (outH > 0) g.fillRoundedRectangle((float)(mx+20), (float)(my + mh - outH), (float)mw, outH, 3);

    g.setFont(juce::Font(8.0f));
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawText("IN",  mx,    my+mh+2, mw,   10, juce::Justification::centred);
    g.drawText("OUT", mx+20, my+mh+2, mw,   10, juce::Justification::centred);

    // Note box
    g.setColour(panel());
    g.fillRoundedRectangle(238, 54, 120, 54, 6);
    g.setColour(accent().withAlpha(0.2f));
    g.drawRoundedRectangle(238, 54, 120, 54, 6, 1.0f);
    g.setFont(juce::Font(9.0f));
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawText("NOTA", 238, 56, 120, 12, juce::Justification::centred);

    // Piano
    int pianoY = getHeight() - 68;
    g.setColour(juce::Colour(0xFF080C14));
    g.fillRect(0, pianoY - 4, getWidth(), 72);
    int activeNote = p.currentMidiNote.load();
    for (auto& k : keys)
    {
        bool active = (k.midi == activeNote);
        if (!k.black)
        {
            g.setColour(active ? accent() : juce::Colours::white.withAlpha(0.92f));
            g.fillRoundedRectangle(k.x, (float)pianoY, k.w, k.h, 3);
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawRoundedRectangle(k.x, (float)pianoY, k.w, k.h, 3, 0.8f);
        }
    }
    for (auto& k : keys)
    {
        bool active = (k.midi == activeNote);
        if (k.black)
        {
            g.setColour(active ? juce::Colour(0xFF0050CC) : juce::Colour(0xFF111827));
            g.fillRoundedRectangle(k.x, (float)pianoY, k.w, k.h, 2);
            if (active) { g.setColour(accent().withAlpha(0.5f)); g.drawRoundedRectangle(k.x, (float)pianoY, k.w, k.h, 2, 1.0f); }
        }
    }
}

void VoiceSynthEditor::resized()
{
    // Mode buttons
    btnSynth.setBounds(10,  6,  70, 30);
    btnMic  .setBounds(84,  6,  70, 30);

    // Osc buttons
    int ox = 20, oy = 70, ow = 44, oh = 24, og = 6;
    btnSine.setBounds(ox,          oy, ow, oh);
    btnSaw .setBounds(ox+ow+og,    oy, ow, oh);
    btnSq  .setBounds(ox+(ow+og)*2,oy, ow, oh);
    btnTri .setBounds(ox+(ow+og)*3,oy, ow, oh);

    // Device
    lblDevice .setBounds(18, 118, 160, 14);
    cmbDevice .setBounds(18, 132, 300, 26);
    btnRefresh.setBounds(322, 132, 30,  26);

    // Knobs
    int ky = 174, kw = 54;
    sldVolume .setBounds(14,         ky, kw, kw);
    sldAttack .setBounds(14+kw+8,    ky, kw, kw);
    sldRelease.setBounds(14+(kw+8)*2,ky, kw, kw);
    lblVol    .setBounds(14,          ky+kw,   kw, 14);
    lblAtk    .setBounds(14+kw+8,     ky+kw,   kw, 14);
    lblRel    .setBounds(14+(kw+8)*2, ky+kw,   kw, 14);

    // Note
    lblNote.setBounds(238, 68, 120, 36);
}
