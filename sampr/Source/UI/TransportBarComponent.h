#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace sampr
{

class TransportBarComponent final : public juce::Component
{
public:
    using PlayCallback = std::function<void()>;
    using PauseCallback = std::function<void()>;
    using StopCallback = std::function<void()>;
    using RecordCallback = std::function<void(bool)>;
    using ExportCallback = std::function<void()>;
    using SettingsCallback = std::function<void()>;
    using ValueChangeCallback = std::function<void()>;

    TransportBarComponent();

    void setPlayCallback (PlayCallback callback);
    void setPauseCallback (PauseCallback callback);
    void setStopCallback (StopCallback callback);
    void setRecordCallback (RecordCallback callback);
    void setExportCallback (ExportCallback callback);
    void setSettingsCallback (SettingsCallback callback);
    void setValueChangeCallback (ValueChangeCallback callback);

    void setPlaying (bool playing);
    void setPaused (bool paused);
    void setRecording (bool recording);
    void setRecordEnabled (bool enabled);
    bool isRecording() const;
    void setBpm (double bpm);
    void setPositionText (const juce::String& text);

    double getBpm() const;
    bool isMetronomeEnabled() const;
    bool isLoopEnabled() const;
    double getLoopStartBeats() const;
    double getLoopEndBeats() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void updateLoopControlStates();
    juce::TextButton playButton { juce::CharPointer_UTF8 ("\\u25B6") };
    juce::TextButton pauseButton { juce::CharPointer_UTF8 ("\\u23F8") };
    juce::TextButton stopButton { juce::CharPointer_UTF8 ("\\u25A0") };
    juce::ToggleButton recordButton { "Record" };
    juce::TextButton exportButton { "Export WAV" };
    juce::TextButton settingsButton { "Audio" };

    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::Label positionLabel;
    juce::ToggleButton metronomeButton { "Metronome" };
    juce::ToggleButton loopButton { "Loop" };
    juce::Slider loopStartSlider;
    juce::Slider loopEndSlider;
    juce::Label loopStartLabel;
    juce::Label loopEndLabel;

    PlayCallback onPlay;
    PauseCallback onPause;
    StopCallback onStop;
    RecordCallback onRecord;
    ExportCallback onExport;
    SettingsCallback onSettings;
    ValueChangeCallback onValueChange;
};

} // namespace sampr