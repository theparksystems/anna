#include "WaveformComponent.h"

#include "SamprLookAndFeel.h"

#include <limits>

namespace sampr
{

void WaveformComponent::setPeaks (const WaveformPeaks& newPeaks)
{
    peaks = newPeaks;
    repaint();
}

void WaveformComponent::setSliceMarkers (const std::vector<SliceRegion>& slices,
                                         int newSelectedSliceIndex,
                                         int sampleCount)
{
    sliceMarkers = slices;
    selectedSliceIndex = newSelectedSliceIndex;
    totalSamples = sampleCount;

    if (! trimDragActive)
        repaint();
}

void WaveformComponent::clear()
{
    cancelTrimDrag();
    peaks = {};
    sliceMarkers.clear();
    totalSamples = 0;
    repaint();
}

void WaveformComponent::setSliceClickedCallback (SliceClickedCallback callback)
{
    onSliceClicked = std::move (callback);
}

void WaveformComponent::setAddSliceCallback (AddSliceCallback callback)
{
    onAddSlice = std::move (callback);
}

void WaveformComponent::setRemoveSliceCallback (RemoveSliceCallback callback)
{
    onRemoveSlice = std::move (callback);
}

void WaveformComponent::setSliceTrimChangedCallback (SliceTrimChangedCallback callback)
{
    onSliceTrimChanged = std::move (callback);
}

juce::Rectangle<float> WaveformComponent::getInnerBounds() const
{
    return getLocalBounds().toFloat().reduced (1.0f);
}

float WaveformComponent::sampleToX (int sample) const
{
    if (totalSamples <= 0)
        return getInnerBounds().getX();

    const auto& inner = getInnerBounds();
    return inner.getX()
           + (static_cast<float> (sample) / static_cast<float> (juce::jmax (1, totalSamples))) * inner.getWidth();
}

int WaveformComponent::xToSample (float x) const
{
    if (totalSamples <= 0)
        return 0;

    const auto& inner = getInnerBounds();
    const auto proportion = juce::jlimit (0.0f, 1.0f, (x - inner.getX()) / juce::jmax (1.0f, inner.getWidth()));
    return static_cast<int> (proportion * static_cast<float> (totalSamples));
}

int WaveformComponent::hitTestSlice (float x) const
{
    if (totalSamples <= 0)
        return -1;

    const auto clickSample = xToSample (x);
    auto bestIndex = -1;
    auto bestDistance = std::numeric_limits<int>::max();

    for (int i = 0; i < static_cast<int> (sliceMarkers.size()); ++i)
    {
        const auto distance = std::abs (sliceMarkers[static_cast<size_t> (i)].startSample - clickSample);
        const auto pixelDistance = static_cast<float> (distance) * getInnerBounds().getWidth()
                                   / static_cast<float> (totalSamples);

        if (pixelDistance < kTrimHandlePixels && distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

WaveformComponent::DragMode WaveformComponent::hitTestTrimHandle (float x, int& outSliceIndex) const
{
    outSliceIndex = -1;

    if (totalSamples <= 0 || ! juce::isPositiveAndBelow (selectedSliceIndex, static_cast<int> (sliceMarkers.size())))
        return DragMode::none;

    const auto& slice = sliceMarkers[static_cast<size_t> (selectedSliceIndex)];
    const auto startX = sampleToX (slice.startSample);
    const auto endX = sampleToX (slice.endSample);

    if (std::abs (x - startX) <= kTrimHandlePixels)
    {
        outSliceIndex = selectedSliceIndex;
        return DragMode::trimStart;
    }

    if (std::abs (x - endX) <= kTrimHandlePixels)
    {
        outSliceIndex = selectedSliceIndex;
        return DragMode::trimEnd;
    }

    return DragMode::none;
}

void WaveformComponent::paintSliceRegion (juce::Graphics& g,
                                          const juce::Rectangle<float>& inner,
                                          int sliceIndex,
                                          int startSample,
                                          int endSample,
                                          bool selected) const
{
    const auto x = sampleToX (startSample);
    const auto endX = sampleToX (endSample);

    g.setColour (selected ? SamprLookAndFeel::accent().brighter (0.2f) : juce::Colour (0xffe06c75));
    g.drawLine (x, inner.getY(), x, inner.getBottom(), selected ? 2.0f : 1.0f);

    if (selected)
    {
        g.setColour (SamprLookAndFeel::accent().withAlpha (0.18f));
        g.fillRect (x, inner.getY(), endX - x, inner.getHeight());

        g.setColour (SamprLookAndFeel::accent());
        g.fillRect (x - 2.0f, inner.getY(), 4.0f, inner.getHeight());
        g.fillRect (endX - 2.0f, inner.getY(), 4.0f, inner.getHeight());
    }

    juce::ignoreUnused (sliceIndex);
}

void WaveformComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll (SamprLookAndFeel::panel());
    SamprLookAndFeel::paintPanelOutline (g, bounds);
    const auto inner = getInnerBounds();

    if (peaks.numColumns <= 0 || peaks.minMax.empty())
    {
        g.setColour (SamprLookAndFeel::textMuted());
        g.drawText ("Load a sample to view waveform", inner, juce::Justification::centred);
        return;
    }

    const auto centreY = inner.getCentreY();
    const auto halfHeight = inner.getHeight() * 0.42f;
    juce::Path waveform;

    for (int column = 0; column < peaks.numColumns; ++column)
    {
        const auto idx = static_cast<size_t> (column * 2);
        const auto minVal = peaks.minMax[idx];
        const auto maxVal = peaks.minMax[idx + 1];
        const auto x = inner.getX() + (static_cast<float> (column) / static_cast<float> (peaks.numColumns - 1))
                       * inner.getWidth();
        const auto yTop = centreY - maxVal * halfHeight;
        const auto yBottom = centreY - minVal * halfHeight;

        if (column == 0)
            waveform.startNewSubPath (x, yTop);
        else
            waveform.lineTo (x, yTop);

        waveform.lineTo (x, yBottom);
    }

    g.setColour (SamprLookAndFeel::accent());
    g.strokePath (waveform, juce::PathStrokeType (1.2f));

    for (int i = 0; i < static_cast<int> (sliceMarkers.size()); ++i)
    {
        const auto& slice = sliceMarkers[static_cast<size_t> (i)];
        const auto selected = i == selectedSliceIndex;
        const auto startSample = (trimDragActive && i == dragSliceIndex && dragMode == DragMode::trimStart)
                                     ? dragPreviewStart
                                     : slice.startSample;
        const auto endSample = (trimDragActive && i == dragSliceIndex && dragMode == DragMode::trimEnd)
                                   ? dragPreviewEnd
                                   : slice.endSample;

        paintSliceRegion (g, inner, i, startSample, endSample, selected);
    }
}

void WaveformComponent::mouseDown (const juce::MouseEvent& event)
{
    if (totalSamples <= 0)
        return;

    if (event.mods.isRightButtonDown())
    {
        const auto sliceIndex = hitTestSlice (static_cast<float> (event.x));

        if (sliceIndex >= 0 && onRemoveSlice != nullptr)
            onRemoveSlice (sliceIndex);

        return;
    }

    int sliceIndex = -1;
    const auto trimHandle = hitTestTrimHandle (static_cast<float> (event.x), sliceIndex);

    if (trimHandle != DragMode::none && juce::isPositiveAndBelow (sliceIndex, static_cast<int> (sliceMarkers.size())))
    {
        const auto& slice = sliceMarkers[static_cast<size_t> (sliceIndex)];
        dragMode = trimHandle;
        dragSliceIndex = sliceIndex;
        dragPreviewStart = slice.startSample;
        dragPreviewEnd = slice.endSample;
        trimDragActive = true;
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    const auto markerIndex = hitTestSlice (static_cast<float> (event.x));

    if (markerIndex >= 0)
    {
        if (onSliceClicked != nullptr)
            onSliceClicked (markerIndex);
    }
    else if (onAddSlice != nullptr)
    {
        onAddSlice (xToSample (static_cast<float> (event.x)));
    }
}

void WaveformComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (! trimDragActive || dragSliceIndex < 0
        || ! juce::isPositiveAndBelow (dragSliceIndex, static_cast<int> (sliceMarkers.size())))
        return;

    const auto& slice = sliceMarkers[static_cast<size_t> (dragSliceIndex)];
    const auto samplePos = xToSample (static_cast<float> (event.x));

    if (dragMode == DragMode::trimStart)
    {
        dragPreviewStart = juce::jlimit (0, dragPreviewEnd - 1, samplePos);

        if (dragSliceIndex > 0)
        {
            const auto prevStart = sliceMarkers[static_cast<size_t> (dragSliceIndex - 1)].startSample;
            dragPreviewStart = juce::jmax (dragPreviewStart, prevStart + 1);
        }
    }
    else if (dragMode == DragMode::trimEnd)
    {
        dragPreviewEnd = juce::jlimit (dragPreviewStart + 1, totalSamples, samplePos);

        if (dragSliceIndex + 1 < static_cast<int> (sliceMarkers.size()))
        {
            const auto nextEnd = sliceMarkers[static_cast<size_t> (dragSliceIndex + 1)].endSample;
            dragPreviewEnd = juce::jmin (dragPreviewEnd, nextEnd - 1);
        }
    }
    else
    {
        juce::ignoreUnused (slice);
    }

    repaint();
}

void WaveformComponent::commitTrimDrag()
{
    if (! trimDragActive || dragSliceIndex < 0 || onSliceTrimChanged == nullptr)
    {
        cancelTrimDrag();
        return;
    }

    onSliceTrimChanged (dragSliceIndex, dragPreviewStart, dragPreviewEnd);
    cancelTrimDrag();
}

void WaveformComponent::cancelTrimDrag()
{
    dragMode = DragMode::none;
    dragSliceIndex = -1;
    trimDragActive = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void WaveformComponent::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    commitTrimDrag();
}

void WaveformComponent::mouseMove (const juce::MouseEvent& event)
{
    if (trimDragActive)
        return;

    int sliceIndex = -1;
    const auto handle = hitTestTrimHandle (static_cast<float> (event.x), sliceIndex);
    setMouseCursor (handle != DragMode::none ? juce::MouseCursor::LeftRightResizeCursor
                                             : juce::MouseCursor::NormalCursor);
}

void WaveformComponent::setOpenSliceEditorCallback (OpenSliceEditorCallback callback)
{
    onOpenSliceEditor = std::move (callback);
}

void WaveformComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (onOpenSliceEditor != nullptr)
    {
        onOpenSliceEditor();
        return;
    }

    mouseDown (event);
}

} // namespace sampr