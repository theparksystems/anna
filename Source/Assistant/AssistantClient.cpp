#include "AssistantClient.h"

#include <juce_events/juce_events.h>

namespace sampr
{

namespace
{
    juce::String readStreamToString (juce::InputStream& stream)
    {
        juce::String result;
        const int chunkSize = 4096;

        for (;;)
        {
            juce::MemoryBlock block (static_cast<size_t> (chunkSize));
            const auto bytesRead = stream.read (block.getData(), chunkSize);

            if (bytesRead <= 0)
                break;

            result += juce::String::fromUTF8 (static_cast<const char*> (block.getData()),
                                             static_cast<int> (bytesRead));
        }

        return result;
    }

    juce::var parseJson (const juce::String& text)
    {
        return juce::JSON::parse (text);
    }
}

AssistantClient::AssistantClient()
    : juce::Thread ("ANNA Assistant Client")
{
    startThread (juce::Thread::Priority::normal);
}

AssistantClient::~AssistantClient()
{
    cancelPending();
    signalThreadShouldExit();
    stopThread (8000);
}

void AssistantClient::setBaseUrl (const juce::String& url)
{
    const juce::ScopedLock sl (lock);
    baseUrl = url.trimCharactersAtEnd ("/");
}

void AssistantClient::checkHealth (HealthCallback callback)
{
    {
        const juce::ScopedLock sl (lock);
        pendingJob = JobType::health;
        healthCallback = std::move (callback);
        chatCallback = nullptr;
    }

    notify();
}

void AssistantClient::sendChat (const juce::String& message,
                                const juce::var& trackContext,
                                const juce::Array<juce::var>& history,
                                ChatCallback callback)
{
    {
        const juce::ScopedLock sl (lock);
        pendingJob = JobType::chat;
        chatMessage = message;
        chatContext = trackContext;
        chatHistory = history;
        chatCallback = std::move (callback);
        healthCallback = nullptr;
    }

    busy = true;
    notify();
}

void AssistantClient::cancelPending()
{
    const juce::ScopedLock sl (lock);
    pendingJob = JobType::none;
    healthCallback = nullptr;
    chatCallback = nullptr;
    busy = false;
}

void AssistantClient::run()
{
    while (! threadShouldExit())
    {
        wait (500);

        JobType job = JobType::none;
        {
            const juce::ScopedLock sl (lock);
            job = pendingJob;
            pendingJob = JobType::none;
        }

        if (job == JobType::none)
            continue;

        if (job == JobType::health)
        {
            const auto ok = performHealthCheck();
            HealthCallback callback;

            {
                const juce::ScopedLock sl (lock);
                callback = std::move (healthCallback);
                healthCallback = nullptr;
            }

            if (callback != nullptr)
            {
                juce::MessageManager::callAsync ([callback, ok, detail = lastError]
                {
                    callback (ok, detail);
                });
            }
        }
        else if (job == JobType::chat)
        {
            const auto ok = performChatRequest();
            ChatCallback callback;
            juce::String response;
            juce::String error = lastError;

            {
                const juce::ScopedLock sl (lock);
                callback = std::move (chatCallback);
                chatCallback = nullptr;

                if (ok)
                    response = chatResponse;
            }

            busy = false;

            if (callback != nullptr)
            {
                juce::MessageManager::callAsync ([callback, ok, response, error]
                {
                    callback (ok, response, error);
                });
            }
        }
    }
}

bool AssistantClient::performHealthCheck()
{
    juce::String url;
    {
        const juce::ScopedLock sl (lock);
        url = baseUrl + "/health";
    }

    const juce::URL request (url);
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (3000);

    if (auto stream = request.createInputStream (options))
    {
        const auto body = readStreamToString (*stream);
        const auto json = parseJson (body);

        if (json.isObject())
        {
            lastHealthOk = json.getProperty ("status", juce::String()).toString() == "ok";
            lastModelName = json.getProperty ("model", juce::String()).toString();
            lastError = lastHealthOk ? juce::String() : "Assistant unhealthy";
            return lastHealthOk;
        }
    }

    lastHealthOk = false;
    lastError = "Assistant offline — start via launch.ps1";
    return false;
}

bool AssistantClient::performChatRequest()
{
    juce::String url;
    juce::String message;
    juce::var context;
    juce::Array<juce::var> history;

    {
        const juce::ScopedLock sl (lock);
        url = baseUrl + "/api/chat";
        message = chatMessage;
        context = chatContext;
        history = chatHistory;
    }

    auto* payload = new juce::DynamicObject();
    payload->setProperty ("message", message);
    payload->setProperty ("trackContext", context);
    payload->setProperty ("history", history);

    const auto jsonBody = juce::JSON::toString (juce::var (payload), false);
    const juce::URL request (url);

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                             .withExtraHeaders ("Content-Type: application/json\r\n")
                             .withConnectionTimeoutMs (55000);

    if (auto stream = request.withPOSTData (jsonBody).createInputStream (options))
    {
        const auto body = readStreamToString (*stream);
        const auto json = parseJson (body);

        if (json.isObject())
        {
            if (json.hasProperty ("error"))
            {
                lastError = json.getProperty ("error", juce::String()).toString();
                return false;
            }

            chatResponse = json.getProperty ("response", juce::String()).toString();
            lastError.clear();
            return chatResponse.isNotEmpty();
        }

        lastError = "Invalid assistant response";
        return false;
    }

    lastError = "ANNA did not receive a response from the local assistant service.";
    return false;
}

} // namespace sampr
