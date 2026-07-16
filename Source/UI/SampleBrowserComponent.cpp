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
    addAndMakeVisible (copyCreditsButton);

    loadButton.setTooltip ("Load audio files (Ctrl+Shift+L). You can also drag and drop anywhere.");
    copyCreditsButton.setTooltip ("Copy sampled-from credits for sourced samples.");
    listBox.setTooltip ("Selected sample drives the chop area, piano roll recording, and + Row.");

    loadButton.onClick = [this]
    {
        if (onLoadRequested != nullptr)
            onLoadRequested();
    };

    copyCreditsButton.onClick = [this]
    {
        juce::String credits;

        for (const auto assetId : sampleManager.getAssetIds())
        {
            const auto* asset = sampleManager.getAsset (assetId);

            if (asset == nullptr || ! asset->origin.hasSource())
                continue;

            const auto title = asset->origin.sourceTitle.isNotEmpty() ? asset->origin.sourceTitle : asset->displayName;
            credits << "- " << title;

            if (asset->origin.sourceAuthor.isNotEmpty())
                credits << " by " << asset->origin.sourceAuthor;

            if (asset->origin.sourceUrl.isNotEmpty())
                credits << " - " << asset->origin.sourceUrl;

            credits << "\n";
        }

        if (credits.isEmpty())
            credits = "No sourced samples loaded.";

        juce::SystemClipboard::copyTextToClipboard (credits);
    };

    listBox.setMultipleSelectionEnabled (false);
    listBox.setRowHeight (42);
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
    {
        g.setGradientFill (juce::ColourGradient (SamprLookAndFeel::accent().withAlpha (0.24f), 0.0f, 0.0f,
                                                 SamprLookAndFeel::accentCool().withAlpha (0.12f), static_cast<float> (width), 0.0f,
                                                 false));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 2.0f, static_cast<float> (width - 4), static_cast<float> (height - 4)), 4.0f);
    }
    else
    {
        g.setColour ((rowNumber % 2 == 0 ? SamprLookAndFeel::surface() : SamprLookAndFeel::panel()).withAlpha (0.52f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 2.0f, static_cast<float> (width - 4), static_cast<float> (height - 4)), 4.0f);
    }

    const auto* asset = sampleManager.getAsset (ids[static_cast<size_t> (rowNumber)]);

    if (asset == nullptr)
        return;

    auto area = juce::Rectangle<int> (4, 0, width - 8, height).reduced (0, 4);

    g.setColour (rowIsSelected ? SamprLookAndFeel::textPrimary() : SamprLookAndFeel::textMuted());
    g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    g.drawText (asset->displayName, area.removeFromTop (18), juce::Justification::centredLeft, true);

    const auto hasSource = asset->origin.hasSource();
    const auto sourcePrefix = asset->origin.sourceType == "youtube" ? juce::String ("YT") : juce::String ("SRC");
    const auto sourceLabel = hasSource
        ? (sourcePrefix + "  "
           + (asset->origin.sourceTitle.isNotEmpty() ? asset->origin.sourceTitle : asset->origin.sourceUrl))
        : juce::String ("LOCAL  no source metadata");

    g.setColour (hasSource ? SamprLookAndFeel::accent().withAlpha (0.9f)
                           : SamprLookAndFeel::textMuted().withAlpha (0.7f));
    g.setFont (juce::FontOptions { 10.0f });
    g.drawText (sourceLabel, area, juce::Justification::centredLeft, true);
}

void SampleBrowserComponent::listBoxItemClicked (int row, const juce::MouseEvent& event)
{
    const auto& ids = sampleManager.getAssetIds();

    if (! juce::isPositiveAndBelow (row, static_cast<int> (ids.size())))
        return;

    const auto assetId = ids[static_cast<size_t> (row)];
    sampleManager.setSelectedAssetId (assetId);

    if (event.mods.isRightButtonDown())
    {
        if (const auto* asset = sampleManager.getAsset (assetId);
            asset != nullptr && asset->origin.sourceUrl.isNotEmpty())
        {
            juce::SystemClipboard::copyTextToClipboard (asset->origin.sourceUrl);
        }
    }

    if (onSelectionChanged != nullptr)
        onSelectionChanged (assetId);
}

juce::var SampleBrowserComponent::getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe)
{
    const auto row = rowsToDescribe[0];
    const auto& ids = sampleManager.getAssetIds();

    if (! juce::isPositiveAndBelow (row, static_cast<int> (ids.size())))
        return {};

    return "anna-sample:" + juce::String (ids[static_cast<size_t> (row)]);
}

void SampleBrowserComponent::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (SamprLookAndFeel::panel().brighter (0.05f), 0.0f, 0.0f,
                                             SamprLookAndFeel::background().brighter (0.03f), 0.0f, static_cast<float> (getHeight()),
                                             false));
    g.fillRect (getLocalBounds());
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
    auto buttons = area.removeFromTop (26);
    const auto buttonWidth = (buttons.getWidth() - 4) / 2;
    loadButton.setBounds (buttons.removeFromLeft (buttonWidth));
    buttons.removeFromLeft (4);
    copyCreditsButton.setBounds (buttons);
    area.removeFromTop (4);
    listBox.setBounds (area);
}

} // namespace sampr
