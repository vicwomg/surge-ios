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

#include "SurgeSynthEditor.h"
#include "gui/SurgeGUIEditor.h"

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
          optionsButton("Options"),
          zoomInButton("zoom+"),
          zoomOutButton("zoom-"),
          zoomFitButton("fit")
    {
        addAndMakeVisible(optionsButton);
        optionsButton.onClick = [this]() { showAudioSettingsDialog(); };
        optionsButton.setTriggeredOnMouseDown(true);
        optionsButton.setAlwaysOnTop(true);

        addAndMakeVisible(zoomOutButton);
        zoomOutButton.onClick = [this]() { changeZoom(-10.0f); };
        zoomOutButton.setTriggeredOnMouseDown(true);
        zoomOutButton.setAlwaysOnTop(true);

        addAndMakeVisible(zoomInButton);
        zoomInButton.onClick = [this]() { changeZoom(10.0f); };
        zoomInButton.setTriggeredOnMouseDown(true);
        zoomInButton.setAlwaysOnTop(true);

        addAndMakeVisible(zoomFitButton);
        zoomFitButton.onClick = [this]() { fitZoom(); };
        zoomFitButton.setTriggeredOnMouseDown(true);
        zoomFitButton.setAlwaysOnTop(true);

        setupiPhoneScrollIfNeeded();
    }

    void changeZoom(float delta)
    {
        if (pluginHolder && pluginHolder->processor)
        {
            if (auto* ed = dynamic_cast<SurgeSynthEditor*>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto* sge = ed->getSurgeGUIEditor())
                {
                    float currentZoom = sge->getZoomFactor();
                    sge->resizeWindow(currentZoom + delta);
                }
            }
        }
    }

    // Calculates the zoom % that makes the synth's UI exactly fill the screen width,
    // using sge->getWindowSizeX() (skin-aware) rather than the hardcoded 913px constant.
    // Falls back to 100% if the editor is not yet available.
    float calcFitZoom() const
    {
        auto &displays = juce::Desktop::getInstance().getDisplays();
        auto *primary = displays.getPrimaryDisplay();
        if (primary == nullptr)
            return 100.f;

        auto userArea = primary->userArea;
        int screenW = juce::jmax(userArea.getWidth(), userArea.getHeight());

        if (pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                {
                    float skinW = static_cast<float>(sge->getWindowSizeX());
                    if (skinW > 0)
                        return std::round(screenW / skinW * 100.f);
                }
            }
        }

        // Editor not ready yet — fall back to the default skin width.
        constexpr int surgeNativeW = 913;
        return std::round(static_cast<float>(screenW) / surgeNativeW * 100.f);
    }

    void fitZoom()
    {
        if (pluginHolder && pluginHolder->processor)
        {
            if (auto *ed = dynamic_cast<SurgeSynthEditor *>(pluginHolder->processor->getActiveEditor()))
            {
                if (auto *sge = ed->getSurgeGUIEditor())
                    sge->resizeWindow(calcFitZoom());
            }
        }
    }

    void resized() override
    {
        if (scrollViewport != nullptr)
        {
            // Don't call parent resized() — it would resize content to fill the window,
            // collapsing it and breaking the scroll. Instead, just fit the viewport.
            scrollViewport->setBounds(getLocalBounds());
        }
        else
        {
            juce::StandaloneFilterWindow::resized();
        }

        // Layout: [Options 60] [zoom- 45] [zoom+ 45] [fit/% 55]
        int x = 20;
        optionsButton.setBounds(x, 12, 60, 22);  x += 60 + 10;
        zoomOutButton.setBounds(x, 12, 45, 22);  x += 45 + 5;
        zoomInButton.setBounds(x, 12, 45, 22);   x += 45 + 5;
        zoomFitButton.setBounds(x, 12, 24, 22);

        optionsButton.toFront(false);
        zoomOutButton.toFront(false);
        zoomInButton.toFront(false);
        zoomFitButton.toFront(false);
    }

  private:
    // Wraps the synth content with empty padding so the user can
    // scroll slightly past every edge, making extremity controls easier to tap.
    // On iPad, we add only vertical padding (top + bottom) to keep the Options
    // and Zoom buttons from overlapping the main UI.
    struct PaddingWrapper : public juce::Component
    {
        PaddingWrapper(juce::Component *contentToOwn, int hPad, int vPad)
            : child(contentToOwn), horizontalPad(hPad), verticalPad(vPad)
        {
            addAndMakeVisible(*child);
            child->setTopLeftPosition(horizontalPad, verticalPad);
            setSize(child->getWidth() + 2 * horizontalPad, child->getHeight() + 2 * verticalPad);
        }

        // If Surge internally changes its editor size (e.g. user changes zoom),
        // keep the child positioned and resize this wrapper to match.
        void childBoundsChanged(juce::Component *) override
        {
            child->setTopLeftPosition(horizontalPad, verticalPad);
            setSize(child->getWidth() + 2 * horizontalPad, child->getHeight() + 2 * verticalPad);
        }

        std::unique_ptr<juce::Component> child;
        int horizontalPad;
        int verticalPad;
    };

    std::unique_ptr<PaddingWrapper> paddingWrapper;

    struct SmartDragToScrollListener : public juce::MouseListener
    {
        juce::Viewport *viewport;
        std::map<int, juce::Point<int>> lastMousePos;
        std::map<int, bool> isDraggingTouch;

        SmartDragToScrollListener(juce::Viewport *v) : viewport(v) {}

        void mouseDown(const juce::MouseEvent &e) override
        {
            if (e.originalComponent == nullptr) return;

            // In Surge, the empty background is drawn by a single "MainFrame" component.
            // Empty margins outside the synth are handled by our "PaddingWrapper".
            // If the user clicks on anything else, it's an interactive control (slider, etc.)
            juce::String typeName = typeid(*(e.originalComponent)).name();
            bool isBackground = typeName.containsIgnoreCase("MainFrame") ||
                                typeName.containsIgnoreCase("PaddingWrapper");

            isDraggingTouch[e.source.getIndex()] = isBackground;
            lastMousePos[e.source.getIndex()] = e.getScreenPosition();
        }

        void mouseDrag(const juce::MouseEvent &e) override
        {
            if (isDraggingTouch[e.source.getIndex()])
            {
                auto currentPos = e.getScreenPosition();
                auto delta = currentPos - lastMousePos[e.source.getIndex()];
                lastMousePos[e.source.getIndex()] = currentPos;

                auto pos = viewport->getViewPosition();
                viewport->setViewPosition(pos.x - delta.x, pos.y - delta.y);
            }
        }

        void mouseUp(const juce::MouseEvent &e) override
        {
            isDraggingTouch.erase(e.source.getIndex());
            lastMousePos.erase(e.source.getIndex());
        }
    };

    std::unique_ptr<juce::Viewport> scrollViewport;
    std::unique_ptr<SmartDragToScrollListener> smartDragListener;

    void setupiPhoneScrollIfNeeded()
    {
        // Detect iPhone vs iPad: iPad short edge >= 768pt.
        auto &displays = juce::Desktop::getInstance().getDisplays();
        auto *primary = displays.getPrimaryDisplay();
        if (primary == nullptr)
            return;

        auto userArea = primary->userArea;
        int shortEdge = juce::jmin(userArea.getWidth(), userArea.getHeight());
        bool isIPhone = (shortEdge < 768);
        // Surge's native canvas size (from globals.h BASE_WINDOW_SIZE_X/Y).
        constexpr int surgeNativeW = 913;
        constexpr int surgeNativeH = 569;

        int screenW = juce::jmax(userArea.getWidth(), userArea.getHeight());
        int screenH = shortEdge;

        juce::Component *content = getContentComponent();
        if (content == nullptr)
            return;

        int hPad, vPad;

        if (isIPhone)
        {
            // Render the UI at 125% of native size on iPhone.
            constexpr float iPhoneZoom = 1.25f;
            constexpr int edgePadding = 50; // px of empty space on each side

            int contentW = juce::roundToInt(surgeNativeW * iPhoneZoom);
            int contentH = juce::roundToInt(surgeNativeH * iPhoneZoom);

            // Resize the content to the 125% zoom dimensions.
            // StandaloneFilterWindow's MainContentComponent propagates this size
            // to the SurgeSynthEditor, which applies the matching zoom factor.
            content->setSize(contentW, contentH);

            hPad = edgePadding;
            vPad = edgePadding;
        }
        else
        {
            // iPad: set content to native size (100%) now — a proper fit-width zoom is
            // applied asynchronously below after the viewport is constructed, going through
            // sge->resizeWindow() so that Surge's zoomFactor and the menu both stay in sync.
            content->setSize(surgeNativeW, surgeNativeH);

            // Only add a top + bottom margin so that the Options / Zoom button bar
            // (≈46 px tall) does not sit on top of the synth UI.
            constexpr int iPadVerticalMargin = 40;

            hPad = 0;
            vPad = iPadVerticalMargin;
        }

        // Detach content from the window without deleting it.
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wdeprecated-declarations")
        setContentComponent(nullptr, /*deleteOldOne=*/false, /*resizeToFit=*/false);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE

        // Wrap the content in a padded container.
        paddingWrapper = std::make_unique<PaddingWrapper>(content, hPad, vPad);

        // Build the viewport.
        scrollViewport = std::make_unique<juce::Viewport>();
        scrollViewport->setViewedComponent(paddingWrapper.get(), /*deleteOnDetach=*/false);
        scrollViewport->setScrollBarsShown(/*vertical=*/true, /*horizontal=*/true);
        // Turn off native drag-to-scroll. We will handle dragging manually with our listener
        // so we can seamlessly ignore drags that start on sliders.
        scrollViewport->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        scrollViewport->setSize(screenW, screenH);
        // Scroll the viewport so the top-left of the content (minus padding) is visible.
        scrollViewport->setViewPosition(hPad, vPad);

        // Attach our custom smart drag-to-scroll listener
        smartDragListener = std::make_unique<SmartDragToScrollListener>(scrollViewport.get());
        paddingWrapper->addMouseListener(smartDragListener.get(), true);

        setContentNonOwned(scrollViewport.get(), false);
        scrollViewport->setBounds(getLocalBounds());

        if (!isIPhone)
        {
            // Apply the fit-width zoom through the official Surge path (sge->resizeWindow)
            // so that zoomFactor and the menu zoom display are both correct.
            // Deferred so the SurgeSynthEditor's active editor is accessible.
            juce::MessageManager::callAsync([this]() { fitZoom(); });
        }
    }

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
    juce::TextButton zoomInButton;
    juce::TextButton zoomOutButton;
    juce::TextButton zoomFitButton;

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
        // Lock to landscape on all iOS devices (iPhone and iPad).
        juce::Desktop::getInstance().setOrientationsEnabled(juce::Desktop::rotatedClockwise |
                                                            juce::Desktop::rotatedAntiClockwise);

        // Prevent the screen from sleeping entirely while the app is open
        juce::Desktop::getInstance().setScreenSaverEnabled(false);

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
