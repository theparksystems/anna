#include "SamprLookAndFeel.h"

namespace sampr
{

namespace
{
    juce::Rectangle<float> toRect (int x, int y, int w, int h)
    {
        return { static_cast<float> (x), static_cast<float> (y),
                 static_cast<float> (w), static_cast<float> (h) };
    }

    juce::Colour buttonFill (const juce::Button& button, bool highlighted, bool down)
    {
        if (! button.isEnabled())
            return SamprLookAndFeel::surface().withAlpha (0.45f);

        const auto text = button.getButtonText().toLowerCase();

        if (text == "play")
            return down ? SamprLookAndFeel::success().darker (0.15f)
                        : SamprLookAndFeel::success().withAlpha (highlighted ? 0.95f : 0.82f);

        if (text == "record")
            return button.getToggleState() ? SamprLookAndFeel::danger()
                                           : SamprLookAndFeel::surface().brighter (highlighted ? 0.08f : 0.0f);

        if (text == "stop")
            return down ? SamprLookAndFeel::danger().darker (0.25f)
                        : SamprLookAndFeel::danger().withAlpha (highlighted ? 0.82f : 0.55f);

        if (text.contains ("export") || text.contains ("copy"))
            return SamprLookAndFeel::accentCool().withAlpha (down ? 0.55f : (highlighted ? 0.38f : 0.24f));

        if (button.getToggleState())
            return SamprLookAndFeel::accent().withAlpha (0.38f);

        if (down)
            return SamprLookAndFeel::divider();

        if (highlighted)
            return SamprLookAndFeel::surface().brighter (0.06f);

        return SamprLookAndFeel::surface();
    }
}

SamprLookAndFeel::SamprLookAndFeel()
{
    setColourScheme (getDarkColourScheme());
    initColours();
}

void SamprLookAndFeel::initColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, background());
    setColour (juce::DocumentWindow::backgroundColourId, background());

    setColour (juce::TabbedComponent::backgroundColourId, panel());
    setColour (juce::TabbedComponent::outlineColourId, border());
    setColour (juce::TabbedButtonBar::tabOutlineColourId, border());
    setColour (juce::TabbedButtonBar::tabTextColourId, textMuted());
    setColour (juce::TabbedButtonBar::frontTextColourId, textPrimary());

    setColour (juce::TextButton::buttonColourId, surface());
    setColour (juce::TextButton::buttonOnColourId, accent().withAlpha (0.22f));
    setColour (juce::TextButton::textColourOffId, textPrimary());
    setColour (juce::TextButton::textColourOnId, textPrimary());

    setColour (juce::ToggleButton::textColourId, textPrimary());
    setColour (juce::ToggleButton::tickColourId, accent());
    setColour (juce::ToggleButton::tickDisabledColourId, textMuted());

    setColour (juce::Slider::backgroundColourId, surface());
    setColour (juce::Slider::thumbColourId, accent());
    setColour (juce::Slider::trackColourId, divider());
    setColour (juce::Slider::textBoxTextColourId, textPrimary());
    setColour (juce::Slider::textBoxBackgroundColourId, surface());
    setColour (juce::Slider::textBoxOutlineColourId, border());

    setColour (juce::Label::textColourId, textPrimary());
    setColour (juce::ComboBox::backgroundColourId, surface());
    setColour (juce::ComboBox::textColourId, textPrimary());
    setColour (juce::ComboBox::outlineColourId, border());
    setColour (juce::ComboBox::arrowColourId, textMuted());

    setColour (juce::PopupMenu::backgroundColourId, panel());
    setColour (juce::PopupMenu::textColourId, textPrimary());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, textPrimary());

    setColour (juce::ListBox::backgroundColourId, panel());
    setColour (juce::ListBox::outlineColourId, border());
    setColour (juce::ListBox::textColourId, textPrimary());

    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, divider());
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    setColour (juce::TextEditor::backgroundColourId, surface());
    setColour (juce::TextEditor::textColourId, textPrimary());
    setColour (juce::TextEditor::outlineColourId, border());
    setColour (juce::TextEditor::focusedOutlineColourId, accent());
}

void SamprLookAndFeel::paintPanelOutline (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto r = bounds.toFloat().reduced (0.5f);
    g.setColour (panel().brighter (0.03f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (border().withAlpha (0.82f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void SamprLookAndFeel::paintHorizontalDivider (juce::Graphics& g, int y, int width)
{
    g.setColour (border());
    g.drawHorizontalLine (y, 0.0f, static_cast<float> (width));
}

void SamprLookAndFeel::paintVerticalDivider (juce::Graphics& g, int x, int y, int height)
{
    g.setColour (border());
    g.drawVerticalLine (x, static_cast<float> (y), static_cast<float> (y + height));
}

void SamprLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour&,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    const auto fill = buttonFill (button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.08f), bounds.getX(), bounds.getY(),
                                             fill.darker (0.14f), bounds.getX(), bounds.getBottom(),
                                             false));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (border().withAlpha (button.isEnabled() ? 0.95f : 0.45f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    if (button.getButtonText().equalsIgnoreCase ("play")
        || button.getButtonText().equalsIgnoreCase ("stop")
        || button.getButtonText().containsIgnoreCase ("export"))
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds.removeFromTop (bounds.getHeight() * 0.45f), 4.0f);
    }
}

void SamprLookAndFeel::drawToggleButton (juce::Graphics& g,
                                         juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    const auto fill = buttonFill (button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.06f), bounds.getX(), bounds.getY(),
                                             fill.darker (0.16f), bounds.getX(), bounds.getBottom(),
                                             false));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (border());
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto tickSize = juce::jmin (14.0f, bounds.getHeight() - 6.0f);
    const auto tickX = bounds.getX() + 6.0f;
    const auto tickY = bounds.getCentreY() - tickSize * 0.5f;

    drawTickBox (g, button, tickX, tickY, tickSize, tickSize,
                 button.getToggleState(),
                 button.isEnabled(),
                 shouldDrawButtonAsHighlighted,
                 shouldDrawButtonAsDown);

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (juce::FontOptions { juce::jmin (13.0f, bounds.getHeight() * 0.55f) });
    g.drawText (button.getButtonText(),
                static_cast<int> (tickX + tickSize + 6.0f),
                0,
                static_cast<int> (bounds.getRight() - tickX - tickSize - 8.0f),
                static_cast<int> (bounds.getHeight()),
                juce::Justification::centredLeft,
                true);
}

void SamprLookAndFeel::drawTickBox (juce::Graphics& g,
                                    juce::Component&,
                                    float x,
                                    float y,
                                    float w,
                                    float h,
                                    bool ticked,
                                    bool isEnabled,
                                    bool,
                                    bool)
{
    const auto box = juce::Rectangle<float> (x, y, w, h).reduced (0.5f);

    g.setColour (isEnabled ? surface().brighter (0.04f) : surface().darker (0.2f));
    g.fillRoundedRectangle (box, 2.0f);
    g.setColour (border());
    g.drawRoundedRectangle (box, 2.0f, 1.0f);

    if (ticked)
    {
        g.setColour (isEnabled ? accent() : textMuted());
        g.fillRoundedRectangle (box.reduced (3.0f), 1.5f);
    }
}

void SamprLookAndFeel::drawTabButton (juce::TabBarButton& button,
                                      juce::Graphics& g,
                                      bool isMouseOver,
                                      bool isMouseDown)
{
    const auto area = button.getActiveArea();
    const bool isFront = button.isFrontTab();

    const auto tab = area.toFloat().reduced (1.0f, 1.0f);
    g.setColour (isFront ? panel().brighter (0.06f) : background().brighter (0.02f));
    g.fillRoundedRectangle (tab.withTrimmedBottom (-4.0f), 5.0f);

    if (isMouseDown)
    {
        g.setColour (divider().withAlpha (0.45f));
        g.fillRect (area);
    }
    else if (isMouseOver && ! isFront)
    {
        g.setColour (surface().withAlpha (0.65f));
        g.fillRect (area);
    }

    if (isFront)
    {
        g.setColour (accent());
        g.fillRect (area.getX() + 6, area.getBottom() - 3, area.getWidth() - 12, 3);
    }

    g.setColour (border());
    g.drawVerticalLine (area.getRight() - 1,
                        static_cast<float> (area.getY()),
                        static_cast<float> (area.getBottom()));

    g.setColour (button.isFrontTab() ? textPrimary() : textMuted());
    g.setFont (juce::FontOptions { 12.0f, button.isFrontTab() ? juce::Font::bold : juce::Font::plain });
    g.drawText (button.getButtonText(), area, juce::Justification::centred, true);
}

void SamprLookAndFeel::drawTabAreaBehindFrontButton (juce::TabbedButtonBar&,
                                                     juce::Graphics& g,
                                                     int w,
                                                     int h)
{
    g.fillAll (background().darker (0.08f));
    paintHorizontalDivider (g, h - 1, w);
}

void SamprLookAndFeel::drawComboBox (juce::Graphics& g,
                                     int width,
                                     int height,
                                     bool isButtonDown,
                                     int buttonX,
                                     int buttonY,
                                     int buttonW,
                                     int buttonH,
                                     juce::ComboBox& box)
{
    juce::ignoreUnused (box);

    const auto bounds = toRect (0, 0, width, height).reduced (0.5f);

    g.setColour (isButtonDown ? divider() : surface().brighter (0.03f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (border());
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path arrow;
    const auto arrowArea = toRect (buttonX, buttonY, buttonW, buttonH).reduced (6.0f);
    arrow.addTriangle (arrowArea.getCentreX() - 3.0f, arrowArea.getCentreY() - 2.0f,
                       arrowArea.getCentreX() + 3.0f, arrowArea.getCentreY() - 2.0f,
                       arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    g.setColour (textMuted());
    g.fillPath (arrow);
}

void SamprLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (panel().brighter (0.03f));
    g.setColour (border());
    g.drawRect (0, 0, width, height, 1);
}

void SamprLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                          const juce::Rectangle<int>& area,
                                          bool isSeparator,
                                          bool isActive,
                                          bool isHighlighted,
                                          bool isTicked,
                                          bool hasSubMenu,
                                          const juce::String& text,
                                          const juce::String& shortcutKeyText,
                                          const juce::Drawable* icon,
                                          const juce::Colour* textColour)
{
    juce::LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted, isTicked,
                                             hasSubMenu, text, shortcutKeyText, icon, textColour);

    if (! isSeparator)
    {
        g.setColour (border().withAlpha (0.5f));
        g.drawHorizontalLine (area.getBottom() - 1, static_cast<float> (area.getX()),
                              static_cast<float> (area.getRight()));
    }
}

void SamprLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         float sliderPos,
                                         float,
                                         float,
                                         juce::Slider::SliderStyle style,
                                         juce::Slider& slider)
{
    const auto bounds = toRect (x, y, width, height);
    const bool isVertical = style == juce::Slider::LinearVertical
                            || style == juce::Slider::LinearBarVertical;

    g.setColour (divider());
    if (isVertical)
        g.fillRect (bounds.withWidth (2.0f).withCentre (bounds.getCentre()));
    else
        g.fillRect (bounds.withHeight (2.0f).withCentre (bounds.getCentre()));

    const auto thumbSize = 10.0f;
    juce::Rectangle<float> thumb;

    if (isVertical)
        thumb = { bounds.getCentreX() - thumbSize * 0.5f, sliderPos - thumbSize * 0.5f,
                  thumbSize, thumbSize };
    else
        thumb = { sliderPos - thumbSize * 0.5f, bounds.getCentreY() - thumbSize * 0.5f,
                  thumbSize, thumbSize };

    g.setColour (slider.isEnabled() ? accent() : textMuted());
    g.fillRoundedRectangle (thumb, 2.0f);
}

void SamprLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const auto bounds = toRect (x, y, width, height).reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (surface().brighter (0.04f));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (divider());
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    juce::Path indicator;
    const auto indicatorLength = radius * 0.75f;
    indicator.startNewSubPath (centre.x, centre.y);
    indicator.lineTo (centre.x, centre.y - indicatorLength);
    indicator.applyTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

    g.setColour (slider.isEnabled() ? accent() : textMuted());
    g.strokePath (indicator, juce::PathStrokeType (2.0f));
}

int SamprLookAndFeel::getDefaultScrollbarWidth()
{
    return 8;
}

void SamprLookAndFeel::drawScrollbar (juce::Graphics& g,
                                    juce::ScrollBar&,
                                    int x,
                                    int y,
                                    int width,
                                    int height,
                                    bool isScrollbarVertical,
                                    int thumbStartPosition,
                                    int thumbSize,
                                    bool isMouseOver,
                                    bool isMouseDown)
{
    juce::Rectangle<int> thumbBounds;

    if (isScrollbarVertical)
        thumbBounds = { x, thumbStartPosition, width, thumbSize };
    else
        thumbBounds = { thumbStartPosition, y, thumbSize, height };

    auto colour = divider();

    if (isMouseDown)
        colour = accent();
    else if (isMouseOver)
        colour = divider().brighter (0.15f);

    g.setColour (colour);
    g.fillRoundedRectangle (thumbBounds.toFloat().reduced (1.0f), 3.0f);
}

void SamprLookAndFeel::fillTextEditorBackground (juce::Graphics& g,
                                                 int width,
                                                 int height,
                                                 juce::TextEditor&)
{
    g.fillAll (surface());
}

void SamprLookAndFeel::drawTextEditorOutline (juce::Graphics& g,
                                              int width,
                                              int height,
                                              juce::TextEditor& editor)
{
    const auto outline = editor.hasKeyboardFocus (true) ? accent() : border();
    g.setColour (outline);
    g.drawRect (0, 0, width, height, 1);
}

juce::Font SamprLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions { juce::jmin (13.0f, static_cast<float> (buttonHeight) * 0.55f), juce::Font::bold };
}

juce::Font SamprLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions { 12.0f };
}

} // namespace sampr
