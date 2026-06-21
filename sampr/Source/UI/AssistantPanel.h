#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Assistant/AssistantClient.h"
#include "../Assistant/TrackContextBuilder.h"

namespace sampr
{

class PatternStore;
class ProjectModel;
class SampleManager;
class AudioEngine;

class AssistantPanel final : public juce::Component
{
public:
    AssistantPanel (AssistantClient& client,
                    PatternStore& patterns,
                    ProjectModel& project,
                    SampleManager& samples,
                    AudioEngine& engine);

    void setChannelIndex (int rowIndex);
    void setScope (ContextScope scope);
    void openWithPrompt (ContextScope scope, int channelIndex, const juce::String& seedMessage);

    void refreshChannelList();
    void checkHealth();
    void updateOnlineStatus();
    void updateScopeControls();
    bool isWaitingForResponse() const noexcept { return waitingForResponse; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct ChatMessage
    {
        bool fromUser = true;
        juce::String text;
    };

    void appendMessage (bool fromUser, const juce::String& text);
    void rebuildChatDisplay();
    void sendCurrentMessage();
    void setWaiting (bool waiting);
    TrackContextInput makeContextInput() const;

    AssistantClient& assistantClient;
    PatternStore& patternStore;
    ProjectModel& projectModel;
    SampleManager& sampleManager;
    AudioEngine& audioEngine;
    ContextScope currentScope = ContextScope::channel;
    int channelIndex = 0;

    juce::Label titleLabel;
    juce::Label scopeLabel { {}, "Scope" };
    juce::ComboBox scopeBox;
    juce::Label channelLabel { {}, "Channel" };
    juce::ComboBox channelBox;
    juce::TextEditor chatLog;
    juce::TextEditor inputEditor;
    juce::TextButton sendButton { "Send" };
    juce::Label statusLabel;
    juce::Label emptyHintLabel;

    std::vector<ChatMessage> messages;
    bool waitingForResponse = false;
    juce::String assistantModelName;
};

} // namespace sampr