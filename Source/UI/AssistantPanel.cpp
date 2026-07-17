#include "AssistantPanel.h"

#include "../Audio/AudioEngine.h"
#include "../Model/PatternStore.h"
#include "../Model/ProjectModel.h"
#include "../Model/SampleManager.h"
#include "SamprLookAndFeel.h"

namespace sampr
{

namespace
{
    int scopeToComboId (ContextScope scope)
    {
        switch (scope)
        {
            case ContextScope::channel: return 1;
            case ContextScope::pattern: return 2;
            case ContextScope::project: return 3;
            case ContextScope::slice:   return 4;
            default:                    return 1;
        }
    }

    ContextScope comboIdToScope (int id)
    {
        switch (id)
        {
            case 2:  return ContextScope::pattern;
            case 3:  return ContextScope::project;
            case 4:  return ContextScope::slice;
            default: return ContextScope::channel;
        }
    }
}

AssistantPanel::AssistantPanel (AssistantClient& client,
                                PatternStore& patterns,
                                ProjectModel& project,
                                SampleManager& samples,
                                AudioEngine& engine)
    : assistantClient (client),
      patternStore (patterns),
      projectModel (project),
      sampleManager (samples),
      audioEngine (engine)
{
    titleLabel.setText ("ANNA Assistant", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions { 14.0f, juce::Font::bold });
    titleLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textPrimary());

    scopeLabel.setFont (juce::FontOptions { 11.0f });
    scopeLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textMuted());
    channelLabel.setFont (juce::FontOptions { 11.0f });
    channelLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textMuted());

    scopeBox.addItem ("Channel", 1);
    scopeBox.addItem ("Pattern", 2);
    scopeBox.addItem ("Project", 3);
    scopeBox.addItem ("Slice", 4);
    scopeBox.setSelectedId (1, juce::dontSendNotification);
    scopeBox.onChange = [this]
    {
        currentScope = comboIdToScope (scopeBox.getSelectedId());
        updateScopeControls();
    };

    channelBox.onChange = [this]
    {
        channelIndex = channelBox.getSelectedItemIndex();
    };

    chatLog.setMultiLine (true);
    chatLog.setReadOnly (true);
    chatLog.setScrollbarsShown (true);
    chatLog.setCaretVisible (false);
    chatLog.setFont (juce::FontOptions { 12.5f });
    chatLog.setColour (juce::TextEditor::backgroundColourId, SamprLookAndFeel::surface());
    chatLog.setColour (juce::TextEditor::textColourId, SamprLookAndFeel::textPrimary());
    chatLog.setColour (juce::TextEditor::outlineColourId, SamprLookAndFeel::border());

    inputEditor.setMultiLine (false);
    inputEditor.setReturnKeyStartsNewLine (false);
    inputEditor.setFont (juce::FontOptions { 12.5f });
    inputEditor.setColour (juce::TextEditor::backgroundColourId, SamprLookAndFeel::surface());
    inputEditor.setColour (juce::TextEditor::textColourId, SamprLookAndFeel::textPrimary());
    inputEditor.setColour (juce::TextEditor::outlineColourId, SamprLookAndFeel::border());
    inputEditor.setTextToShowWhenEmpty ("Why does this track sound flat?", SamprLookAndFeel::textMuted());
    inputEditor.onReturnKey = [this] { sendCurrentMessage(); return true; };

    sendButton.onClick = [this] { sendCurrentMessage(); };
    sendButton.setTooltip ("Send question to ANNA (Enter)");

    statusLabel.setFont (juce::FontOptions { 11.0f });
    statusLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textMuted());

    emptyHintLabel.setText ("Ask why a track sounds flat - ANNA reads your mixer, FX, and step data.",
                            juce::dontSendNotification);
    emptyHintLabel.setFont (juce::FontOptions { 12.0f });
    emptyHintLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textMuted());
    emptyHintLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (scopeLabel);
    addAndMakeVisible (scopeBox);
    addAndMakeVisible (channelLabel);
    addAndMakeVisible (channelBox);
    addAndMakeVisible (chatLog);
    addAndMakeVisible (inputEditor);
    addAndMakeVisible (sendButton);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (emptyHintLabel);

    refreshChannelList();
    updateScopeControls();
    checkHealth();
}

void AssistantPanel::setChannelIndex (int rowIndex)
{
    channelIndex = juce::jmax (0, rowIndex);
    refreshChannelList();
}

void AssistantPanel::setScope (ContextScope scope)
{
    currentScope = scope;
    scopeBox.setSelectedId (scopeToComboId (scope), juce::dontSendNotification);
    updateScopeControls();
}

void AssistantPanel::openWithPrompt (ContextScope scope, int rowIndex, const juce::String& seedMessage)
{
    currentScope = scope;
    channelIndex = juce::jmax (0, rowIndex);

    scopeBox.setSelectedId (scopeToComboId (scope), juce::dontSendNotification);

    refreshChannelList();
    updateScopeControls();

    if (seedMessage.isNotEmpty())
        inputEditor.setText (seedMessage, false);

    inputEditor.grabKeyboardFocus();
}

void AssistantPanel::refreshChannelList()
{
    const auto& pattern = patternStore.getCurrentPattern();
    const auto previous = channelIndex;
    channelBox.clear();

    if (pattern.rows.empty())
    {
        channelBox.addItem ("No channels", 1);
        channelBox.setSelectedId (1, juce::dontSendNotification);
        channelIndex = 0;
        return;
    }

    for (int i = 0; i < static_cast<int> (pattern.rows.size()); ++i)
    {
        const auto& row = pattern.rows[static_cast<size_t> (i)];
        const auto label = row.label.isNotEmpty() ? row.label : ("Ch " + juce::String (i + 1));
        channelBox.addItem (label, i + 1);
    }

    channelIndex = juce::jlimit (0, static_cast<int> (pattern.rows.size()) - 1, previous);
    channelBox.setSelectedItemIndex (channelIndex, juce::dontSendNotification);
}

void AssistantPanel::checkHealth()
{
    statusLabel.setText ("Checking assistant...", juce::dontSendNotification);

    assistantClient.checkHealth ([this] (bool online, const juce::String& detail)
    {
        juce::ignoreUnused (online, detail);
        assistantModelName = assistantClient.getModelName();
        updateOnlineStatus();
    });
}

void AssistantPanel::updateOnlineStatus()
{
    if (waitingForResponse)
        return;

    if (assistantClient.isOnline())
    {
        const auto model = assistantModelName.isNotEmpty() ? assistantModelName : "local model";
        statusLabel.setText ("ANNA online (" + model + " via Ollama)", juce::dontSendNotification);
    }
    else
    {
        const auto err = assistantClient.getLastError();
        statusLabel.setText (err.isNotEmpty() ? err : "Assistant offline - run launch.ps1",
                             juce::dontSendNotification);
    }
}

void AssistantPanel::updateScopeControls()
{
    const auto needsChannel = currentScope == ContextScope::channel
                              || currentScope == ContextScope::pattern;
    channelLabel.setVisible (needsChannel);
    channelBox.setVisible (needsChannel);
    resized();
}

TrackContextInput AssistantPanel::makeContextInput() const
{
    TrackContextInput input;
    input.scope = currentScope;
    input.project = &projectModel;
    input.pattern = &patternStore.getCurrentPattern();
    input.patternIndex = patternStore.getCurrentPatternIndex();
    input.channelIndex = channelIndex;
    input.meters = &audioEngine.getLevelMeters();
    input.bpm = audioEngine.getBpm() > 0.0 ? audioEngine.getBpm() : projectModel.getSettings().bpm;
    input.transportPlaying = audioEngine.isTransportPlaying();
    input.sampleAsset = sampleManager.getAsset (sampleManager.getSelectedAssetId());
    input.sliceIndex = input.sampleAsset != nullptr ? input.sampleAsset->selectedSliceIndex : 0;
    return input;
}

void AssistantPanel::appendMessage (bool fromUser, const juce::String& text)
{
    messages.push_back ({ fromUser, text });
    rebuildChatDisplay();
}

void AssistantPanel::rebuildChatDisplay()
{
    juce::String display;

    for (const auto& msg : messages)
    {
        display << (msg.fromUser ? "You: " : "ANNA: ");
        display << msg.text.trim();
        display << "\n\n";
    }

    chatLog.setText (display, false);
    chatLog.moveCaretToEnd();
    emptyHintLabel.setVisible (messages.empty() && ! waitingForResponse);
}

void AssistantPanel::setWaiting (bool waiting)
{
    waitingForResponse = waiting;
    sendButton.setEnabled (! waiting);
    inputEditor.setEnabled (! waiting);

    if (waiting)
    {
        responseWaitTicks = 0;
        startTimerHz (2);
        statusLabel.setText ("ANNA is thinking...", juce::dontSendNotification);
    }
    else
    {
        stopTimer();
        updateOnlineStatus();
    }

    emptyHintLabel.setVisible (messages.empty() && ! waiting);
}

void AssistantPanel::timerCallback()
{
    if (! waitingForResponse)
    {
        stopTimer();
        return;
    }

    ++responseWaitTicks;

    if (responseWaitTicks == 20)
        statusLabel.setText ("ANNA is still working...", juce::dontSendNotification);

    if (responseWaitTicks < 90)
        return;

    assistantClient.cancelPending();
    setWaiting (false);
    ++activeRequestId;
    appendMessage (false, "ANNA response timed out. The local model did not answer fast enough, so the input was unlocked.");
    statusLabel.setText ("ANNA timed out - try a shorter question or smaller model.", juce::dontSendNotification);
}

void AssistantPanel::sendCurrentMessage()
{
    const auto text = inputEditor.getText().trim();

    if (text.isEmpty() || waitingForResponse || assistantClient.isBusy())
        return;

    if (! assistantClient.isOnline())
    {
        checkHealth();
        appendMessage (false, "ANNA is offline. Run launch.ps1 to start the assistant sidecar.");
        return;
    }

    appendMessage (true, text);
    inputEditor.clear();
    setWaiting (true);
    const auto requestId = ++activeRequestId;

    const auto context = TrackContextBuilder::build (makeContextInput());

    juce::Array<juce::var> history;

    for (const auto& msg : messages)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("role", msg.fromUser ? "user" : "assistant");
        entry->setProperty ("content", msg.text);
        history.add (juce::var (entry));
    }

    assistantClient.sendChat (text, context, history,
                              [this, requestId] (bool success, const juce::String& response, const juce::String& error)
    {
        if (requestId != activeRequestId)
            return;

        setWaiting (false);

        if (success)
        {
            appendMessage (false, response);
        }
        else
        {
            appendMessage (false, error.isNotEmpty() ? error : "Request failed.");
            statusLabel.setText (error, juce::dontSendNotification);
        }
    });
}

void AssistantPanel::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
    SamprLookAndFeel::paintPanelOutline (g, getLocalBounds());
}

void AssistantPanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);

    auto controls = area.removeFromTop (26);
    scopeLabel.setBounds (controls.removeFromLeft (44));
    scopeBox.setBounds (controls.removeFromLeft (110));
    controls.removeFromLeft (10);
    channelLabel.setBounds (controls.removeFromLeft (54));
    channelBox.setBounds (controls.removeFromLeft (160));
    area.removeFromTop (6);

    statusLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (6);

    auto inputRow = area.removeFromBottom (30);
    sendButton.setBounds (inputRow.removeFromRight (72));
    inputRow.removeFromRight (6);
    inputEditor.setBounds (inputRow);
    area.removeFromBottom (6);

    chatLog.setBounds (area);
    emptyHintLabel.setBounds (chatLog.getBounds().reduced (12));
}

} // namespace sampr
