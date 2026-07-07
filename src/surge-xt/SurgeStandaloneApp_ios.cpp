/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#ifdef Component
#undef Component
#endif

#ifdef Point
#undef Point
#endif

namespace Surge
{
namespace Standalone
{
namespace iOS
{
void disableFeedbackLoopMute(juce::StandalonePluginHolder &holder)
{
    holder.processorHasPotentialFeedbackLoop = false;
    holder.muteInput = false;
    holder.getMuteInputValue().setValue(false);
}

class PluginHolder final : public juce::StandalonePluginHolder
{
  public:
    using juce::StandalonePluginHolder::StandalonePluginHolder;

    void createPlugin() override
    {
        juce::StandalonePluginHolder::createPlugin();
        disableFeedbackLoopMute(*this);
    }
};

class AudioSettingsComponent final : public juce::Component
{
  public:
    AudioSettingsComponent(juce::StandalonePluginHolder &pluginHolder,
                           juce::AudioDeviceManager &deviceManagerToUse,
                           int maxAudioInputChannels, int maxAudioOutputChannels)
        : owner(pluginHolder),
          deviceSelector(deviceManagerToUse, 0, maxAudioInputChannels, 0, maxAudioOutputChannels,
                         true,
                         (pluginHolder.processor.get() != nullptr &&
                          pluginHolder.processor->producesMidi()),
                         true, false),
          shouldMuteLabel("Feedback Loop:", "Feedback Loop:"),
          shouldMuteButton("Mute audio input"), closeButton("Close Settings")
    {
        setOpaque(true);

        shouldMuteButton.setClickingTogglesState(true);
        shouldMuteButton.getToggleStateValue().referTo(owner.shouldMuteInput);

        addAndMakeVisible(deviceSelector);
        addAndMakeVisible(closeButton);

        closeButton.onClick = [this]() {
            if (auto *w = findParentComponentOfClass<juce::DialogWindow>())
                w->exitModalState(0);
        };

        if (owner.getProcessorHasPotentialFeedbackLoop())
        {
            addAndMakeVisible(shouldMuteButton);
            addAndMakeVisible(shouldMuteLabel);
            shouldMuteLabel.attachToComponent(&shouldMuteButton, true);
        }
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        const juce::ScopedValueSetter<bool> scope(isResizing, true);

        auto r = getLocalBounds();
        auto bottomArea = r.removeFromBottom(closeAreaHeight);
        closeButton.setBounds(bottomArea.reduced(20, 10));

        if (owner.getProcessorHasPotentialFeedbackLoop())
        {
            auto itemHeight = deviceSelector.getItemHeight();
            auto extra = r.removeFromTop(itemHeight);
            auto separatorHeight = (itemHeight >> 1);

            shouldMuteButton.setBounds(juce::Rectangle<int>(
                extra.proportionOfWidth(0.35f), separatorHeight, extra.proportionOfWidth(0.60f),
                deviceSelector.getItemHeight()));

            r.removeFromTop(separatorHeight);
        }

        deviceSelector.setBounds(r);
    }

    void childBoundsChanged(juce::Component *childComp) override
    {
        if (!isResizing && childComp == &deviceSelector)
            setToRecommendedSize();
    }

    void setToRecommendedSize()
    {
        const auto extraHeight = [this]() {
            if (!owner.getProcessorHasPotentialFeedbackLoop())
                return 0;

            const auto itemHeight = deviceSelector.getItemHeight();
            const auto separatorHeight = (itemHeight >> 1);
            return itemHeight + separatorHeight;
        }();

        setSize(getWidth(), deviceSelector.getHeight() + extraHeight + closeAreaHeight);
    }

  private:
    static constexpr int closeAreaHeight = 50;

    juce::StandalonePluginHolder &owner;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::Label shouldMuteLabel;
    juce::ToggleButton shouldMuteButton;
    juce::TextButton closeButton;
    bool isResizing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsComponent)
};

std::unique_ptr<juce::StandalonePluginHolder>
prepareStandalonePluginHolder(std::unique_ptr<juce::StandalonePluginHolder> holder)
{
    if (holder != nullptr)
        disableFeedbackLoopMute(*holder);

    return holder;
}

class StandaloneWindow final : public juce::StandaloneFilterWindow
{
  public:
    StandaloneWindow(const juce::String &title, juce::Colour backgroundColour,
                     std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::StandaloneFilterWindow(title, backgroundColour,
                                       prepareStandalonePluginHolder(std::move(pluginHolderIn))),
          optionsButton("Options")
    {
        addAndMakeVisible(optionsButton);
        optionsButton.onClick = [this]() { showAudioSettingsDialog(); };
        optionsButton.setTriggeredOnMouseDown(true);
        optionsButton.setAlwaysOnTop(true);
    }

    void resized() override
    {
        juce::StandaloneFilterWindow::resized();
        optionsButton.setBounds(10, 10, 80, 30);
        optionsButton.toFront(false);
    }

  private:
    void showAudioSettingsDialog()
    {
        juce::DialogWindow::LaunchOptions options;

        int maxNumInputs = 0;
        int maxNumOutputs = 0;

        if (pluginHolder->channelConfiguration.size() > 0)
        {
            auto &defaultConfig = pluginHolder->channelConfiguration.getReference(0);
            maxNumInputs = juce::jmax(0, static_cast<int>(defaultConfig.numIns));
            maxNumOutputs = juce::jmax(0, static_cast<int>(defaultConfig.numOuts));
        }

        if (auto *bus = pluginHolder->processor->getBus(true, 0))
            maxNumInputs = juce::jmax(0, bus->getDefaultLayout().size());

        if (auto *bus = pluginHolder->processor->getBus(false, 0))
            maxNumOutputs = juce::jmax(0, bus->getDefaultLayout().size());

        auto content = std::make_unique<AudioSettingsComponent>(
            *pluginHolder, pluginHolder->deviceManager, maxNumInputs, maxNumOutputs);
        content->setSize(500, 550);
        content->setToRecommendedSize();

        auto *contentPtr = content.get();
        options.content.setOwned(content.release());
        options.dialogTitle = juce::translate("Audio/MIDI Settings");
        options.dialogBackgroundColour =
            contentPtr->getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    }

    juce::TextButton optionsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneWindow)
};

class StandaloneApp final : public juce::JUCEApplication
{
  public:
    StandaloneApp()
    {
        juce::PropertiesFile::Options options;

        options.applicationName = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = "";

        appProperties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override
    {
        return juce::CharPointer_UTF8(JucePlugin_Name);
    }

    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String &) override {}

    StandaloneWindow *createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new StandaloneWindow(
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
#if (JUCE_ANDROID || JUCE_IOS) && !JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
            true;
#else
            false;
#endif

#ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[]{
            JucePlugin_PreferredChannelConfigurations};
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig(
            channels, juce::numElementsInArray(channels));
#else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
#endif

        return std::make_unique<PluginHolder>(
            appProperties.getUserSettings(), false, juce::String{}, nullptr, channelConfig,
            autoOpenMidiDevices);
    }

    void initialise(const juce::String &) override
    {
        mainWindow.reset(createWindow());

        if (mainWindow != nullptr)
        {
#if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            juce::Desktop::getInstance().setKioskModeComponent(mainWindow.get(), false);
#endif

            mainWindow->setVisible(true);
        }
        else
        {
            pluginHolder = prepareStandalonePluginHolder(createPluginHolder());
        }
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(100, []() {
                if (auto *app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

  private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<StandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
} // namespace iOS
} // namespace Standalone
} // namespace Surge

START_JUCE_APPLICATION(Surge::Standalone::iOS::StandaloneApp)

#if JucePlugin_Build_Standalone && JUCE_IOS

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wmissing-prototypes")

bool JUCE_CALLTYPE juce_isInterAppAudioConnected()
{
    if (auto *holder = juce::StandalonePluginHolder::getInstance())
        return holder->isInterAppAudioConnected();

    return false;
}

void JUCE_CALLTYPE juce_switchToHostApplication()
{
    if (auto *holder = juce::StandalonePluginHolder::getInstance())
        holder->switchToHostApplication();
}

juce::Image JUCE_CALLTYPE juce_getIAAHostIcon(int size)
{
    if (auto *holder = juce::StandalonePluginHolder::getInstance())
        return holder->getIAAHostIcon(size);

    return {};
}

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

#endif
