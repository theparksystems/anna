#include "ArrangementTimelineComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

ArrangementTimelineComponent::ArrangementTimelineComponent (ProjectController& controller,
                                                            PatternStore& patterns)
    : projectController (controller),
      patternStore (patterns)
{
    addClipButton.onClick = [this]
    {
        const auto patternIndex = patternSelector.getSelectedId() - 1;

        if (! juce::isPositiveAndBelow (patternIndex, patternStore.getPatternCount()))
            return;

        const auto& pattern = patternStore.getPattern (patternIndex);
        const auto startBar = static_cast<double> (projectController.getModel().getArrangementEndBar());
        projectController.addArrangementClip (pattern.id, startBar, juce::jmax (1, pattern.lengthBars));
        refresh();

        if (onChange != nullptr)
            onChange();
    };

    lengthBarsSlider.setRange (8, 256, 1);
    lengthBarsSlider.setValue (projectController.getModel().getSettings().arrangementLengthBars,
                               juce::dontSendNotification);
    lengthBarsSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    lengthBarsSlider.onValueChange = [this]
    {
        projectController.getModel().getSettings().arrangementLengthBars
            = static_cast<int> (lengthBarsSlider.getValue());
        repaint();
        if (onChange != nullptr)
            onChange();
    };

    lengthBarsLabel.setText ("Length (bars)", juce::dontSendNotification);

    addClipButton.setTooltip ("Add the selected pattern to the song timeline");
    addAndMakeVisible (addClipButton);
    addAndMakeVisible (patternSelector);
    addAndMakeVisible (lengthBarsSlider);
    addAndMakeVisible (lengthBarsLabel);

    refresh();
}

void ArrangementTimelineComponent::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void ArrangementTimelineComponent::refresh()
{
    patternSelector.clear();

    const auto& patterns = projectController.getModel().getPatterns();

    for (int i = 0; i < static_cast<int> (patterns.size()); ++i)
        patternSelector.addItem (patterns[static_cast<size_t> (i)].name, i + 1);

    if (! patterns.empty())
        patternSelector.setSelectedId (patternStore.getCurrentPatternIndex() + 1, juce::dontSendNotification);

    lengthBarsSlider.setValue (projectController.getModel().getSettings().arrangementLengthBars,
                               juce::dontSendNotification);
    repaint();
}

void ArrangementTimelineComponent::setSongPlayheadBar (double barPosition)
{
    songPlayheadBar = barPosition;
    repaint();
}

juce::Rectangle<int> ArrangementTimelineComponent::getTimelineBounds() const
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (36);
    return area;
}

double ArrangementTimelineComponent::xToBar (int x) const
{
    const auto timeline = getTimelineBounds();
    const auto lengthBars = juce::jmax (8, projectController.getModel().getSettings().arrangementLengthBars);
    const auto norm = static_cast<double> (x - timeline.getX()) / static_cast<double> (timeline.getWidth());
    return juce::jlimit (0.0, static_cast<double> (lengthBars), norm * static_cast<double> (lengthBars));
}

int ArrangementTimelineComponent::barToX (double bar) const
{
    const auto timeline = getTimelineBounds();
    const auto lengthBars = juce::jmax (8, projectController.getModel().getSettings().arrangementLengthBars);
    const auto norm = bar / static_cast<double> (lengthBars);
    return timeline.getX() + static_cast<int> (norm * static_cast<double> (timeline.getWidth()));
}

ArrangementTimelineComponent::ClipHit ArrangementTimelineComponent::hitTestClip (juce::Point<int> pos) const
{
    ClipHit hit;
    const auto& arrangement = projectController.getModel().getArrangement();
    const auto timeline = getTimelineBounds();

    if (! timeline.contains (pos))
        return hit;

    const auto bar = xToBar (pos.x);

    for (const auto& clip : arrangement)
    {
        const auto endBar = clip.startBar + static_cast<double> (clip.lengthBars);

        if (bar >= clip.startBar && bar < endBar)
        {
            hit.clipId = clip.id;
            hit.patternIndex = projectController.getModel().findPatternIndexById (clip.patternId);
            return hit;
        }
    }

    return hit;
}

void ArrangementTimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::background());

    const auto timeline = getTimelineBounds();
    const auto lengthBars = juce::jmax (8, projectController.getModel().getSettings().arrangementLengthBars);

    g.setColour (SamprLookAndFeel::panel());
    g.fillRect (timeline);
    SamprLookAndFeel::paintPanelOutline (g, timeline);

    g.setColour (juce::Colours::white.withAlpha (0.08f));

    for (int bar = 0; bar <= lengthBars; bar += 4)
    {
        const auto x = barToX (static_cast<double> (bar));
        g.drawVerticalLine (x, static_cast<float> (timeline.getY()),
                            static_cast<float> (timeline.getBottom()));
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (juce::FontOptions { 11.0f });

    for (int bar = 0; bar <= lengthBars; bar += 4)
    {
        const auto x = barToX (static_cast<double> (bar));
        g.drawText (juce::String (bar + 1), x + 2, timeline.getY() - 16, 40, 14,
                    juce::Justification::topLeft, false);
    }

    for (const auto& clip : projectController.getModel().getArrangement())
    {
        const auto x0 = barToX (clip.startBar);
        const auto x1 = barToX (clip.startBar + clip.lengthBars);
        auto clipBounds = juce::Rectangle<int> (x0, timeline.getY() + 4,
                                                juce::jmax (12, x1 - x0), timeline.getHeight() - 8);

        const auto isSelected = clip.id == selectedClipId;
        g.setColour (clip.colour.withAlpha (isSelected ? 0.95f : 0.72f));
        g.fillRect (clipBounds);

        g.setColour (isSelected ? SamprLookAndFeel::accent() : SamprLookAndFeel::border());
        g.drawRect (clipBounds, isSelected ? 2 : 1);

        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
        g.drawText (clip.name, clipBounds.reduced (4, 1), juce::Justification::centredLeft, true);
    }

    if (projectController.getPlaybackMode() == PlaybackMode::song)
    {
        const auto playheadX = barToX (songPlayheadBar);
        g.setColour (SamprLookAndFeel::accent());
        g.drawLine (static_cast<float> (playheadX), static_cast<float> (timeline.getY()),
                    static_cast<float> (playheadX), static_cast<float> (timeline.getBottom()), 2.0f);
    }

    if (projectController.getModel().getArrangement().empty())
    {
        g.setColour (SamprLookAndFeel::textMuted());
        g.setFont (juce::FontOptions { 12.0f });
        g.drawText ("Timeline empty — choose a pattern and click + Clip.\nDrag clips to move. Enable Song Mode to play the arrangement.",
                    timeline.reduced (12),
                    juce::Justification::centred);
    }
}

void ArrangementTimelineComponent::resized()
{
    auto toolbar = getLocalBounds().reduced (4);
    toolbar.setHeight (28);
    addClipButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (8);
    patternSelector.setBounds (toolbar.removeFromLeft (160));
    toolbar.removeFromLeft (12);
    lengthBarsLabel.setBounds (toolbar.removeFromLeft (90));
    lengthBarsSlider.setBounds (toolbar.removeFromLeft (140));
}

void ArrangementTimelineComponent::mouseDown (const juce::MouseEvent& event)
{
    const auto hit = hitTestClip (event.getPosition());

    if (hit.clipId != kInvalidClipId)
    {
        selectedClipId = hit.clipId;
        dragClipId = hit.clipId;

        if (const auto* clip = projectController.getModel().findClipById (hit.clipId))
        {
            dragStartBar = clip->startBar;
            dragPointerOffsetBars = static_cast<int> (xToBar (event.x) - clip->startBar);
        }

        if (hit.patternIndex >= 0)
            patternStore.selectPattern (hit.patternIndex);

        repaint();

        if (onChange != nullptr)
            onChange();
        return;
    }

    selectedClipId = kInvalidClipId;
    repaint();
}

void ArrangementTimelineComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (dragClipId == kInvalidClipId)
        return;

    const auto newStart = xToBar (event.x) - static_cast<double> (dragPointerOffsetBars);
    projectController.moveArrangementClip (dragClipId, newStart);
    repaint();

    if (onChange != nullptr)
        onChange();
}

} // namespace sampr