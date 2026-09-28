// Investigate's own Standalone application entry point.
//
// The only difference from JUCE's default Standalone app
// (juce_audio_plugin_client_Standalone.cpp) is that "Mute audio input"
// starts OFF instead of ON. The roadmap (risk #7) and T-002 section 9 both
// call for this: Investigate passes audio through bit-exact with zero added
// latency, so there is no feedback risk from analysing live input while
// also outputting it, and the analyser shows nothing useful with muted
// input.
//
// JUCE's own StandaloneFilterApp class is defined entirely inside its own
// .cpp file and is not reachable from a header, so there is no way to
// subclass or reconfigure just the one setting; the escape hatch JUCE
// provides is to compile this whole file's contents in its place. That is
// controlled by the JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP compile
// definition, which build-engineer set on the Investigate target in T-025.
//
// JUCE's stock juce_audio_plugin_client_Standalone.cpp only guards its own
// StandaloneFilterApp class and JUCE_CREATE_APPLICATION_DEFINE behind
// !JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP; it always keeps its own
// JUCE_MAIN_FUNCTION_DEFINITION (WinMain), unless
// JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT is *also* set (it is not -
// T-025's log). So this file must provide only juce_CreateApplication() via
// JUCE_CREATE_APPLICATION_DEFINE, and must NOT also define
// JUCE_MAIN_FUNCTION_DEFINITION: the stock file's WinMain is the one and
// only entry point, and it calls juce_CreateApplication(), which resolves
// to the definition below (T-025 round 2, T-009 step 1).
#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace investigate
{

class InvestigateStandaloneApp final : public juce::JUCEApplication
{
public:
    InvestigateStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override           { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
           #if (JUCE_ANDROID || JUCE_IOS) && ! JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
            true;
           #else
            false;
           #endif

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        auto holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(),
                                                                        false,
                                                                        juce::String{},
                                                                        nullptr,
                                                                        channelConfig,
                                                                        autoOpenMidiDevices);

        // The one deliberate change from JUCE's default (T-002 section 9):
        // start unmuted. This overrides whatever "shouldMuteInput" value a
        // previous run may have persisted to the settings file, so the input
        // is unmuted on every launch, matching an analyser's expected
        // out-of-the-box behaviour.
        holder->getMuteInputValue().setValue (false);
        return holder;
    }

    juce::StandaloneFilterWindow* createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new juce::StandaloneFilterWindow (getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (createWindow());

        if (mainWindow != nullptr)
            mainWindow->setVisible (true);
        else
            pluginHolder = createPluginHolder();
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
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
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
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace investigate

JUCE_CREATE_APPLICATION_DEFINE (investigate::InvestigateStandaloneApp)

#endif
