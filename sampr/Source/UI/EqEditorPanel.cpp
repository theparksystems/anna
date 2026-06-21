#include "EqEditorPanel.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

EqEditorPanel::EqEditorPanel (PatternStore& store)
    : patternStore (store)
{
    headerLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    headerLabel.setJustificationType (juce::Justification::centredLeft);

    enabledButton.onClick = [this]
    {
        if (syncing || channelIndex < 0)
            return;

        patternStore.setChannelEqEnabled (channelIndex, enabledButton.getToggleState());
        notifyChanged();
    };

    setupBand (lowBand, "Low", 30.0, 500.0, EqBandKind::lowShelf);
    setupBand (midBand, "Mid", 200.0, 8000.0, EqBandKind::peak);
    setupBand (highBand, "High", 2000.0, 20000.0, EqBandKind::highShelf);

    addAndMakeVisible (headerLabel);
    addAndMakeVisible (enabledButton);
    setVisible (false);
}

void EqEditorPanel::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void EqEditorPanel::setupBand (BandControls& band,
                               const juce::String& title,
                               double minFreq,
                               double maxFreq,
                               EqBandKind kind)
{
    band.title.setText (title, juce::dontSendNotification);
    band.title.setJustificationType (juce::Justification::centred);
    band.freqLabel.setText ("Hz", juce::dontSendNotification);
    band.gainLabel.setText ("dB", juce::dontSendNotification);
    band.qLabel.setText ("Q", juce::dontSendNotification);

    configureBandSlider (band.freqSlider, minFreq, maxFreq, " Hz");
    configureBandSlider (band.gainSlider, -24.0, 24.0, " dB");
    configureBandSlider (band.qSlider, 0.1, 10.0, "");

    band.freqSlider.onValueChange = [this, kind] { handleBandChanged (kind); };
    band.gainSlider.onValueChange = [this, kind] { handleBandChanged (kind); };
    band.qSlider.onValueChange = [this, kind] { handleBandChanged (kind); };

    addAndMakeVisible (band.title);
    addAndMakeVisible (band.freqSlider);
    addAndMakeVisible (band.gainSlider);
    addAndMakeVisible (band.qSlider);
    addAndMakeVisible (band.freqLabel);
    addAndMakeVisible (band.gainLabel);
    addAndMakeVisible (band.qLabel);
}

void EqEditorPanel::configureBandSlider (juce::Slider& slider,
                                         double min,
                                         double max,
                                         const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    slider.setRange (min, max, 0.0);
    slider.setTextValueSuffix (suffix);
}

void EqEditorPanel::setChannel (int rowIndex)
{
    channelIndex = rowIndex;
    setVisible (channelIndex >= 0);
    syncFromStore();
}

void EqEditorPanel::clearChannel()
{
    channelIndex = -1;
    setVisible (false);
}

void EqEditorPanel::refreshFromStore()
{
    syncFromStore();
}

void EqEditorPanel::syncFromStore()
{
    if (channelIndex < 0)
        return;

    const auto& pattern = patternStore.getCurrentPattern();

    if (! juce::isPositiveAndBelow (channelIndex, static_cast<int> (pattern.rows.size())))
    {
        clearChannel();
        return;
    }

    const auto& row = pattern.rows[static_cast<size_t> (channelIndex)];
    const auto& eq = row.channelEq;

    syncing = true;
    headerLabel.setText ("EQ — " + row.label, juce::dontSendNotification);
    enabledButton.setToggleState (eq.enabled, juce::dontSendNotification);

    lowBand.freqSlider.setValue (eq.low.frequencyHz, juce::dontSendNotification);
    lowBand.gainSlider.setValue (eq.low.gainDb, juce::dontSendNotification);
    lowBand.qSlider.setValue (eq.low.q, juce::dontSendNotification);

    midBand.freqSlider.setValue (eq.mid.frequencyHz, juce::dontSendNotification);
    midBand.gainSlider.setValue (eq.mid.gainDb, juce::dontSendNotification);
    midBand.qSlider.setValue (eq.mid.q, juce::dontSendNotification);

    highBand.freqSlider.setValue (eq.high.frequencyHz, juce::dontSendNotification);
    highBand.gainSlider.setValue (eq.high.gainDb, juce::dontSendNotification);
    highBand.qSlider.setValue (eq.high.q, juce::dontSendNotification);
    syncing = false;
}

void EqEditorPanel::notifyChanged()
{
    if (onChange != nullptr)
        onChange();
}

void EqEditorPanel::handleBandChanged (EqBandKind band)
{
    if (syncing || channelIndex < 0)
        return;

    const BandControls* controls = nullptr;

    switch (band)
    {
        case EqBandKind::lowShelf:  controls = &lowBand;  break;
        case EqBandKind::peak:      controls = &midBand;  break;
        case EqBandKind::highShelf: controls = &highBand; break;
    }

    if (controls == nullptr)
        return;

    patternStore.setChannelEqBand (channelIndex,
                                   band,
                                   static_cast<float> (controls->freqSlider.getValue()),
                                   static_cast<float> (controls->gainSlider.getValue()),
                                   static_cast<float> (controls->qSlider.getValue()));
    notifyChanged();
}

void EqEditorPanel::layoutBand (BandControls& band, juce::Rectangle<int> bounds)
{
    band.title.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (2);
    auto row = bounds.removeFromTop (20);
    band.freqLabel.setBounds (row.removeFromLeft (18));
    band.freqSlider.setBounds (row);
    bounds.removeFromTop (2);
    row = bounds.removeFromTop (20);
    band.gainLabel.setBounds (row.removeFromLeft (18));
    band.gainSlider.setBounds (row);
    bounds.removeFromTop (2);
    row = bounds.removeFromTop (20);
    band.qLabel.setBounds (row.removeFromLeft (18));
    band.qSlider.setBounds (row);
}

void EqEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
    SamprLookAndFeel::paintHorizontalDivider (g, getHeight() - 1, getWidth());
}

void EqEditorPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    auto header = area.removeFromTop (22);
    headerLabel.setBounds (header.removeFromLeft (juce::jmax (120, header.getWidth() / 2)));
    enabledButton.setBounds (header.removeFromRight (72));
    area.removeFromTop (4);

    const auto bandWidth = area.getWidth() / 3;
    layoutBand (lowBand, area.removeFromLeft (bandWidth).reduced (2, 0));
    layoutBand (midBand, area.removeFromLeft (bandWidth).reduced (2, 0));
    layoutBand (highBand, area.reduced (2, 0));
}

} // namespace sampr