#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sampr
{

class SamprLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SamprLookAndFeel();

    static juce::Colour background() noexcept { return juce::Colour (0xff101216); }
    static juce::Colour panel() noexcept { return juce::Colour (0xff181b22); }
    static juce::Colour surface() noexcept { return juce::Colour (0xff242934); }
    static juce::Colour border() noexcept { return juce::Colour (0xff343b49); }
    static juce::Colour divider() noexcept { return juce::Colour (0xff465064); }
    static juce::Colour accent() noexcept { return juce::Colour (0xffffa21a); }
    static juce::Colour accentCool() noexcept { return juce::Colour (0xff35c7ff); }
    static juce::Colour success() noexcept { return juce::Colour (0xff68d86d); }
    static juce::Colour danger() noexcept { return juce::Colour (0xffff4f63); }
    static juce::Colour transport() noexcept { return juce::Colour (0xff1e232c); }
    static juce::Colour textMuted() noexcept { return juce::Colour (0xff9aa3b4); }
    static juce::Colour textPrimary() noexcept { return juce::Colour (0xffeef2f8); }

    static void paintPanelOutline (juce::Graphics& g, juce::Rectangle<int> bounds);
    static void paintHorizontalDivider (juce::Graphics& g, int y, int width);
    static void paintVerticalDivider (juce::Graphics& g, int x, int y, int height);

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawTickBox (juce::Graphics& g,
                      juce::Component& component,
                      float x,
                      float y,
                      float w,
                      float h,
                      bool ticked,
                      bool isEnabled,
                      bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

    void drawTabButton (juce::TabBarButton& button,
                        juce::Graphics& g,
                        bool isMouseOver,
                        bool isMouseDown) override;

    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar,
                                       juce::Graphics& g,
                                       int w,
                                       int h) override;

    void drawComboBox (juce::Graphics& g,
                       int width,
                       int height,
                       bool isButtonDown,
                       int buttonX,
                       int buttonY,
                       int buttonW,
                       int buttonH,
                       juce::ComboBox& box) override;

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator,
                            bool isActive,
                            bool isHighlighted,
                            bool isTicked,
                            bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override;

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    int getDefaultScrollbarWidth() override;

    void drawScrollbar (juce::Graphics& g,
                        juce::ScrollBar& scrollbar,
                        int x,
                        int y,
                        int width,
                        int height,
                        bool isScrollbarVertical,
                        int thumbStartPosition,
                        int thumbSize,
                        bool isMouseOver,
                        bool isMouseDown) override;

    void fillTextEditorBackground (juce::Graphics& g,
                                   int width,
                                   int height,
                                   juce::TextEditor& editor) override;

    void drawTextEditorOutline (juce::Graphics& g,
                                int width,
                                int height,
                                juce::TextEditor& editor) override;

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;

private:
    void initColours();
};

} // namespace sampr
