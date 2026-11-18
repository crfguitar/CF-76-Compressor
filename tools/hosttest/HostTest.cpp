// Loads the built CF-76.vst3 the way a DAW does - instantiate, process audio, open/close the
// editor, delete the instance, unload the module - to catch lifetime/teardown crashes.
//
// usage: CF76HostTest <path to CF-76.vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

namespace
{
    void pump (int ms)
    {
        MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    void log (const String& s)
    {
        std::cout << s << std::endl;
    }

    struct Window : DocumentWindow
    {
        Window() : DocumentWindow ("CF-76 host test", Colours::black, 0) {}
        void closeButtonPressed() override {}
    };
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        log ("usage: CF76HostTest <CF-76.vst3>");
        return 2;
    }

    {
        ScopedJuceInitialiser_GUI juce;
        AudioPluginFormatManager formats;
        formats.addFormat (std::make_unique<VST3PluginFormat>());

        OwnedArray<PluginDescription> types;
        VST3PluginFormat vst3;
        vst3.findAllTypesForFile (types, String (CharPointer_UTF8 (argv[1])));
        if (types.isEmpty())
        {
            log ("FAIL  could not scan plugin");
            return 1;
        }

        // Scenarios mirror what hosts do when a slot is cleared.
        const char* scenarios[] = {
            "no editor: create, process, delete",
            "editor open, close editor, then delete",
            "editor open with audio running, delete editor + instance back-to-back",
            "twice in a row (re-load after delete)",
            "two instances, delete in reverse order",
        };

        for (int sc = 0; sc < (int) std::size (scenarios); ++sc)
        {
            log (String ("....  ") + scenarios[sc]);
            String error;
            std::vector<std::unique_ptr<AudioPluginInstance>> instances;
            const int count = sc == 4 ? 2 : 1;
            for (int i = 0; i < count; ++i)
            {
                auto inst = formats.createPluginInstance (*types[0], 48000.0, 512, error);
                if (inst == nullptr)
                {
                    log ("FAIL  instantiate: " + error);
                    return 1;
                }
                inst->enableAllBuses();
                inst->prepareToPlay (48000.0, 512);
                instances.push_back (std::move (inst));
            }

            AudioBuffer<float> buffer (2, 512);
            MidiBuffer midi;
            Random rng (1);
            auto process = [&] (int blocks)
            {
                for (int b = 0; b < blocks; ++b)
                    for (auto& inst : instances)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            for (int s = 0; s < 512; ++s)
                                buffer.setSample (ch, s, (rng.nextFloat() - 0.5f) * 0.5f);
                        inst->processBlock (buffer, midi);
                    }
            };
            process (40);

            std::vector<std::unique_ptr<Window>> windows;
            std::vector<AudioProcessorEditor*> editors;
            if (sc > 0)
            {
                for (auto& inst : instances)
                {
                    log ("      creating editor");
                    auto* ed = inst->createEditorAndMakeActive();
                    log ("      editor created");
                    auto w = std::make_unique<Window>();
                    w->setContentNonOwned (ed, true);
                    w->setVisible (true);
                    editors.push_back (ed);
                    windows.push_back (std::move (w));
                }
                pump (600);
                log ("      pumped UI");
                process (40); // audio while the UI timers run
                pump (300);
                log ("      UI + audio ok");
            }

            // State round trip like a project save.
            log ("      state round trip");
            MemoryBlock state;
            instances[0]->getStateInformation (state);
            instances[0]->setStateInformation (state.getData(), (int) state.getSize());

            if (sc == 4)
            {
                std::reverse (instances.begin(), instances.end());
                std::reverse (windows.begin(), windows.end());
                std::reverse (editors.begin(), editors.end());
            }
            for (size_t i = 0; i < instances.size(); ++i)
            {
                if (i < windows.size())
                {
                    log ("      clearing window content");
                    windows[i]->clearContentComponent();
                    log ("      deleting editor");
                    delete editors[i];
                    log ("      editor deleted");
                    windows[i].reset();
                    if (sc != 2) pump (200);
                }
                log ("      releasing + deleting instance");
                instances[i]->releaseResources();
                instances[i].reset();
                log ("      instance deleted");
                if (sc != 2) pump (200);
            }
            pump (300);
            log ("PASS  " + String (scenarios[sc]));
        }
        log ("....  shutting down host (unloads the plug-in module)");
    }
    log ("PASS  module unloaded cleanly");
    return 0;
}
