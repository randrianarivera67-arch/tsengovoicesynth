#include "CrystalLookAndFeel.h"

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using juce::jmap;
using juce::MathConstants;

CrystalLookAndFeel::CrystalLookAndFeel()
{
    monoFont_ = juce::Font ("Courier New", 10.0f, juce::Font::plain);

    setColour (juce::Slider::rotarySliderFillColourId,   Colour (colCyan));
    setColour (juce::Slider::rotarySliderOutlineColourId, Colour (colBgMid));
    setColour (juce::Slider::thumbColourId,               Colour (colCyan));
    setColour (juce::Slider::trackColourId,               Colour (colCyanDim));
    setColour (juce::Label::textColourId,                 Colour (colText));
    setColour (juce::ComboBox::backgroundColourId,        Colour (colBgMid));
    setColour (juce::ComboBox::textColourId,              Colour (colText));
    setColour (juce::ComboBox::outlineColourId,           Colour (colCyanDim));
    setColour (juce::ComboBox::arrowColourId,             Colour (colCyan));
    setColour (juce::PopupMenu::backgroundColourId,       Colour (colBgMid));
    setColour (juce::PopupMenu::textColourId,             Colour (colText));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (colCyan).withAlpha (0.2f));
    setColour (juce::TextButton::buttonColourId,          Colour (colBgMid));
    setColour (juce::TextButton::buttonOnColourId,        Colour (colCyan).withAlpha (0.18f));
    setColour (juce::TextButton::textColourOffId,         Colour (colCyan).withAlpha (0.45f));
    setColour (juce::TextButton::textColourOnId,          Colour (colCyan));
}

// ─────────────────────────────────────────────────────────────────────────────
void CrystalLookAndFeel::drawRotarySlider (Graphics& g,
                                            int x, int y, int w, int h,
                                            float sliderPos,
                                            float startAngle, float endAngle,
                                            juce::Slider& slider)
{
    const float cx    = x + w * 0.5f;
    const float cy    = y + h * 0.5f;
    const float r     = std::min (w, h) * 0.5f - 4.0f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Background disc
    g.setColour (Colour (colBgMid));
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);

    // Outer ring
    g.setColour (Colour (colCyan).withAlpha (0.1f));
    juce::Path ring;
    ring.addEllipse (cx - r, cy - r, r * 2, r * 2);
    g.strokePath (ring, juce::PathStrokeType (0.8f));

    // Track arc
    juce::Path track;
    track.addArc (cx - r + 5, cy - r + 5, (r - 5) * 2, (r - 5) * 2,
                  startAngle, endAngle, true);
    g.setColour (Colour (colCyan).withAlpha (0.08f));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc
    juce::Path arc;
    arc.addArc (cx - r + 5, cy - r + 5, (r - 5) * 2, (r - 5) * 2,
                startAngle, angle, true);
    g.setColour (Colour (colCyan));
    g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Dot indicator
    const float dotR  = r - 10.0f;
    const float dotX  = cx + dotR * std::cos (angle);
    const float dotY  = cy + dotR * std::sin (angle);
    g.setColour (Colour (colCyan));
    g.fillEllipse (dotX - 3, dotY - 3, 6, 6);

    // Centre value text
    g.setColour (Colour (colCyan).withAlpha (0.6f));
    g.setFont (monoFont_.withHeight (9.0f).boldened());
    const int pct = juce::roundToInt (sliderPos * 100);
    g.drawText (juce::String (pct), (int)(cx - 14), (int)(cy - 5), 28, 10,
                juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────────
void CrystalLookAndFeel::drawLinearSlider (Graphics& g,
                                            int x, int y, int w, int h,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style,
                                            juce::Slider&)
{
    if (style == juce::Slider::LinearVertical)
    {
        const int trackX = x + w / 2 - 2;
        g.setColour (Colour (colCyan).withAlpha (0.08f));
        g.fillRoundedRectangle ((float)trackX, (float)y, 4.0f, (float)h, 2.0f);

        g.setColour (Colour (colCyan));
        const float fillH = h - (sliderPos - y);
        g.fillRoundedRectangle ((float)trackX, sliderPos, 4.0f, fillH, 2.0f);

        // Thumb
        g.setColour (Colour (colCyan));
        g.fillEllipse ((float)(x + w / 2) - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void CrystalLookAndFeel::drawButtonBackground (Graphics& g, juce::Button& btn,
                                                const juce::Colour&,
                                                bool highlighted, bool down)
{
    const auto bounds = btn.getLocalBounds().toFloat();
    const bool on     = btn.getToggleState();

    const float alpha = down ? 0.25f : highlighted ? 0.15f : (on ? 0.12f : 0.04f);
    g.setColour (Colour (colCyan).withAlpha (alpha));
    g.fillRoundedRectangle (bounds, 4.0f);

    const float borderAlpha = on ? 0.45f : highlighted ? 0.3f : 0.18f;
    g.setColour (Colour (colCyan).withAlpha (borderAlpha));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 0.8f);
}

void CrystalLookAndFeel::drawButtonText (Graphics& g, juce::TextButton& btn,
                                          bool, bool)
{
    const bool on = btn.getToggleState();
    g.setColour (Colour (colCyan).withAlpha (on ? 1.0f : 0.45f));
    g.setFont (monoFont_.withHeight (9.0f).boldened());
    g.drawText (btn.getButtonText(), btn.getLocalBounds(),
                juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────────────────────────
void CrystalLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool,
                                        int bx, int by, int bw, int bh,
                                        juce::ComboBox&)
{
    const auto bounds = Rectangle<float> (0, 0, (float)w, (float)h);
    g.setColour (Colour (colBgMid));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (Colour (colCyan).withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 0.8f);

    // Arrow
    g.setColour (Colour (colCyan).withAlpha (0.5f));
    juce::Path arrow;
    const float ax = bx + bw * 0.5f;
    const float ay = by + bh * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.fillPath (arrow);
}

// ─────────────────────────────────────────────────────────────────────────────
void CrystalLookAndFeel::drawLabel (Graphics& g, juce::Label& label)
{
    g.fillAll (juce::Colours::transparentBlack);
    g.setColour (Colour (colText).withAlpha (0.38f));
    g.setFont (getLabelFont (label));
    g.drawText (label.getText(), label.getLocalBounds(),
                label.getJustificationType());
}

juce::Font CrystalLookAndFeel::getLabelFont (juce::Label&)
{
    return monoFont_.withHeight (9.0f);
}

juce::Font CrystalLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return monoFont_.withHeight (10.0f);
}
