#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/SampleManager.h"

namespace sampr
{

class SampleBrowserComponent final : public juce::Component,
                                     public juce::ListBoxModel
{
public:
    using SelectionCallback = std::function<void (AssetId assetId)>;
    using LoadRequestedCallback = std::function<void()>;

    explicit SampleBrowserComponent (SampleManager& manager);

    void setSelectionCallback (SelectionCallback callback);
    void setLoadRequestedCallback (LoadRequestedCallback callback);
    void refresh();

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    SampleManager& sampleManager;
    juce::Label titleLabel { {}, "Samples" };
    juce::ListBox listBox { "Samples", this };
    juce::TextButton loadButton { "Load..." };
    juce::TextButton copyCreditsButton { "Copy Credits" };

    SelectionCallback onSelectionChanged;
    LoadRequestedCallback onLoadRequested;
};

} // namespace sampr
