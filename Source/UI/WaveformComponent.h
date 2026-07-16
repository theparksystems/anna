#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/SampleAsset.h"

namespace sampr
{

class WaveformComponent final : public juce::Component
{
public:
    using SliceClickedCallback = std::function<void (int sliceIndex)>;
    using AddSliceCallback = std::function<void (int samplePosition)>;
    using RemoveSliceCallback = std::function<void (int sliceIndex)>;
    using SliceTrimChangedCallback = std::function<void (int sliceIndex, int startSample, int endSample)>;
    using OpenSliceEditorCallback = std::function<void()>;

    void setPeaks (const WaveformPeaks& newPeaks);
    void setSliceMarkers (const std::vector<SliceRegion>& slices, int selectedSliceIndex, int totalSamples);
    void clear();

    void setSliceClickedCallback (SliceClickedCallback callback);
    void setAddSliceCallback (AddSliceCallback callback);
    void setRemoveSliceCallback (RemoveSliceCallback callback);
    void setSliceTrimChangedCallback (SliceTrimChangedCallback callback);
    void setOpenSliceEditorCallback (OpenSliceEditorCallback callback);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    enum class DragMode
    {
        none,
        trimStart,
        trimEnd
    };

    juce::Rectangle<float> getInnerBounds() const;
    float sampleToX (int sample) const;
    int xToSample (float x) const;
    int hitTestSlice (float x) const;
    DragMode hitTestTrimHandle (float x, int& outSliceIndex) const;
    void paintSliceRegion (juce::Graphics& g,
                           const juce::Rectangle<float>& inner,
                           int sliceIndex,
                           int startSample,
                           int endSample,
                           bool selected) const;
    void commitTrimDrag();
    void cancelTrimDrag();

    WaveformPeaks peaks;
    std::vector<SliceRegion> sliceMarkers;
    int selectedSliceIndex = 0;
    int totalSamples = 0;

    DragMode dragMode = DragMode::none;
    int dragSliceIndex = -1;
    int dragPreviewStart = 0;
    int dragPreviewEnd = 0;
    bool trimDragActive = false;

    static constexpr float kTrimHandlePixels = 7.0f;

    SliceClickedCallback onSliceClicked;
    AddSliceCallback onAddSlice;
    RemoveSliceCallback onRemoveSlice;
    SliceTrimChangedCallback onSliceTrimChanged;
    OpenSliceEditorCallback onOpenSliceEditor;
};

} // namespace sampr