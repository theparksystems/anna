#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/ProjectController.h"

namespace sampr
{

class ArrangementTimelineComponent final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit ArrangementTimelineComponent (ProjectController& controller,
                                             PatternStore& patterns);

    void setChangeCallback (ChangeCallback callback);
    void refresh();
    void setSongPlayheadBar (double barPosition);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    struct ClipHit
    {
        ClipId clipId = kInvalidClipId;
        int patternIndex = -1;
    };

    juce::Rectangle<int> getTimelineBounds() const;
    double xToBar (int x) const;
    int barToX (double bar) const;
    ClipHit hitTestClip (juce::Point<int> pos) const;

    ProjectController& projectController;
    PatternStore& patternStore;

    juce::TextButton addClipButton { "+ Clip" };
    juce::ComboBox patternSelector;
    juce::Slider lengthBarsSlider;
    juce::Label lengthBarsLabel;

    double songPlayheadBar = 0.0;
    ClipId selectedClipId = kInvalidClipId;
    ClipId dragClipId = kInvalidClipId;
    double dragStartBar = 0.0;
    int dragPointerOffsetBars = 0;
    bool dragEditOpen = false;

    ChangeCallback onChange;
};

} // namespace sampr
