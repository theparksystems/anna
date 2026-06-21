#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"
#include "CompressorEditorPanel.h"
#include "DelayEditorPanel.h"
#include "EqEditorPanel.h"
#include "ReverbEditorPanel.h"

namespace sampr
{

class FxWorkspaceComponent final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit FxWorkspaceComponent (PatternStore& store);

    void setChangeCallback (ChangeCallback callback);
    void setChannel (int rowIndex);
    void refreshFromStore();
    int getChannelIndex() const noexcept { return channelIndex; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildChannelList();
    void syncEditors();
    void updateTitle();

    PatternStore& patternStore;
    ChangeCallback onChange;
    int channelIndex = 0;

    juce::Label titleLabel;
    juce::Label channelLabel { {}, "Channel" };
    juce::ComboBox channelBox;
    juce::TabbedComponent fxTabs { juce::TabbedButtonBar::TabsAtTop };

    EqEditorPanel eqPanel;
    CompressorEditorPanel compressorPanel;
    DelayEditorPanel delayPanel;
    ReverbEditorPanel reverbPanel;
};

} // namespace sampr