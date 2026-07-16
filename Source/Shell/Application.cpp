#include <juce_gui_extra/juce_gui_extra.h>

#include "../UI/SamprLookAndFeel.h"
#include "MainComponent.h"

namespace
{
    constexpr int kDefaultWidth  = 1680;
    constexpr int kDefaultHeight = 1000;
    constexpr int kMinWidth      = 1180;
    constexpr int kMinHeight     = 720;

    juce::PropertiesFile& getAppProperties()
    {
        static juce::PropertiesFile::Options options;
        options.applicationName     = "ANNA";
        options.filenameSuffix        = "settings";
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers    = false;
        options.storageFormat       = juce::PropertiesFile::storeAsXML;

        static juce::PropertiesFile props (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                               .getChildFile ("ANNA")
                                               .getChildFile ("anna.settings"),
                                           options);
        return props;
    }

    class BringWindowToFrontCallback final : public juce::ModalComponentManager::Callback
    {
    public:
        explicit BringWindowToFrontCallback (juce::Component* windowIn)
            : window (windowIn)
        {
        }

        void modalStateFinished (int) override
        {
            if (auto* topLevel = window.getComponent())
            {
                topLevel->toFront (true);
                topLevel->grabKeyboardFocus();
            }
        }

    private:
        juce::Component::SafePointer<juce::Component> window;
    };

    void showWelcomeIfNeeded (juce::Component* parentWindow)
    {
        auto& props = getAppProperties();

        if (props.getBoolValue ("welcomeShown", false))
            return;

        props.setValue ("welcomeShown", true);
        props.saveIfNeeded();

        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::InfoIcon,
            "Welcome to ANNA",
            "Native desktop sampler - no browser required.\n\n"
            "Quick start:\n"
            "  Drop audio files anywhere, or Ctrl+Shift+L to load\n"
            "  Auto Slice in the slice editor to chop samples\n"
            "  Space = play/pause  |  1/2/3 = editor tabs\n"
            "  Ctrl+S save  |  Ctrl+E export WAV\n\n"
            "Audio starts automatically when the window opens.",
            "OK",
            parentWindow,
            new BringWindowToFrontCallback (parentWindow));
    }
}

class SamprApplication final : public juce::JUCEApplication
{
public:
    SamprApplication() = default;

    const juce::String getApplicationName() override       { return "ANNA"; }
    const juce::String getApplicationVersion() override    { return "0.9.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String& commandLine) override
    {
        lookAndFeel = std::make_unique<sampr::SamprLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        mainWindow.reset (new MainWindow (getApplicationName()));

        if (! commandLine.containsIgnoreCase ("--no-welcome"))
            showWelcomeIfNeeded (mainWindow.get());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);

        if (mainWindow != nullptr)
            mainWindow->activateWindow();
    }

private:
    class MainWindow final : public juce::DocumentWindow,
                             private juce::Timer
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              sampr::SamprLookAndFeel::background(),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setResizeLimits (kMinWidth, kMinHeight, 4096, 2160);

            setContentOwned (new MainComponent(), true);

            centreWithSize (kDefaultWidth, kDefaultHeight);
            setVisible (true);
            activateWindow();

            startTimer (150);
        }

        void activateWindow()
        {
            setVisible (true);
            toFront (true);
            setAlwaysOnTop (true);
            setAlwaysOnTop (false);

            if (auto* content = getContentComponent())
            {
                content->setVisible (true);
                content->grabKeyboardFocus();
            }
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void visibilityChanged() override
        {
            DocumentWindow::visibilityChanged();

            if (isVisible() && isShowing())
                toFront (true);
        }

        void focusGained (FocusChangeType cause) override
        {
            DocumentWindow::focusGained (cause);
        }

        void timerCallback() override
        {
            if (focusAttempts < 8)
            {
                activateWindow();
                ++focusAttempts;
            }
            else
            {
                stopTimer();
            }
        }

    private:
        int focusAttempts = 0;
    };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<sampr::SamprLookAndFeel> lookAndFeel;
};

START_JUCE_APPLICATION (SamprApplication)
