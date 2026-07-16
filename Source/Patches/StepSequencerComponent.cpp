#include "StepSequencerComponent.h"

#include "../UI/SamprLookAndFeel.h"

#include "../Model/ProjectModel.h"

namespace sampr
{

StepSequencerComponent::StepSequencerComponent (PatternStore& store)
    : patternStore (store)
{
    addAndMakeVisible (patternSelector);
    addAndMakeVisible (stepCountSelector);
    addAndMakeVisible (addRowButton);
    addAndMakeVisible (addPatternButton);
    addAndMakeVisible (clearPatternButton);
    addAndMakeVisible (syncAssetsButton);

    for (int i = 0; i < ProjectModel::kMaxPatterns; ++i)
        patternSelector.addItem ("Pattern " + juce::String (i + 1), i + 1);

    stepCountSelector.addItem ("16 steps", 16);
    stepCountSelector.addItem ("32 steps", 32);
    stepCountSelector.setSelectedId (16, juce::dontSendNotification);

    patternSelector.onChange = [this] { onToolbarChanged(); };
    stepCountSelector.onChange = [this] { onToolbarChanged(); };
    addRowButton.setTooltip ("Add the selected sample as a new instrument track.");
    addPatternButton.setButtonText ("+ Track");
    addPatternButton.setTooltip ("Add the selected sample as a new instrument track.");
    syncAssetsButton.setTooltip ("Create one sequencer track for every loaded sample.");

    addRowButton.onClick = [this] { addSelectedSampleRow(); };

    addPatternButton.onClick = [this] { addSelectedSampleRow(); };

    clearPatternButton.onClick = [this]
    {
        patternStore.clearPattern();
        refresh();
        if (onChange != nullptr)
            onChange();
    };

    syncAssetsButton.onClick = [this]
    {
        patternStore.syncRowsFromLoadedAssets();
        refresh();
        if (onChange != nullptr)
            onChange();
    };

    refresh();
}

void StepSequencerComponent::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void StepSequencerComponent::setUserMessageCallback (UserMessageCallback callback)
{
    onUserMessage = std::move (callback);
}

void StepSequencerComponent::setCurrentStepIndex (int stepIndex)
{
    if (currentStepIndex == stepIndex)
        return;

    currentStepIndex = stepIndex;
    repaint();
}

void StepSequencerComponent::refresh()
{
    const juce::ScopedValueSetter<bool> guard (refreshingToolbar, true);

    patternSelector.clear();
    for (int i = 0; i < patternStore.getPatternCount(); ++i)
        patternSelector.addItem (patternStore.getPattern (i).name, i + 1);

    patternSelector.setSelectedId (patternStore.getCurrentPatternIndex() + 1, juce::dontSendNotification);
    stepCountSelector.setSelectedId (patternStore.getCurrentPattern().numSteps, juce::dontSendNotification);
    repaint();
}

int StepSequencerComponent::addSelectedSampleRow()
{
    const auto row = patternStore.addRowFromSelectedAsset();

    if (row < 0)
    {
        if (onUserMessage != nullptr)
            onUserMessage ("Select a sample on the left, then add or drag it into the sequencer.");
        return row;
    }

    if (onUserMessage != nullptr)
        onUserMessage ("Added track " + juce::String (row + 1) + " from selected sample.");

    return row;
}

void StepSequencerComponent::onToolbarChanged()
{
    if (refreshingToolbar)
        return;

    const auto selectedPatternId = patternSelector.getSelectedId();
    const auto selectedStepCount = stepCountSelector.getSelectedId();

    if (selectedPatternId <= 0 || selectedStepCount <= 0)
        return;

    patternStore.selectPattern (patternStore.getPatternCount() > 0
                                    ? selectedPatternId - 1
                                    : 0);
    patternStore.setNumSteps (selectedStepCount);
    refresh();

    if (onChange != nullptr)
        onChange();
}

juce::Rectangle<int> StepSequencerComponent::getGridBounds() const
{
    auto area = getLocalBounds();
    area.removeFromTop (metrics.toolbarHeight + metrics.beatMarkerHeight + 4);
    return area.reduced (4, 0);
}

bool StepSequencerComponent::hitTestCell (juce::Point<int> pos, int& rowOut, int& stepOut) const
{
    const auto grid = getGridBounds();
    const auto& pattern = patternStore.getCurrentPattern();

    if (! grid.contains (pos) || pattern.numSteps <= 0)
        return false;

    const auto gridContent = grid.withTrimmedLeft (metrics.rowHeaderWidth);
    const auto cellWidth = gridContent.getWidth() / pattern.numSteps;
    const auto row = (pos.y - grid.getY()) / metrics.rowHeight;

    if (! juce::isPositiveAndBelow (row, static_cast<int> (pattern.rows.size())))
        return false;

    const auto step = (pos.x - gridContent.getX()) / juce::jmax (1, cellWidth);

    if (! juce::isPositiveAndBelow (step, pattern.numSteps))
        return false;

    rowOut = row;
    stepOut = step;
    return true;
}

void StepSequencerComponent::handleCellInteraction (int row,
                                                    int step,
                                                    const juce::ModifierKeys& mods,
                                                    bool isDrag)
{
    auto& pattern = patternStore.getCurrentPattern();

    if (! juce::isPositiveAndBelow (row, static_cast<int> (pattern.rows.size())))
        return;

    auto& cell = pattern.rows[static_cast<size_t> (row)].steps[static_cast<size_t> (step)];

    if (mods.isRightButtonDown())
    {
        patternStore.clearRow (row);
        if (onChange != nullptr)
            onChange();
        repaint();
        return;
    }

    if (mods.isShiftDown() || velocityDragMode)
    {
        if (! isDrag)
        {
            dragRow = row;
            dragStep = step;
            dragStartValue = cell.velocity;
            velocityDragMode = true;
        }

        return;
    }

    if (mods.isAltDown())
    {
        if (! isDrag)
        {
            dragRow = row;
            dragStep = step;
            dragStartValue = cell.probability;
            velocityDragMode = false;
        }

        return;
    }

    if (! isDrag)
    {
        patternStore.toggleStep (row, step);

        if (onChange != nullptr)
            onChange();

        repaint();
    }
}

void StepSequencerComponent::mouseDown (const juce::MouseEvent& event)
{
    int row = -1;
    int step = -1;

    if (! hitTestCell (event.getPosition(), row, step))
        return;

    handleCellInteraction (row, step, event.mods, false);
}

void StepSequencerComponent::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    dragRow = -1;
    dragStep = -1;
    velocityDragMode = false;
}

void StepSequencerComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (dragRow < 0 || dragStep < 0)
        return;

    const auto delta = juce::jlimit (-1.0f, 1.0f, -static_cast<float> (event.getDistanceFromDragStartY()) / 80.0f);
    const auto value = juce::jlimit (0.05f, 1.0f, dragStartValue + delta);

    if (event.mods.isAltDown())
        patternStore.setStepProbability (dragRow, dragStep, value);
    else
        patternStore.setStepVelocity (dragRow, dragStep, value);

    if (onChange != nullptr)
        onChange();

    repaint();
}

bool StepSequencerComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        const auto row = patternStore.getCurrentPattern().rows.empty() ? -1 : 0;

        if (row >= 0)
        {
            patternStore.clearRow (row);
            refresh();

            if (onChange != nullptr)
                onChange();
        }

        return true;
    }

    return false;
}

void StepSequencerComponent::paintBeatMarkers (juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    if (gridArea.getWidth() <= metrics.rowHeaderWidth || gridArea.getHeight() <= 0)
        return;

    const auto& pattern = patternStore.getCurrentPattern();
    const auto gridContent = gridArea.withTrimmedLeft (metrics.rowHeaderWidth);
    const auto cellWidth = gridContent.getWidth() / juce::jmax (1, pattern.numSteps);

    g.setColour (SamprLookAndFeel::textMuted());
    g.setFont (juce::FontOptions { 10.0f });

    for (int step = 0; step < pattern.numSteps; ++step)
    {
        if (step % 4 != 0)
            continue;

        const auto x = gridContent.getX() + step * cellWidth;
        g.drawText (juce::String ((step / 4) + 1),
                    x, gridArea.getY() - metrics.beatMarkerHeight, cellWidth * 4, metrics.beatMarkerHeight,
                    juce::Justification::centred);
    }
}

void StepSequencerComponent::paintRows (juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    if (gridArea.getWidth() <= metrics.rowHeaderWidth || gridArea.getHeight() <= 0)
        return;

    const auto& pattern = patternStore.getCurrentPattern();
    const auto gridContent = gridArea.withTrimmedLeft (metrics.rowHeaderWidth);
    const auto cellWidth = gridContent.getWidth() / juce::jmax (1, pattern.numSteps);

    for (int row = 0; row < static_cast<int> (pattern.rows.size()); ++row)
    {
        const auto& patternRow = pattern.rows[static_cast<size_t> (row)];
        auto rowArea = gridArea.removeFromTop (metrics.rowHeight);

        g.setColour (row % 2 == 0 ? SamprLookAndFeel::panel().brighter (0.02f)
                                  : SamprLookAndFeel::panel().darker (0.03f));
        g.fillRect (rowArea);
        g.setColour (SamprLookAndFeel::border().withAlpha (0.65f));
        g.drawHorizontalLine (rowArea.getBottom() - 1,
                              static_cast<float> (gridArea.getX()),
                              static_cast<float> (gridArea.getRight()));

        auto header = rowArea.removeFromLeft (metrics.rowHeaderWidth);
        g.setColour (SamprLookAndFeel::surface().withAlpha (0.84f));
        g.fillRect (header);
        g.setColour (patternRow.colour.withAlpha (0.95f));
        g.fillEllipse (static_cast<float> (header.getRight() - 14),
                       static_cast<float> (header.getCentreY() - 4),
                       8.0f,
                       8.0f);

        g.setColour (patternRow.colour.withAlpha (0.9f));
        g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
        g.drawText (patternRow.label, header.reduced (8, 0), juce::Justification::centredLeft, true);

        for (int step = 0; step < pattern.numSteps; ++step)
        {
            auto cell = gridContent.withWidth (cellWidth).withX (gridContent.getX() + step * cellWidth)
                                   .withY (rowArea.getY()).withHeight (metrics.rowHeight - 1);

            const auto& cellData = patternRow.steps[static_cast<size_t> (step)];
            const bool isCurrent = step == currentStepIndex;

            const auto cellBounds = cell.toFloat().reduced (1.0f);
            g.setColour (step % 4 == 0 ? SamprLookAndFeel::surface().brighter (0.02f)
                                        : SamprLookAndFeel::background().brighter (0.08f));
            g.fillRect (cellBounds);

            if (isCurrent)
            {
                g.setColour (SamprLookAndFeel::accent().withAlpha (0.62f));
                g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 3.0f, 1.5f);
            }

            if (step % 4 == 0)
            {
                g.setColour (SamprLookAndFeel::border());
                g.drawVerticalLine (cell.getX(), static_cast<float> (cell.getY()),
                                    static_cast<float> (cell.getBottom()));
            }

            if (cellData.active)
            {
                const auto alpha = 0.35f + cellData.velocity * 0.65f;
                const auto active = cell.reduced (3).toFloat();
                g.setGradientFill (juce::ColourGradient (patternRow.colour.brighter (0.16f).withAlpha (alpha),
                                                         active.getX(), active.getY(),
                                                         patternRow.colour.darker (0.22f).withAlpha (alpha),
                                                         active.getX(), active.getBottom(),
                                                         false));
                g.fillRoundedRectangle (active.reduced (1.0f, 5.0f), 3.0f);

                if (cellData.probability < 0.99f)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.7f));
                    g.setFont (juce::FontOptions { 9.0f });
                    g.drawText (juce::String (static_cast<int> (cellData.probability * 100)) + "%",
                                cell, juce::Justification::bottomRight, false);
                }
            }
        }
    }
}

void StepSequencerComponent::paintEmptyPlaylistRows (juce::Graphics& g, juce::Rectangle<int> gridArea)
{
    if (gridArea.getWidth() <= metrics.rowHeaderWidth || gridArea.getHeight() <= 0)
        return;

    const auto visibleRows = juce::jmax (8, gridArea.getHeight() / metrics.rowHeight);
    const auto gridContent = gridArea.withTrimmedLeft (metrics.rowHeaderWidth);
    const auto& pattern = patternStore.getCurrentPattern();
    const auto cellWidth = gridContent.getWidth() / juce::jmax (1, pattern.numSteps);

    for (int row = 0; row < visibleRows; ++row)
    {
        auto rowArea = gridArea.withY (gridArea.getY() + row * metrics.rowHeight).withHeight (metrics.rowHeight);

        if (rowArea.getY() >= gridArea.getBottom())
            break;

        g.setColour (row % 2 == 0 ? juce::Colour (0xff17242b) : juce::Colour (0xff142027));
        g.fillRect (rowArea);

        auto header = rowArea.removeFromLeft (metrics.rowHeaderWidth);
        g.setColour (juce::Colour (0xff253039));
        g.fillRect (header);

        g.setColour (SamprLookAndFeel::textMuted().withAlpha (0.72f));
        g.setFont (juce::FontOptions { 12.0f });
        g.drawText ("Track " + juce::String (row + 1), header.reduced (8, 0), juce::Justification::centredLeft, true);

        g.setColour (SamprLookAndFeel::success().withAlpha (0.82f));
        g.fillEllipse (static_cast<float> (header.getRight() - 14),
                       static_cast<float> (header.getCentreY() - 4),
                       8.0f,
                       8.0f);

        for (int step = 0; step < pattern.numSteps; ++step)
        {
            const auto x = gridContent.getX() + step * cellWidth;
            g.setColour (step % 4 == 0 ? SamprLookAndFeel::border().withAlpha (0.72f)
                                        : SamprLookAndFeel::divider().withAlpha (0.22f));
            g.drawVerticalLine (x, static_cast<float> (rowArea.getY()), static_cast<float> (rowArea.getBottom()));
        }

        g.setColour (SamprLookAndFeel::border().withAlpha (0.58f));
        g.drawHorizontalLine (rowArea.getBottom() - 1,
                              static_cast<float> (gridArea.getX()),
                              static_cast<float> (gridArea.getRight()));
    }
}

void StepSequencerComponent::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff203039), 0.0f, 0.0f,
                                             juce::Colour (0xff0f1b21), 0.0f, static_cast<float> (getHeight()),
                                             false));
    g.fillRect (getLocalBounds());

    g.setColour (SamprLookAndFeel::accentCool());
    g.setFont (juce::FontOptions { 12.5f, juce::Font::bold });
    g.drawText ("PLAYLIST SEQUENCER", 8, 5, 220, 18, juce::Justification::centredLeft);

    auto markerArea = getLocalBounds();
    markerArea.removeFromTop (metrics.toolbarHeight + 2);
    markerArea.setHeight (metrics.beatMarkerHeight);
    g.setColour (juce::Colour (0xff1a2a32));
    g.fillRect (markerArea);
    paintBeatMarkers (g, markerArea);

    if (patternStore.getCurrentPattern().rows.empty())
    {
        paintEmptyPlaylistRows (g, getGridBounds());
        g.setColour (SamprLookAndFeel::textMuted().withAlpha (0.82f));
        g.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        g.drawText ("Drag samples from the left panel into a track lane",
                    getGridBounds().reduced (metrics.rowHeaderWidth + 24, 0),
                    juce::Justification::centred,
                    true);
        return;
    }

    paintRows (g, getGridBounds());
}

void StepSequencerComponent::resized()
{
    auto toolbar = getLocalBounds().reduced (4, 2);
    toolbar.removeFromTop (22);
    toolbar.setHeight (metrics.toolbarHeight);

    auto x = toolbar.getX();
    const int h = toolbar.getHeight();
    const int gap = 6;

    patternSelector.setBounds (x, toolbar.getY(), 150, h);
    x += 150 + gap;
    stepCountSelector.setBounds (x, toolbar.getY(), 100, h);
    x += 100 + gap;
    addRowButton.setBounds (x, toolbar.getY(), 70, h);
    x += 70 + gap;
    syncAssetsButton.setBounds (x, toolbar.getY(), 100, h);
    x += 100 + gap;
    addPatternButton.setBounds (x, toolbar.getY(), 82, h);
    x += 82 + gap;
    clearPatternButton.setBounds (x, toolbar.getY(), 60, h);
}

} // namespace sampr
