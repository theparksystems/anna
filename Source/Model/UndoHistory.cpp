#include "UndoHistory.h"

namespace sampr
{

void UndoHistory::saveUndoPoint (const ProjectModel& current)
{
    Entry entry;
    entry.state = current.clone();
    entry.description = "Edit";

    undoStack.push_back (std::move (entry));

    if (undoStack.size() > maxDepth)
        undoStack.erase (undoStack.begin());

    redoStack.clear();
}

bool UndoHistory::undo (ProjectModel& current)
{
    if (undoStack.empty())
        return false;

    Entry redoEntry;
    redoEntry.state = current.clone();
    redoEntry.description = "Redo";
    redoStack.push_back (std::move (redoEntry));

    const auto entry = std::move (undoStack.back());
    undoStack.pop_back();
    current.assignFrom (entry.state);
    return true;
}

bool UndoHistory::redo (ProjectModel& current)
{
    if (redoStack.empty())
        return false;

    Entry undoEntry;
    undoEntry.state = current.clone();
    undoEntry.description = "Undo";
    undoStack.push_back (std::move (undoEntry));

    const auto entry = std::move (redoStack.back());
    redoStack.pop_back();
    current.assignFrom (entry.state);
    return true;
}

juce::String UndoHistory::getUndoDescription() const
{
    return undoStack.empty() ? juce::String() : undoStack.back().description;
}

juce::String UndoHistory::getRedoDescription() const
{
    return redoStack.empty() ? juce::String() : redoStack.back().description;
}

void UndoHistory::clear()
{
    undoStack.clear();
    redoStack.clear();
}

} // namespace sampr