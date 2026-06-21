#include "FxWorkspaceComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

FxWorkspaceComponent::FxWorkspaceComponent (PatternStore& store)
    : patternStore (store),
      eqPanel (store),
      compressorPanel (store),
      delayPanel (store),
      reverbPanel (store)
{
    titleLabel.setText ("FX Rack", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
    channelLabel.setFont (juce::FontOptions { 11.0f });
    channelLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textMuted());

    channelBox.onChange = [this]
    {
        channelIndex = channelBox.getSelectedItemIndex();
        syncEditors();
    };

    const auto notify = [this]
    {
        if (onChange != nullptr)
            onChange();
    };

    eqPanel.setChangeCallback (notify);
    compressorPanel.setChangeCallback (notify);
    delayPanel.setChangeCallback (notify);
    reverbPanel.setChangeCallback (notify);

    fxTabs.addTab ("EQ", SamprLookAndFeel::panel(), &eqPanel, false);
    fxTabs.addTab ("Compressor", SamprLookAndFeel::panel(), &compressorPanel, false);
    fxTabs.addTab ("Delay", SamprLookAndFeel::panel(), &delayPanel, false);
    fxTabs.addTab ("Reverb", SamprLookAndFeel::panel(), &reverbPanel, false);
    fxTabs.setCurrentTabIndex (0);

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (channelLabel);
    addAndMakeVisible (channelBox);
    addAndMakeVisible (fxTabs);

    rebuildChannelList();
}

void FxWorkspaceComponent::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void FxWorkspaceComponent::setChannel (int rowIndex)
{
    channelIndex = juce::jmax (0, rowIndex);
    rebuildChannelList();
    syncEditors();
}

void FxWorkspaceComponent::refreshFromStore()
{
    rebuildChannelList();
    syncEditors();
}

void FxWorkspaceComponent::rebuildChannelList()
{
    const auto& pattern = patternStore.getCurrentPattern();
    const auto previous = channelIndex;
    channelBox.clear();

    if (pattern.rows.empty())
    {
        channelBox.addItem ("No channels", 1);
        channelBox.setSelectedId (1, juce::dontSendNotification);
        channelIndex = -1;
        updateTitle();
        fxTabs.setEnabled (false);
        return;
    }

    for (int i = 0; i < static_cast<int> (pattern.rows.size()); ++i)
        channelBox.addItem (juce::String (i + 1) + ": " + pattern.rows[static_cast<size_t> (i)].label, i + 1);

    channelIndex = juce::jlimit (0, static_cast<int> (pattern.rows.size()) - 1, previous);
    channelBox.setSelectedId (channelIndex + 1, juce::dontSendNotification);
    fxTabs.setEnabled (true);
    updateTitle();
}

void FxWorkspaceComponent::updateTitle()
{
    if (channelIndex < 0)
    {
        titleLabel.setText ("FX Rack", juce::dontSendNotification);
        return;
    }

    const auto& pattern = patternStore.getCurrentPattern();

    if (! juce::isPositiveAndBelow (channelIndex, static_cast<int> (pattern.rows.size())))
        return;

    const auto& row = pattern.rows[static_cast<size_t> (channelIndex)];
    titleLabel.setText ("FX — Ch " + juce::String (channelIndex + 1) + ": " + row.label,
                        juce::dontSendNotification);
}

void FxWorkspaceComponent::syncEditors()
{
    updateTitle();

    if (channelIndex < 0)
    {
        eqPanel.clearChannel();
        compressorPanel.clearChannel();
        delayPanel.clearChannel();
        reverbPanel.clearChannel();
        repaint();
        return;
    }

    eqPanel.setChannel (channelIndex);
    compressorPanel.setChannel (channelIndex);
    delayPanel.setChannel (channelIndex);
    reverbPanel.setChannel (channelIndex);
}

void FxWorkspaceComponent::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::background());
    SamprLookAndFeel::paintHorizontalDivider (g, 0, getWidth());

    if (channelIndex < 0)
    {
        auto area = getLocalBounds().reduced (8);
        area.removeFromTop (36);
        g.setColour (SamprLookAndFeel::textMuted());
        g.setFont (juce::FontOptions { 12.0f });
        g.drawText ("Add channels in Step Sequencer (+ Row), then return here to shape your sound.",
                    area,
                    juce::Justification::centred);
    }
}

void FxWorkspaceComponent::resized()
{
    auto area = getLocalBounds().reduced (4);
    auto header = area.removeFromTop (28);
    titleLabel.setBounds (header.removeFromLeft (juce::jmax (180, header.getWidth() / 3)));
    header.removeFromLeft (8);
    channelLabel.setBounds (header.removeFromLeft (52));
    channelBox.setBounds (header);
    area.removeFromTop (4);
    fxTabs.setBounds (area);
}

} // namespace sampr