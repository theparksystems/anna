#pragma once

#include <functional>

#include <juce_core/juce_core.h>

namespace sampr
{

class AssistantClient final : private juce::Thread
{
public:
    using HealthCallback = std::function<void (bool online, const juce::String& detail)>;
    using ChatCallback = std::function<void (bool success,
                                             const juce::String& response,
                                             const juce::String& error)>;

    AssistantClient();
    ~AssistantClient() override;

    void setBaseUrl (const juce::String& url);
    juce::String getBaseUrl() const { return baseUrl; }

    bool isOnline() const noexcept { return lastHealthOk; }
    bool isBusy() const noexcept { return busy; }
    juce::String getLastError() const { return lastError; }
    juce::String getModelName() const { return lastModelName; }

    void checkHealth (HealthCallback callback);
    void sendChat (const juce::String& message,
                   const juce::var& trackContext,
                   const juce::Array<juce::var>& history,
                   ChatCallback callback);
    void cancelPending();

private:
    void run() override;
    bool performHealthCheck();
    bool performChatRequest();

    juce::String baseUrl { "http://127.0.0.1:3921" };
    juce::CriticalSection lock;
    enum class JobType { none, health, chat };
    JobType pendingJob = JobType::none;
    juce::String chatMessage;
    juce::String chatResponse;
    juce::var chatContext;
    juce::Array<juce::var> chatHistory;
    HealthCallback healthCallback;
    ChatCallback chatCallback;
    std::atomic<bool> busy { false };
    std::atomic<bool> lastHealthOk { false };
    juce::String lastError;
    juce::String lastModelName;
};

} // namespace sampr