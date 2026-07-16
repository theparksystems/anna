#include "PianoRollComponent.h"

#include "SamprLookAndFeel.h"

#include <cmath>

namespace sampr
{

namespace
{
    bool isBlackKey (int pitch)
    {
        const auto n = pitch % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    juce::String pitchName (int pitch)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto octave = pitch / 12 - 1;
        return juce::String (names[pitch % 12]) + juce::String (octave);
    }
}

PianoRollComponent::PianoRollComponent (PatternStore& patterns, SampleManager& samples)
    : patternStore (patterns),
      sampleManager (samples)
{
    addAndMakeVisible (quantizeBox);
    addAndMakeVisible (lengthBarsBox);
    addAndMakeVisible (clearNotesButton);
    addAndMakeVisible (zoomLabel);

    quantizeBox.addItem ("1/4", 1);
    quantizeBox.addItem ("1/8", 2);
    quantizeBox.addItem ("1/16", 3);
    quantizeBox.addItem ("1/32", 4);
    quantizeBox.setSelectedId (3, juce::dontSendNotification);
    quantizeBox.onChange = [this]
    {
        switch (quantizeBox.getSelectedId())
        {
            case 1: quantizeBeats = 1.0; break;
            case 2: quantizeBeats = 0.5; break;
            case 3: quantizeBeats = 0.25; break;
            case 4: quantizeBeats = 0.125; break;
            default: quantizeBeats = 0.25; break;
        }
    };

    for (int bars = 1; bars <= 8; ++bars)
        lengthBarsBox.addItem (juce::String (bars) + (bars == 1 ? " bar" : " bars"), bars);

    lengthBarsBox.setSelectedId (1, juce::dontSendNotification);
    lengthBarsBox.onChange = [this]
    {
        patternStore.setLengthBars (lengthBarsBox.getSelectedId());
        if (onChange != nullptr)
            onChange();
        repaint();
    };

    clearNotesButton.onClick = [this]
    {
        patternStore.clearNotes();
        selectedNoteId = kInvalidNoteId;
        if (onChange != nullptr)
            onChange();
        refresh();
    };

    zoomLabel.setText ("Zoom: wheel", juce::dontSendNotification);
    refresh();
}

void PianoRollComponent::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void PianoRollComponent::setPlayheadBeats (double beats)
{
    if (std::abs (playheadBeats - beats) < 0.0001)
        return;

    playheadBeats = beats;
    repaint();
}

void PianoRollComponent::refresh()
{
    const auto& pattern = patternStore.getCurrentPattern();
    lengthBarsBox.setSelectedId (pattern.lengthBars, juce::dontSendNotification);
    repaint();
}

juce::Rectangle<int> PianoRollComponent::getGridArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop (layout.toolbarHeight + 2);
    area.removeFromBottom (layout.velocityLaneHeight + 4);
    area.removeFromLeft (layout.keyboardWidth);
    return area.reduced (0, 2);
}

juce::Rectangle<int> PianoRollComponent::getVelocityLaneArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop (layout.toolbarHeight + 2);
    area = area.removeFromBottom (layout.velocityLaneHeight);
    area.removeFromLeft (layout.keyboardWidth);
    return area.reduced (0, 2);
}

double PianoRollComponent::xToBeats (float x) const
{
    const auto grid = getGridArea();
    return static_cast<double> (x - static_cast<float> (grid.getX())) / pixelsPerBeat + scrollBeats;
}

float PianoRollComponent::beatsToX (double beats) const
{
    const auto grid = getGridArea();
    return static_cast<float> (grid.getX()) + static_cast<float> ((beats - scrollBeats) * pixelsPerBeat);
}

int PianoRollComponent::yToPitch (float y) const
{
    const auto grid = getGridArea();
    const auto row = static_cast<int> ((y - static_cast<float> (grid.getY())) / static_cast<float> (layout.rowHeight));
    const auto pitch = kPianoRollHighPitch - row - scrollPitchOffset;
    return juce::jlimit (kPianoRollLowPitch, kPianoRollHighPitch, pitch);
}

float PianoRollComponent::pitchToY (int pitch) const
{
    const auto grid = getGridArea();
    const auto row = kPianoRollHighPitch - pitch - scrollPitchOffset;
    return static_cast<float> (grid.getY()) + static_cast<float> (row * layout.rowHeight);
}

double PianoRollComponent::snapBeats (double beats) const
{
    if (quantizeBeats <= 0.0)
        return beats;

    return std::round (beats / quantizeBeats) * quantizeBeats;
}

juce::Rectangle<float> PianoRollComponent::getNoteBounds (const NoteEvent& note) const
{
    const auto x = beatsToX (note.startBeats);
    const auto w = static_cast<float> (note.durationBeats * pixelsPerBeat);
    const auto y = pitchToY (note.pitch);
    return { x, y, juce::jmax (4.0f, w), static_cast<float> (layout.rowHeight - 2) };
}

NoteId PianoRollComponent::hitTestNote (juce::Point<float> pos, bool& nearRightEdge) const
{
    nearRightEdge = false;
    const auto& notes = patternStore.getCurrentPattern().notes;

    for (auto it = notes.rbegin(); it != notes.rend(); ++it)
    {
        auto bounds = getNoteBounds (*it);

        if (bounds.contains (pos))
        {
            nearRightEdge = pos.x > bounds.getRight() - 8.0f;
            return it->id;
        }
    }

    return kInvalidNoteId;
}

void PianoRollComponent::createNoteAt (double beats, int pitch)
{
    const auto assetId = sampleManager.getSelectedAssetId();
    const auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
        return;

    NoteEvent note;
    note.startBeats = snapBeats (juce::jmax (0.0, beats));
    note.durationBeats = quantizeBeats;
    note.pitch = pitch;
    note.velocity = 0.85f;
    note.assetId = assetId;
    note.sliceIndex = asset->selectedSliceIndex;

    const auto loopBeats = static_cast<double> (patternStore.getCurrentPattern().lengthBars) * 4.0;

    if (note.startBeats >= loopBeats)
        return;

    selectedNoteId = patternStore.addNote (note);

    if (onChange != nullptr)
        onChange();

    repaint();
}

void PianoRollComponent::deleteSelectedNote()
{
    if (selectedNoteId == kInvalidNoteId)
        return;

    patternStore.deleteNote (selectedNoteId);
    selectedNoteId = kInvalidNoteId;

    if (onChange != nullptr)
        onChange();

    repaint();
}

void PianoRollComponent::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff161c25), 0.0f, 0.0f,
                                             juce::Colour (0xff0d1118), 0.0f, static_cast<float> (getHeight()),
                                             false));
    g.fillRect (getLocalBounds());

    auto keyboardArea = getLocalBounds();
    keyboardArea.removeFromTop (layout.toolbarHeight + 2);
    keyboardArea.removeFromBottom (layout.velocityLaneHeight + 4);
    keyboardArea.setWidth (layout.keyboardWidth);
    paintKeyboard (g, keyboardArea);

    paintGrid (g, getGridArea());
    paintNotes (g, getGridArea());
    paintPlayhead (g, getGridArea());
    paintVelocityLane (g, getVelocityLaneArea());
}

void PianoRollComponent::paintKeyboard (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff1c2230));
    g.fillRect (area);

    for (int pitch = kPianoRollHighPitch; pitch >= kPianoRollLowPitch; --pitch)
    {
        const auto y = static_cast<int> (pitchToY (pitch));
        const auto row = juce::Rectangle<int> (area.getX(), y, area.getWidth(), layout.rowHeight);

        if (row.getBottom() < area.getY() || row.getY() > area.getBottom())
            continue;

        g.setColour (isBlackKey (pitch) ? juce::Colour (0xff11151d) : juce::Colour (0xff2a3142));
        g.fillRect (row);

        if (pitch % 12 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (juce::FontOptions { 10.0f });
            g.drawText (pitchName (pitch), row.reduced (4, 0), juce::Justification::centredLeft, false);
        }
    }
}

void PianoRollComponent::paintGrid (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff121821));
    g.fillRect (area);

    const auto loopBeats = static_cast<double> (patternStore.getCurrentPattern().lengthBars) * 4.0;
    const auto visibleBeats = static_cast<double> (area.getWidth()) / pixelsPerBeat;

    for (double beat = 0.0; beat <= loopBeats + visibleBeats; beat += quantizeBeats)
    {
        const auto x = beatsToX (beat);

        if (x < static_cast<float> (area.getX()) || x > static_cast<float> (area.getRight()))
            continue;

        const bool isBar = std::fmod (beat, 4.0) < 0.0001;
        const bool isBeat = std::fmod (beat, 1.0) < 0.0001;
        g.setColour (isBar ? juce::Colour (0xff68748b)
                           : (isBeat ? juce::Colour (0xff394558) : juce::Colour (0xff252d3a)));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (area.getY()),
                            static_cast<float> (area.getBottom()));
    }

    for (int pitch = kPianoRollLowPitch; pitch <= kPianoRollHighPitch; ++pitch)
    {
        const auto y = pitchToY (pitch);

        if (y < static_cast<float> (area.getY()) || y > static_cast<float> (area.getBottom()))
            continue;

        g.setColour (isBlackKey (pitch) ? juce::Colour (0xff1c222d) : juce::Colour (0xff26303e));
        g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (area.getX()),
                              static_cast<float> (area.getRight()));
    }

    const auto loopX = beatsToX (loopBeats);
    g.setColour (juce::Colour (0x88ffcc66));
    g.drawLine (loopX, static_cast<float> (area.getY()),
                loopX, static_cast<float> (area.getBottom()), 2.0f);
}

void PianoRollComponent::paintNotes (juce::Graphics& g, juce::Rectangle<int> area)
{
    for (const auto& note : patternStore.getCurrentPattern().notes)
    {
        auto bounds = getNoteBounds (note);

        if (! bounds.intersects (area.toFloat()))
            continue;

        const bool selected = note.id == selectedNoteId;
        const auto noteBounds = bounds.reduced (1.0f);
        const auto colour = selected ? SamprLookAndFeel::accent().brighter (0.15f) : SamprLookAndFeel::accent();
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.18f), noteBounds.getX(), noteBounds.getY(),
                                                 colour.darker (0.18f), noteBounds.getX(), noteBounds.getBottom(),
                                                 false));
        g.fillRoundedRectangle (noteBounds, 3.0f);

        if (selected)
        {
            g.setColour (SamprLookAndFeel::textPrimary());
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.4f);
        }
    }
}

void PianoRollComponent::paintPlayhead (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto loopBeats = static_cast<double> (patternStore.getCurrentPattern().lengthBars) * 4.0;
    const auto wrapped = std::fmod (playheadBeats, loopBeats);
    const auto x = beatsToX (wrapped);

    if (x >= static_cast<float> (area.getX()) && x <= static_cast<float> (area.getRight()))
    {
        g.setColour (juce::Colour (0xeeffffff));
        g.drawLine (x, static_cast<float> (area.getY()),
                    x, static_cast<float> (area.getBottom()), 2.0f);
    }
}

void PianoRollComponent::paintVelocityLane (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff1a2030));
    g.fillRect (area);

    g.setColour (juce::Colour (0xff8b95a8));
    g.setFont (juce::FontOptions { 10.0f });
    g.drawText ("Velocity", area.removeFromLeft (56), juce::Justification::centred);

    for (const auto& note : patternStore.getCurrentPattern().notes)
    {
        const auto x = beatsToX (note.startBeats);
        const auto w = static_cast<float> (note.durationBeats * pixelsPerBeat);
        const auto h = note.velocity * static_cast<float> (area.getHeight() - 6);
        const auto bar = juce::Rectangle<float> (x, static_cast<float> (area.getBottom()) - h - 2.0f,
                                                 juce::jmax (3.0f, w), h);

        g.setColour (note.id == selectedNoteId ? juce::Colour (0xffffb347) : juce::Colour (0xff6ec6ff));
        g.fillRect (bar);
    }
}

void PianoRollComponent::resized()
{
    auto toolbar = getLocalBounds().reduced (4, 2);
    toolbar.setHeight (layout.toolbarHeight);

    auto x = toolbar.getX();
    quantizeBox.setBounds (x, toolbar.getY(), 80, toolbar.getHeight());
    x += 86;
    lengthBarsBox.setBounds (x, toolbar.getY(), 90, toolbar.getHeight());
    x += 96;
    clearNotesButton.setBounds (x, toolbar.getY(), 90, toolbar.getHeight());
    x += 96;
    zoomLabel.setBounds (x, toolbar.getY(), 120, toolbar.getHeight());
}

void PianoRollComponent::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isMiddleButtonDown() || event.mods.isShiftDown())
    {
        dragMode = DragMode::panTimeline;
        dragStartPos = event.position;
        return;
    }

    const auto grid = getGridArea();
    const auto velocityLane = getVelocityLaneArea();

    if (velocityLane.contains (event.getPosition()))
    {
        bool edge = false;
        const auto hit = hitTestNote (event.position, edge);

        if (hit != kInvalidNoteId)
        {
            selectedNoteId = hit;
            dragMode = DragMode::velocity;

            if (auto* note = patternStore.findNote (hit))
                dragStartNote = *note;
        }

        repaint();
        return;
    }

    if (! grid.contains (event.getPosition()))
        return;

    bool nearRightEdge = false;
    const auto hit = hitTestNote (event.position, nearRightEdge);

    if (hit != kInvalidNoteId)
    {
        selectedNoteId = hit;

        if (auto* note = patternStore.findNote (hit))
            dragStartNote = *note;

        dragMode = nearRightEdge ? DragMode::resizeNote : DragMode::moveNote;
        dragStartPos = event.position;
        repaint();
        return;
    }

    if (event.mods.isRightButtonDown())
        return;

    createNoteAt (xToBeats (event.x), yToPitch (event.y));
}

void PianoRollComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (dragMode == DragMode::panTimeline)
    {
        scrollBeats = juce::jmax (0.0, scrollBeats - static_cast<double> (event.getDistanceFromDragStartX()) / pixelsPerBeat);
        scrollPitchOffset = juce::jlimit (-12, 12,
                                          scrollPitchOffset + event.getDistanceFromDragStartY() / layout.rowHeight);
        repaint();
        return;
    }

    if (selectedNoteId == kInvalidNoteId)
        return;

    auto* note = patternStore.findNote (selectedNoteId);

    if (note == nullptr)
        return;

    if (dragMode == DragMode::moveNote)
    {
        const auto deltaBeats = static_cast<double> (event.getDistanceFromDragStartX()) / pixelsPerBeat;
        const auto deltaPitch = -event.getDistanceFromDragStartY() / layout.rowHeight;

        note->startBeats = snapBeats (juce::jmax (0.0, dragStartNote.startBeats + deltaBeats));
        note->pitch = juce::jlimit (kPianoRollLowPitch, kPianoRollHighPitch,
                                    dragStartNote.pitch + static_cast<int> (std::lround (deltaPitch)));
        patternStore.updateNote (*note);

        if (onChange != nullptr)
            onChange();

        repaint();
        return;
    }

    if (dragMode == DragMode::resizeNote)
    {
        const auto deltaBeats = static_cast<double> (event.getDistanceFromDragStartX()) / pixelsPerBeat;
        note->durationBeats = juce::jmax (quantizeBeats, snapBeats (dragStartNote.durationBeats + deltaBeats));
        patternStore.updateNote (*note);

        if (onChange != nullptr)
            onChange();

        repaint();
        return;
    }

    if (dragMode == DragMode::velocity)
    {
        const auto lane = getVelocityLaneArea();
        const auto rel = juce::jlimit (0.05f, 1.0f,
                                       1.0f - (event.y - static_cast<float> (lane.getY())) / static_cast<float> (lane.getHeight()));
        note->velocity = rel;
        patternStore.updateNote (*note);

        if (onChange != nullptr)
            onChange();

        repaint();
    }
}

void PianoRollComponent::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    dragMode = DragMode::none;
}

void PianoRollComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused (event);

    if (event.mods.isCtrlDown())
    {
        pixelsPerBeat = juce::jlimit (24.0, 200.0, pixelsPerBeat + wheel.deltaY * 12.0);
        zoomLabel.setText ("Zoom: " + juce::String (static_cast<int> (pixelsPerBeat)) + " px/beat",
                           juce::dontSendNotification);
    }
    else
    {
        scrollBeats = juce::jmax (0.0, scrollBeats - wheel.deltaX * 0.5);
        scrollPitchOffset = juce::jlimit (-12, 12, scrollPitchOffset + static_cast<int> (wheel.deltaY * 2));
    }

    repaint();
}

bool PianoRollComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedNote();
        return true;
    }

    return false;
}

} // namespace sampr
