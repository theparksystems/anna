#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"
#include "../Model/SampleManager.h"

namespace sampr
{

class PianoRollComponent final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    PianoRollComponent (PatternStore& patterns, SampleManager& samples);

    void setChangeCallback (ChangeCallback callback);
    void setPlayheadBeats (double beats);
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    enum class DragMode
    {
        none,
        moveNote,
        resizeNote,
        velocity,
        panTimeline
    };

    struct Layout
    {
        int keyboardWidth = 52;
        int toolbarHeight = 30;
        int velocityLaneHeight = 44;
        int rowHeight = 14;
    };

    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getVelocityLaneArea() const;
    double xToBeats (float x) const;
    float beatsToX (double beats) const;
    int yToPitch (float y) const;
    float pitchToY (int pitch) const;
    double snapBeats (double beats) const;
    juce::Rectangle<float> getNoteBounds (const NoteEvent& note) const;
    NoteId hitTestNote (juce::Point<float> pos, bool& nearRightEdge) const;
    void createNoteAt (double beats, int pitch);
    void deleteSelectedNote();
    void paintKeyboard (juce::Graphics& g, juce::Rectangle<int> area);
    void paintGrid (juce::Graphics& g, juce::Rectangle<int> area);
    void paintNotes (juce::Graphics& g, juce::Rectangle<int> area);
    void paintVelocityLane (juce::Graphics& g, juce::Rectangle<int> area);
    void paintPlayhead (juce::Graphics& g, juce::Rectangle<int> area);

    PatternStore& patternStore;
    SampleManager& sampleManager;

    juce::ComboBox quantizeBox;
    juce::ComboBox lengthBarsBox;
    juce::TextButton clearNotesButton { "Clear Notes" };
    juce::Label zoomLabel;

    Layout layout;
    double pixelsPerBeat = 72.0;
    double scrollBeats = 0.0;
    int scrollPitchOffset = 0;
    double quantizeBeats = 0.25;
    double playheadBeats = 0.0;

    NoteId selectedNoteId = kInvalidNoteId;
    DragMode dragMode = DragMode::none;
    juce::Point<float> dragStartPos;
    NoteEvent dragStartNote;
    ChangeCallback onChange;
};

} // namespace sampr