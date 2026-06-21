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

    addRowButton.onClick = [this]
    {
        patternStore.addRowFromSelectedAsset();
        refresh();
        if (onChange != nullptr)
            onChange();
    };

    addPatternButton.onClick = [this]
    {
        patternStore.addPattern();
        refresh();
        if (onChange != nullptr)
            onChange();
    };

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

void StepSequencerComponent::setCurrentStepIndex (int stepIndex)
{
    if (currentStepIndex == stepIndex)
        return;

    currentStepIndex = stepIndex;
    repaint();
}

void StepSequencerComponent::refresh()
{
    patternSelector.clear();
    for (int i = 0; i < patternStore.getPatternCount(); ++i)
        patternSelector.addItem (patternStore.getPattern (i).name, i + 1);

    patternSelector.setSelectedId (patternStore.getCurrentPatternIndex() + 1, juce::dontSendNotification);
    stepCountSelector.setSelectedId (patternStore.getCurrentPattern().numSteps, juce::dontSendNotification);
    repaint();
}

void StepSequencerComponent::onToolbarChanged()
{
    patternStore.selectPattern (patternStore.getPatternCount() > 0
                                    ? patternSelector.getSelectedId() - 1
                                    : 0);
    patternStore.setNumSteps (stepCountSelector.getSelectedId());
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
    const auto& pattern = patternStore.getCurrentPattern();
    const auto gridContent = gridArea.withTrimmedLeft (metrics.rowHeaderWidth);
    const auto cellWidth = gridContent.getWidth() / juce::jmax (1, pattern.numSteps);

    for (int row = 0; row < static_cast<int> (pattern.rows.size()); ++row)
    {
        const auto& patternRow = pattern.rows[static_cast<size_t> (row)];
        auto rowArea = gridArea.removeFromTop (metrics.rowHeight);

        g.setColour (patternRow.colour.withAlpha (0.85f));
        g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
        g.drawText (patternRow.label, rowArea.removeFromLeft (metrics.rowHeaderWidth),
                    juce::Justification::centredLeft, true);

        for (int step = 0; step < pattern.numSteps; ++step)
        {
            auto cell = gridContent.withWidth (cellWidth).withX (gridContent.getX() + step * cellWidth)
                                   .withY (rowArea.getY()).withHeight (metrics.rowHeight - 2);

            const auto& cellData = patternRow.steps[static_cast<size_t> (step)];
            const bool isCurrent = step == currentStepIndex;

            g.setColour (SamprLookAndFeel::surface());
            g.fillRect (cell);

            if (isCurrent)
            {
                g.setColour (SamprLookAndFeel::accent().withAlpha (0.35f));
                g.drawRect (cell, 1);
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
                g.setColour (patternRow.colour.withAlpha (alpha));
                g.fillRect (cell.reduced (2));

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

void StepSequencerComponent::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::background());

    g.setColour (SamprLookAndFeel::textPrimary());
    g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    g.drawText ("Step Sequencer", 4, 4, 200, 18, juce::Justification::centredLeft);

    auto markerArea = getLocalBounds();
    markerArea.removeFromTop (metrics.toolbarHeight + 2);
    markerArea.setHeight (metrics.beatMarkerHeight);
    paintBeatMarkers (g, markerArea);

    paintRows (g, getGridBounds());

    if (patternStore.getCurrentPattern().rows.empty())
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Add rows with '+ Row' or 'Sync Samples'",
                    getGridBounds(), juce::Justification::centred);
    }
}

void StepSequencerComponent::resized()
{
    auto toolbar = getLocalBounds().reduced (4, 2);
    toolbar.removeFromTop (22);
    toolbar.setHeight (metrics.toolbarHeight);

    auto x = toolbar.getX();
    const int h = toolbar.getHeight();
    const int gap = 6;

    patternSelector.setBounds (x, toolbar.getY(), 130, h);
    x += 130 + gap;
    stepCountSelector.setBounds (x, toolbar.getY(), 100, h);
    x += 100 + gap;
    addRowButton.setBounds (x, toolbar.getY(), 70, h);
    x += 70 + gap;
    syncAssetsButton.setBounds (x, toolbar.getY(), 100, h);
    x += 100 + gap;
    addPatternButton.setBounds (x, toolbar.getY(), 90, h);
    x += 90 + gap;
    clearPatternButton.setBounds (x, toolbar.getY(), 60, h);
}

} // namespace sampr
