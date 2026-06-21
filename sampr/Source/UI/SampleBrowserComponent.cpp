#include "SampleBrowserComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

SampleBrowserComponent::SampleBrowserComponent (SampleManager& manager)
    : sampleManager (manager)
{
    titleLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    titleLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textPrimary());

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (listBox);
    addAndMakeVisible (loadButton);

    loadButton.setTooltip ("Load audio files (Ctrl+Shift+L). You can also drag and drop anywhere.");
    listBox.setTooltip ("Selected sample drives the chop area, piano roll recording, and + Row.");

    loadButton.onClick = [this]
    {
        if (onLoadRequested != nullptr)
            onLoadRequested();
    };

    listBox.setMultipleSelectionEnabled (false);
    refresh();
}

void SampleBrowserComponent::setSelectionCallback (SelectionCallback callback)
{
    onSelectionChanged = std::move (callback);
}

void SampleBrowserComponent::setLoadRequestedCallback (LoadRequestedCallback callback)
{
    onLoadRequested = std::move (callback);
}

void SampleBrowserComponent::refresh()
{
    listBox.updateContent();
    listBox.repaint();

    const auto ids = sampleManager.getAssetIds();
    const auto selected = sampleManager.getSelectedAssetId();

    for (int i = 0; i < static_cast<int> (ids.size()); ++i)
    {
        if (ids[static_cast<size_t> (i)] == selected)
        {
            listBox.selectRow (i);
            break;
        }
    }
}

int SampleBrowserComponent::getNumRows()
{
    return static_cast<int> (sampleManager.getAssetIds().size());
}

void SampleBrowserComponent::paintListBoxItem (int rowNumber,
                                               juce::Graphics& g,
                                               int width,
                                               int height,
                                               bool rowIsSelected)
{
    const auto& ids = sampleManager.getAssetIds();

    if (! juce::isPositiveAndBelow (rowNumber, static_cast<int> (ids.size())))
        return;

    if (rowIsSelected)
        g.fillAll (SamprLookAndFeel::accent().withAlpha (0.18f));

    const auto* asset = sampleManager.getAsset (ids[static_cast<size_t> (rowNumber)]);

    if (asset == nullptr)
        return;

    g.setColour (rowIsSelected ? SamprLookAndFeel::textPrimary() : SamprLookAndFeel::textMuted());
    g.setFont (juce::FontOptions { 12.0f });
    g.drawText (asset->displayName, 4, 0, width - 6, height, juce::Justification::centredLeft, true);
}

void SampleBrowserComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    const auto& ids = sampleManager.getAssetIds();

    if (! juce::isPositiveAndBelow (row, static_cast<int> (ids.size())))
        return;

    const auto assetId = ids[static_cast<size_t> (row)];
    sampleManager.setSelectedAssetId (assetId);

    if (onSelectionChanged != nullptr)
        onSelectionChanged (assetId);
}

void SampleBrowserComponent::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
    SamprLookAndFeel::paintPanelOutline (g, getLocalBounds());

    if (sampleManager.getAssetIds().empty())
    {
        auto area = getLocalBounds().reduced (8);
        area.removeFromTop (56);
        g.setColour (SamprLookAndFeel::textMuted());
        g.setFont (juce::FontOptions { 11.0f });
        g.drawText ("No samples yet.\nLoad files or drop audio onto the window.",
                    area,
                    juce::Justification::centred);
    }
}

void SampleBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced (4);
    titleLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (2);
    loadButton.setBounds (area.removeFromTop (26));
    area.removeFromTop (4);
    listBox.setBounds (area);
}

} // namespace sampr