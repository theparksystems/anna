#include "MixerComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

class MixerComponent::ChannelStrip final : public juce::Component
{
public:
    ChannelStrip (MixerComponent& ownerIn, int rowIndexIn)
        : owner (ownerIn),
          rowIndex (rowIndexIn)
    {
        gainSlider.setSliderStyle (juce::Slider::LinearVertical);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
        gainSlider.setRange (0.0, 2.0, 0.01);
        gainSlider.onValueChange = [this]
        {
            owner.patternStore.setChannelGain (rowIndex, static_cast<float> (gainSlider.getValue()));
            owner.notifyChange();
        };

        panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
        panSlider.setRange (-1.0, 1.0, 0.01);
        panSlider.onValueChange = [this]
        {
            owner.patternStore.setChannelPan (rowIndex, static_cast<float> (panSlider.getValue()));
            owner.notifyChange();
        };

        muteButton.setButtonText ("M");
        soloButton.setButtonText ("S");
        fxButton.setButtonText ("FX");
        askButton.setButtonText ("Ask");
        muteButton.setTooltip ("Mute channel");
        soloButton.setTooltip ("Solo channel");
        fxButton.setTooltip ("Open FX Rack for this channel");
        askButton.setTooltip ("Ask ANNA about this channel");
        muteButton.setClickingTogglesState (true);
        soloButton.setClickingTogglesState (true);
        muteButton.onClick = [this]
        {
            owner.patternStore.setChannelMute (rowIndex, muteButton.getToggleState());
            owner.notifyChange();
        };
        soloButton.onClick = [this]
        {
            owner.patternStore.setChannelSolo (rowIndex, soloButton.getToggleState());
            owner.notifyChange();
        };
        fxButton.onClick = [this]
        {
            owner.openFxForRow (rowIndex);
        };
        askButton.onClick = [this]
        {
            owner.openAskForRow (rowIndex);
        };

        addAndMakeVisible (nameLabel);
        addAndMakeVisible (gainSlider);
        addAndMakeVisible (panSlider);
        addAndMakeVisible (muteButton);
        addAndMakeVisible (soloButton);
        addAndMakeVisible (fxButton);
        addAndMakeVisible (askButton);
        addAndMakeVisible (meter);
    }

    void syncFromRow (const PatternRow& row)
    {
        nameLabel.setText (row.label, juce::dontSendNotification);
        gainSlider.setValue (row.channelGain, juce::dontSendNotification);
        panSlider.setValue (row.channelPan, juce::dontSendNotification);
        muteButton.setToggleState (row.channelMute, juce::dontSendNotification);
        soloButton.setToggleState (row.channelSolo, juce::dontSendNotification);
        stripColour = row.colour;
        repaint();
    }

    void setMeterLevel (float level)
    {
        meterLevel = level;
        meter.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.fillAll (SamprLookAndFeel::panel());
        g.setColour (stripColour.withAlpha (0.85f));
        g.fillRect (bounds.removeFromTop (2));
        SamprLookAndFeel::paintPanelOutline (g, getLocalBounds());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2);
        area.removeFromTop (6);
        nameLabel.setBounds (area.removeFromTop (18));
        area.removeFromTop (4);
        meter.setBounds (area.removeFromTop (72));
        area.removeFromTop (4);
        gainSlider.setBounds (area.removeFromTop (110));
        area.removeFromTop (4);
        panSlider.setBounds (area.removeFromTop (64));
        area.removeFromTop (4);
        auto buttons = area.removeFromTop (48);
        fxButton.setBounds (buttons.removeFromTop (22));
        buttons.removeFromTop (2);
        askButton.setBounds (buttons.removeFromTop (22));
        buttons.removeFromTop (2);
        muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1));
        soloButton.setBounds (buttons.reduced (1));
    }

private:
    class Meter final : public juce::Component
    {
    public:
        explicit Meter (ChannelStrip& ownerIn) : owner (ownerIn) {}

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (SamprLookAndFeel::surface());
            g.fillRect (bounds);
            g.setColour (SamprLookAndFeel::border());
            g.drawRect (bounds, 1.0f);

            const auto fillHeight = bounds.getHeight() * owner.meterLevel;
            juce::Rectangle<float> fill (bounds.getX() + 1.0f, bounds.getBottom() - fillHeight,
                                         bounds.getWidth() - 2.0f, fillHeight - 1.0f);
            g.setColour (owner.stripColour.withAlpha (0.9f));
            g.fillRect (fill);
        }

    private:
        ChannelStrip& owner;
    };

    MixerComponent& owner;
    int rowIndex;
    juce::Label nameLabel;
    juce::Slider gainSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton fxButton;
    juce::TextButton askButton;
    Meter meter { *this };
    juce::Colour stripColour { 0xff4cc2ff };
    float meterLevel = 0.0f;
};

MixerComponent::~MixerComponent() = default;

MixerComponent::MixerComponent (PatternStore& patterns, AudioEngine& engine)
    : patternStore (patterns),
      audioEngine (engine)
{
    stripViewport.setViewedComponent (&stripContainer, false);
    stripViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (stripViewport);

    masterLabel.setText ("Master", juce::dontSendNotification);
    masterGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    masterGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
    masterGainSlider.setRange (0.0, 2.0, 0.01);
    masterGainSlider.setValue (1.0, juce::dontSendNotification);
    masterGainSlider.onValueChange = [this]
    {
        if (onMasterGainChange != nullptr)
            onMasterGainChange();
    };

    addAndMakeVisible (masterLabel);
    addAndMakeVisible (masterGainSlider);
    addAndMakeVisible (masterMeter);

    rebuildStrips();
    startTimerHz (30);
}

void MixerComponent::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void MixerComponent::setMasterGainChangeCallback (ChangeCallback callback)
{
    onMasterGainChange = std::move (callback);
}

void MixerComponent::setFxOpenCallback (FxOpenCallback callback)
{
    onFxOpen = std::move (callback);
}

void MixerComponent::setAskCallback (AskCallback callback)
{
    onAsk = std::move (callback);
}

void MixerComponent::setMasterGain (float gain)
{
    masterGainSlider.setValue (gain, juce::dontSendNotification);
}

float MixerComponent::getMasterGain() const
{
    return static_cast<float> (masterGainSlider.getValue());
}

void MixerComponent::notifyChange()
{
    if (onChange != nullptr)
        onChange();
}

void MixerComponent::openFxForRow (int rowIndex)
{
    if (onFxOpen != nullptr)
        onFxOpen (rowIndex);
}

void MixerComponent::openAskForRow (int rowIndex)
{
    if (onAsk != nullptr)
        onAsk (rowIndex);
}

void MixerComponent::refresh()
{
    rebuildStrips();
}

void MixerComponent::rebuildStrips()
{
    strips.clear();
    stripContainer.removeAllChildren();

    const auto& pattern = patternStore.getCurrentPattern();
    const auto stripWidth = 72;
    auto x = 0;

    for (int i = 0; i < static_cast<int> (pattern.rows.size()); ++i)
    {
        auto strip = std::make_unique<ChannelStrip> (*this, i);
        strip->syncFromRow (pattern.rows[static_cast<size_t> (i)]);
        strip->setBounds (x, 0, stripWidth, 318);
        stripContainer.addAndMakeVisible (strip.get());
        x += stripWidth + 2;
        strips.push_back (std::move (strip));
    }

    stripContainer.setSize (juce::jmax (x, 1), 318);
}

void MixerComponent::timerCallback()
{
    for (int i = 0; i < static_cast<int> (strips.size()); ++i)
        strips[static_cast<size_t> (i)]->setMeterLevel (audioEngine.getLevelMeters().getPeak (i));

    masterMeterLevel = audioEngine.getLevelMeters().getMasterPeak();
    masterMeter.repaint();
    audioEngine.getLevelMeters().decayPeaks (0.85f);
}

void MixerComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (SamprLookAndFeel::background());
    SamprLookAndFeel::paintHorizontalDivider (g, 0, bounds.getWidth());

    g.setColour (SamprLookAndFeel::textPrimary());
    g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    g.drawText ("Mixer", bounds.removeFromTop (18).reduced (4, 0),
                juce::Justification::centredLeft, true);

    if (strips.empty())
    {
        auto stripArea = stripViewport.getBounds();
        g.setColour (SamprLookAndFeel::textMuted());
        g.setFont (juce::FontOptions { 11.0f });
        g.drawText ("No channels yet — add rows in Step Sequencer.",
                    stripArea,
                    juce::Justification::centred);
    }

    auto masterArea = masterMeter.getBounds().toFloat().reduced (0.5f);
    g.setColour (SamprLookAndFeel::surface());
    g.fillRect (masterArea);
    g.setColour (SamprLookAndFeel::border());
    g.drawRect (masterArea, 1.0f);

    juce::Rectangle<float> fill (masterArea.getX() + 1.0f,
                                 masterArea.getBottom() - masterArea.getHeight() * masterMeterLevel,
                                 masterArea.getWidth() - 2.0f,
                                 masterArea.getHeight() * masterMeterLevel - 1.0f);
    g.setColour (SamprLookAndFeel::accent());
    g.fillRect (fill);
}

void MixerComponent::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromTop (18);

    auto masterArea = area.removeFromRight (80);
    masterLabel.setBounds (masterArea.removeFromTop (18));
    masterMeter.setBounds (masterArea.removeFromTop (72));
    masterArea.removeFromTop (4);
    masterGainSlider.setBounds (masterArea.removeFromTop (110));

    stripViewport.setBounds (area);
}

} // namespace sampr
