#pragma once
#include <JuceHeader.h>

/**
 * Crystal dark-cyan aesthetic — custom JUCE LookAndFeel.
 * Used by the plugin editor for all sliders, buttons, combo boxes.
 */
class CrystalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrystalLookAndFeel();

    // ── Colours ────────────────────────────────────────────────────────────
    static constexpr uint32_t colCyan      = 0xFF00D4FF;
    static constexpr uint32_t colCyanDim   = 0x6600D4FF;
    static constexpr uint32_t colBg        = 0xFF05080F;
    static constexpr uint32_t colBgMid     = 0xFF090E1C;
    static constexpr uint32_t colGreen     = 0xFF00E676;
    static constexpr uint32_t colOrange    = 0xFFFF6D00;
    static constexpr uint32_t colText      = 0xFFC8EEFF;

    // ── Overrides ──────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                                const juce::Colour& bg,
                                bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down,
                       int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

private:
    juce::Font monoFont_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrystalLookAndFeel)
};
