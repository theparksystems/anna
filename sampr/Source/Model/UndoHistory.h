#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "ProjectModel.h"

namespace sampr
{

class UndoHistory
{
public:
    explicit UndoHistory (size_t maxDepthIn = 64) : maxDepth (maxDepthIn) {}

    void saveUndoPoint (const ProjectModel& current);
    bool undo (ProjectModel& current);
    bool redo (ProjectModel& current);

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;

    void clear();

private:
    struct Entry
    {
        ProjectModel state;
        juce::String description;
    };

    std::vector<Entry> undoStack;
    std::vector<Entry> redoStack;
    size_t maxDepth;
};

} // namespace sampr