#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"

namespace sampr
{

class StepSequencerComponent final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit StepSequencerComponent (PatternStore& store);

    void setChangeCallback (ChangeCallback callback);
    void setCurrentStepIndex (int stepIndex);
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    struct LayoutMetrics
    {
        int rowHeaderWidth = 120;
        int rowHeight = 28;
        int toolbarHeight = 34;
        int beatMarkerHeight = 18;
    };

    juce::Rectangle<int> getGridBounds() const;
    bool hitTestCell (juce::Point<int> pos, int& rowOut, int& stepOut) const;
    void handleCellInteraction (int row, int step, const juce::ModifierKeys& mods, bool isDrag);
    void paintBeatMarkers (juce::Graphics& g, juce::Rectangle<int> gridArea);
    void paintRows (juce::Graphics& g, juce::Rectangle<int> gridArea);
    void onToolbarChanged();

    PatternStore& patternStore;

    juce::ComboBox patternSelector;
    juce::ComboBox stepCountSelector;
    juce::TextButton addRowButton { "+ Row" };
    juce::TextButton addPatternButton { "+ Pattern" };
    juce::TextButton clearPatternButton { "Clear" };
    juce::TextButton syncAssetsButton { "Sync Samples" };

    int currentStepIndex = -1;
    int dragRow = -1;
    int dragStep = -1;
    float dragStartValue = 1.0f;
    bool velocityDragMode = false;

    ChangeCallback onChange;
    LayoutMetrics metrics;
};

} // namespace sampr