// T-008/T-009 black-box render harness (plugin-validator).
//
// Drives investigate::InvestigateProcessor only through the public
// juce::AudioProcessor host-facing surface -- the same surface a VST3 or
// Standalone host uses -- to independently re-verify the acceptance criteria
// of docs/tasks/T-008-params-buses-state.md and docs/tasks/T-009-processor-wiring.md
// that are plugin-validator's responsibility: bus layout negotiation, the
// null test across block sizes and sample rates (with analysis active and
// inactive), latency/tail reporting, parameter automation, a binary-level
// (getStateInformation/setStateInformation) state round trip between two
// separately constructed instances, isSidechainActive() at the host level, a
// sample-rate change mid-session while analysis is active, the analysis
// thread's <= 50 ms stop budget (roadmap Budgets), and processBlock/MeterBank
// CPU timing (roadmap Budgets, independent re-check of docs/qa/T-009-unit.md's
// thin p99 margin).
//
// This program never reads or depends on source/dsp/ or any internal class;
// it only calls the same juce::AudioProcessor / editor-facing API surface
// (docs/roadmap.md, "Processor API used by the editor") a host or editor
// uses. It is intentionally independent of tests/unit/*.cpp (which cover
// similar ground at the white-box level): a different signal generator, a
// different bus matrix walk and direct getChannelIndexInProcessBlockBuffer()
// -based buffer layout rather than assuming channel order.
//
// Exit code: 0 if every check passes, 1 otherwise. Prints one line per check.

#include "PluginProcessor.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <thread>

using investigate::InvestigateProcessor;

namespace
{
    int totalChecks = 0;
    int failedChecks = 0;

    void check (bool condition, const juce::String& description)
    {
        ++totalChecks;
        if (condition)
        {
            std::printf ("[PASS] %s\n", description.toRawUTF8());
        }
        else
        {
            ++failedChecks;
            std::printf ("[FAIL] %s\n", description.toRawUTF8());
        }
    }

    juce::AudioProcessor::BusesLayout makeLayout (juce::AudioChannelSet mainInOut,
                                                   juce::AudioChannelSet sidechain)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (mainInOut);
        layout.inputBuses.add (sidechain);
        layout.outputBuses.add (mainInOut);
        return layout;
    }

    // ------------------------------------------------------------------
    // 1. Bus layout negotiation, driven through setBusesLayout() (what a
    //    host actually calls), not isBusesLayoutSupported() directly.
    // ------------------------------------------------------------------
    void runBusLayoutMatrix()
    {
        std::printf ("\n-- Bus layout negotiation (setBusesLayout) --\n");

        const juce::AudioChannelSet mains[] { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() };
        const juce::AudioChannelSet sidechains[] {
            juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()
        };

        for (auto main : mains)
        {
            for (auto sc : sidechains)
            {
                InvestigateProcessor processor;
                const auto layout = makeLayout (main, sc);
                const bool accepted = processor.setBusesLayout (layout);
                check (accepted, "accepts main " + main.getDescription() + "/" + main.getDescription()
                                      + " with sidechain " + sc.getDescription());
            }
        }

        {
            InvestigateProcessor processor;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::mono());
            layout.inputBuses.add (juce::AudioChannelSet::disabled());
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            check (! processor.setBusesLayout (layout), "rejects mono-in / stereo-out");
        }

        {
            InvestigateProcessor processor;
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.inputBuses.add (juce::AudioChannelSet::disabled());
            layout.outputBuses.add (juce::AudioChannelSet::mono());
            check (! processor.setBusesLayout (layout), "rejects stereo-in / mono-out");
        }

        const juce::AudioChannelSet threePlus[] {
            juce::AudioChannelSet::createLCR(), juce::AudioChannelSet::quadraphonic(),
            juce::AudioChannelSet::create5point1()
        };
        for (auto layout3 : threePlus)
        {
            InvestigateProcessor processor;
            const auto layout = makeLayout (layout3, juce::AudioChannelSet::disabled());
            check (! processor.setBusesLayout (layout), "rejects " + layout3.getDescription() + " main (3+ channels)");
        }

        {
            InvestigateProcessor processor;
            const auto layout = makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::createLCR());
            check (! processor.setBusesLayout (layout), "rejects a 3-channel sidechain");
        }
    }

    // ------------------------------------------------------------------
    // 2. Null test: accepted layout, several block sizes and sample rates,
    //    a distinct signal on main and a distinct signal on the sidechain.
    //    The main output must be bit-exact (difference exactly 0) to the
    //    main input, and the sidechain content must never appear in the
    //    main output channels.
    // ------------------------------------------------------------------
    void runNullTest (const juce::String& label, juce::AudioChannelSet mainSet, juce::AudioChannelSet sidechainSet,
                       double sampleRate, int blockSize)
    {
        InvestigateProcessor processor;
        const auto layout = makeLayout (mainSet, sidechainSet);
        if (! processor.setBusesLayout (layout))
        {
            check (false, label + " @ " + juce::String (sampleRate) + "Hz/" + juce::String (blockSize)
                               + ": setBusesLayout should have succeeded for the null test precondition");
            return;
        }

        processor.prepareToPlay (sampleRate, blockSize);

        const int mainChannels = mainSet.size();
        const bool sidechainActive = sidechainSet != juce::AudioChannelSet::disabled();
        const int sidechainChannels = sidechainActive ? sidechainSet.size() : 0;

        const int totalChannels = juce::jmax (processor.getTotalNumInputChannels(),
                                               processor.getTotalNumOutputChannels());

        juce::AudioBuffer<float> buffer (totalChannels, blockSize);
        buffer.clear();

        // Distinct, non-trivial signals: main gets a ramp + per-channel offset,
        // sidechain gets an unrelated sine-ish pattern, so the two could never
        // be mistaken for one another if channels were ever mixed up.
        for (int ch = 0; ch < mainChannels; ++ch)
        {
            const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 0, ch);
            auto* data = buffer.getWritePointer (bufferCh);
            for (int i = 0; i < blockSize; ++i)
                data[i] = 0.01f * (float) ((i % 997) - 498) + 0.1f * (float) (ch + 1);
        }

        if (sidechainActive)
        {
            for (int ch = 0; ch < sidechainChannels; ++ch)
            {
                const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 1, ch);
                auto* data = buffer.getWritePointer (bufferCh);
                for (int i = 0; i < blockSize; ++i)
                    data[i] = 0.5f + 0.001f * (float) (i % 500) + 10.0f * (float) (ch + 1);
            }
        }

        // Snapshot the main input exactly as sent, before processBlock.
        std::vector<std::vector<float>> mainInputSnapshot (static_cast<size_t> (mainChannels));
        for (int ch = 0; ch < mainChannels; ++ch)
        {
            const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 0, ch);
            mainInputSnapshot[static_cast<size_t> (ch)].assign (buffer.getReadPointer (bufferCh),
                                                                 buffer.getReadPointer (bufferCh) + blockSize);
        }

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        bool bitExact = true;
        for (int ch = 0; ch < mainChannels; ++ch)
        {
            const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 0, ch);
            const auto* out = buffer.getReadPointer (bufferCh);
            const auto& in = mainInputSnapshot[static_cast<size_t> (ch)];
            for (int i = 0; i < blockSize; ++i)
            {
                if (out[i] != in[static_cast<size_t> (i)])
                {
                    bitExact = false;
                    break;
                }
            }
            if (! bitExact)
                break;
        }
        check (bitExact, label + " @ " + juce::String (sampleRate, 0) + "Hz/" + juce::String (blockSize)
                              + " blk: main output bit-exact to main input");

        // The sidechain content must not appear anywhere in the output: since
        // the bit-exact check above already proves every main-output channel
        // equals the distinct main-input pattern (never the sidechain
        // pattern), the remaining, unambiguous check is that the sidechain's
        // own buffer channels still hold their own input pattern unchanged --
        // proving processBlock never wrote into the shared buffer at all,
        // main or sidechain.
        bool sidechainLeaked = false;
        if (sidechainActive)
        {
            for (int ch = 0; ch < sidechainChannels; ++ch)
            {
                const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 1, ch);
                const auto* data = buffer.getReadPointer (bufferCh);
                for (int i = 0; i < blockSize; ++i)
                {
                    const float expected = 0.5f + 0.001f * (float) (i % 500) + 10.0f * (float) (ch + 1);
                    if (data[i] != expected)
                    {
                        sidechainLeaked = true;
                        break;
                    }
                }
            }
        }
        check (! sidechainLeaked, label + " @ " + juce::String (sampleRate, 0) + "Hz/" + juce::String (blockSize)
                                       + " blk: sidechain channels untouched, never copied to output");
    }

    // ------------------------------------------------------------------
    // 3. Latency / tail.
    // ------------------------------------------------------------------
    void runLatencyTailChecks()
    {
        std::printf ("\n-- Latency / tail --\n");
        InvestigateProcessor processor;
        check (processor.getLatencySamples() == 0, "getLatencySamples() == 0 before prepareToPlay");
        check (processor.getTailLengthSeconds() == 0.0, "getTailLengthSeconds() == 0.0 before prepareToPlay");

        const double rates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
        const int blocks[] { 1, 64, 512, 4096 };
        for (double sr : rates)
        {
            for (int bs : blocks)
            {
                processor.prepareToPlay (sr, bs);
                check (processor.getLatencySamples() == 0,
                       "getLatencySamples() == 0 after prepareToPlay(" + juce::String (sr, 0) + ", " + juce::String (bs) + ")");
                check (processor.getTailLengthSeconds() == 0.0,
                       "getTailLengthSeconds() == 0.0 after prepareToPlay(" + juce::String (sr, 0) + ", " + juce::String (bs) + ")");
            }
        }
    }

    // ------------------------------------------------------------------
    // 4. Automation sweep through the real AudioProcessorParameter API.
    // ------------------------------------------------------------------
    void runAutomationSweep()
    {
        std::printf ("\n-- Automation sweep --\n");
        InvestigateProcessor processor;
        processor.prepareToPlay (48000.0, 128);

        const char* idsToSweep[] { "slope", "avgTime", "freqLow", "channelMode", "rmsWindow", "dbHigh" };

        for (auto* id : idsToSweep)
        {
            auto* param = processor.getApvts().getParameter (id);
            if (param == nullptr)
            {
                check (false, juce::String ("parameter exists: ") + id);
                continue;
            }

            bool noThrow = true;
            bool allFinite = true;
            bool roundTripsThroughGetSet = true;

            const float positions[] { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f };
            for (float pos : positions)
            {
                try
                {
                    param->setValueNotifyingHost (pos);

                    // Drive a real block through the pass-through path while automated;
                    // output must stay finite (trivially true for pass-through, but this
                    // guards against a future regression that reads the parameter on the
                    // audio thread and does something unsafe with it).
                    juce::AudioBuffer<float> buffer (2, 128);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        auto* data = buffer.getWritePointer (ch);
                        for (int i = 0; i < 128; ++i)
                            data[i] = 0.2f * std::sin ((float) i * 0.1f + (float) ch);
                    }
                    juce::MidiBuffer midi;
                    processor.processBlock (buffer, midi);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 128; ++i)
                            if (! std::isfinite (buffer.getSample (ch, i)))
                                allFinite = false;

                    const auto text = param->getText (pos, 0);
                    const auto valueBack = param->getValueForText (text);
                    const auto textBack = param->getText (valueBack, 0);
                    if (textBack != text)
                    {
                        roundTripsThroughGetSet = false;
                        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
                        const float denormalised = ranged != nullptr ? ranged->convertFrom0to1 (pos) : -1.0f;
                        std::printf ("       %s @ pos=%.4f (raw value %.6f): getText=\"%s\" -> getValueForText=%.6f -> getText=\"%s\"\n",
                                     id, (double) pos, (double) denormalised, text.toRawUTF8(), (double) valueBack, textBack.toRawUTF8());
                    }
                }
                catch (...)
                {
                    noThrow = false;
                }
            }

            check (noThrow, juce::String (id) + ": sweep across its range raises no exception");
            check (allFinite, juce::String (id) + ": processBlock output stays finite while automated");
            check (roundTripsThroughGetSet, juce::String (id) + ": getText(getValueForText(getText(pos))) is stable "
                                                                 "at the 7 sampled positions (0/0.1/0.25/0.5/0.75/0.9/1.0)");
        }
    }

    // ------------------------------------------------------------------
    // 5. Binary-level state round trip: two independently constructed
    //    instances, driven only through getStateInformation/setStateInformation.
    // ------------------------------------------------------------------
    void runStateRoundTrip()
    {
        std::printf ("\n-- State round trip (binary level, two instances) --\n");

        auto source = std::make_unique<InvestigateProcessor>();
        auto& sourceParams = source->getApvts().processor.getParameters();

        juce::Random random (424242);
        std::vector<float> setValues;
        for (auto* p : sourceParams)
        {
            const float v = random.nextFloat();
            p->setValueNotifyingHost (v);

            // What getStateInformation() will actually persist is APVTS's own
            // denormalised copy of the parameter (synced through its internal
            // listener), not necessarily param->getValue() verbatim: e.g.
            // juce::AudioParameterBool's getValue() returns the raw value
            // passed to setValueNotifyingHost() unsnapped, while APVTS stores
            // (and therefore round-trips) the thresholded 0/1 value. Reading
            // back through getRawParameterValue()/convertTo0to1() -- the same
            // path APVTS itself uses -- captures the value that a save
            // immediately afterwards will actually carry, for every
            // parameter type uniformly.
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            const auto* raw = ranged != nullptr ? source->getApvts().getRawParameterValue (ranged->paramID) : nullptr;
            setValues.push_back (raw != nullptr ? ranged->convertTo0to1 (raw->load()) : p->getValue());
        }

        auto sourceViewState = source->getApvts().state.getChildWithName ("ViewState");
        sourceViewState.setProperty ("editorWidth", 1440, nullptr);
        sourceViewState.setProperty ("editorHeight", 810, nullptr);
        sourceViewState.setProperty ("metersCollapsed", true, nullptr);

        juce::MemoryBlock saved;
        source->getStateInformation (saved);
        check (saved.getSize() > 0, "getStateInformation() on the source instance produces non-empty data");

        // Destroy the source entirely before constructing the destination, so
        // nothing can be shared except the serialised bytes.
        source.reset();

        auto destination = std::make_unique<InvestigateProcessor>();
        destination->setStateInformation (saved.getData(), (int) saved.getSize());

        auto& destParams = destination->getApvts().processor.getParameters();
        check (destParams.size() == (int) setValues.size(), "destination instance has the same parameter count");

        bool allMatch = true;
        for (int i = 0; i < destParams.size() && i < (int) setValues.size(); ++i)
        {
            const float diff = std::abs (destParams[i]->getValue() - setValues[static_cast<size_t> (i)]);
            if (diff > 1.0e-6f)
            {
                allMatch = false;
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (destParams[i]);
                std::printf ("       mismatch: index %d id=%s expected=%.8f actual=%.8f diff=%.8f\n",
                             i, ranged != nullptr ? ranged->paramID.toRawUTF8() : "?",
                             (double) setValues[static_cast<size_t> (i)], (double) destParams[i]->getValue(), (double) diff);
            }
        }
        check (allMatch, "every parameter matches within 1e-6 after save/restore into a fresh instance");

        auto destViewState = destination->getApvts().state.getChildWithName ("ViewState");
        check (destViewState.isValid(), "ViewState child survives the round trip");
        check ((int) destViewState.getProperty ("editorWidth") == 1440, "ViewState.editorWidth survives (1440)");
        check ((int) destViewState.getProperty ("editorHeight") == 810, "ViewState.editorHeight survives (810)");
        check ((bool) destViewState.getProperty ("metersCollapsed") == true, "ViewState.metersCollapsed survives (true)");

        // Corrupt / empty data must not crash the binary-level API.
        bool crashed = false;
        try
        {
            InvestigateProcessor robust;
            robust.setStateInformation (nullptr, 0);
            juce::MemoryBlock garbage;
            juce::Random garbageRandom (1);
            garbage.setSize (1024);
            for (size_t i = 0; i < garbage.getSize(); ++i)
                garbage[i] = (char) garbageRandom.nextInt (256);
            robust.setStateInformation (garbage.getData(), (int) garbage.getSize());
        }
        catch (...)
        {
            crashed = true;
        }
        check (! crashed, "setStateInformation(nullptr/0) and 1KB of random bytes do not throw/crash");
    }

    // ------------------------------------------------------------------
    // 6. T-009: null test with analysis active. MeterBank + SpectrumEngine
    //    both run while an editor is "open" (setAnalysisActive(true)); the
    //    main buffer must still never be written.
    // ------------------------------------------------------------------
    void runNullTestWithAnalysisActive()
    {
        std::printf ("\n-- Null test with analysis active (T-009) --\n");

        struct LayoutCase { const char* label; juce::AudioChannelSet main; juce::AudioChannelSet sidechain; };
        const LayoutCase cases[] {
            { "mono/mono, sidechain disabled",     juce::AudioChannelSet::mono(),   juce::AudioChannelSet::disabled() },
            { "stereo/stereo, sidechain disabled", juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled() },
            { "stereo/stereo, mono sidechain",     juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono() },
            { "stereo/stereo, stereo sidechain",   juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo() },
        };

        for (auto& lc : cases)
        {
            InvestigateProcessor processor;
            const auto layout = makeLayout (lc.main, lc.sidechain);
            if (! processor.setBusesLayout (layout))
            {
                check (false, juce::String (lc.label) + ": setBusesLayout should have succeeded");
                continue;
            }

            processor.prepareToPlay (48000.0, 128);
            processor.setAnalysisActive (true);

            const int mainChannels = lc.main.size();
            const int totalChannels = juce::jmax (processor.getTotalNumInputChannels(),
                                                   processor.getTotalNumOutputChannels());

            bool bitExact = true;
            // Several blocks, so the analysis thread has real work queued
            // concurrently with processBlock (a live producer/consumer
            // situation, not just a single call).
            for (int block = 0; block < 20 && bitExact; ++block)
            {
                juce::AudioBuffer<float> buffer (totalChannels, 128);
                buffer.clear();

                std::vector<std::vector<float>> mainInputSnapshot (static_cast<size_t> (mainChannels));
                for (int ch = 0; ch < mainChannels; ++ch)
                {
                    const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 0, ch);
                    auto* data = buffer.getWritePointer (bufferCh);
                    for (int i = 0; i < 128; ++i)
                        data[i] = 0.3f * std::sin (0.05f * (float) (i + block * 128) + (float) ch);
                    mainInputSnapshot[static_cast<size_t> (ch)].assign (data, data + 128);
                }

                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);

                for (int ch = 0; ch < mainChannels && bitExact; ++ch)
                {
                    const int bufferCh = processor.getChannelIndexInProcessBlockBuffer (true, 0, ch);
                    const auto* out = buffer.getReadPointer (bufferCh);
                    const auto& in = mainInputSnapshot[static_cast<size_t> (ch)];
                    for (int i = 0; i < 128; ++i)
                        if (out[i] != in[static_cast<size_t> (i)]) { bitExact = false; break; }
                }
            }

            check (bitExact, juce::String (lc.label) + ": bit-exact pass-through with analysis active (20 blocks)");

            processor.setAnalysisActive (false);
        }
    }

    // ------------------------------------------------------------------
    // 7. T-009: isSidechainActive() at the host level, including a live bus
    //    enable/disable and sample-rate change while analysis is active.
    // ------------------------------------------------------------------
    void runSidechainActiveAndSampleRateChange()
    {
        std::printf ("\n-- isSidechainActive() and sample-rate change mid-session (T-009) --\n");

        {
            InvestigateProcessor processor;
            check (! processor.isSidechainActive(), "isSidechainActive() false before any bus layout is set");

            processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled()));
            check (! processor.isSidechainActive(), "isSidechainActive() false: sidechain bus disabled");

            processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono()));
            check (processor.isSidechainActive(), "isSidechainActive() true: sidechain bus enabled, mono");

            processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()));
            check (processor.isSidechainActive(), "isSidechainActive() true: sidechain bus enabled, stereo");

            processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled()));
            check (! processor.isSidechainActive(), "isSidechainActive() false again after re-disabling the sidechain bus");
        }

        {
            InvestigateProcessor processor;
            processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()));
            processor.setAnalysisActive (true);

            bool crashed = false;
            try
            {
                const double rates[] { 44100.0, 96000.0, 48000.0 };
                for (double sr : rates)
                {
                    processor.prepareToPlay (sr, 128);

                    // Feed some audio at the new rate and poll for a fresh
                    // frame; the roadmap gives 200ms for a frame to reflect a
                    // settings/sample-rate change, but this harness's only
                    // goal here is "no crash, meters/frames keep working", not
                    // re-verifying that specific unit-tested timing budget.
                    for (int block = 0; block < 10; ++block)
                    {
                        juce::AudioBuffer<float> buffer (4, 128);
                        for (int ch = 0; ch < 4; ++ch)
                        {
                            auto* data = buffer.getWritePointer (ch);
                            for (int i = 0; i < 128; ++i)
                                data[i] = 0.25f * std::sin (0.1f * (float) i + (float) ch);
                        }
                        juce::MidiBuffer midi;
                        processor.processBlock (buffer, midi);
                    }

                    std::this_thread::sleep_for (std::chrono::milliseconds (30));
                    const auto snapshot = processor.getMeterSnapshot();
                    (void) snapshot;
                    investigate::SpectrumFrame frame;
                    (void) processor.getLatestSpectrumFrame (frame);
                }
            }
            catch (...)
            {
                crashed = true;
            }

            check (! crashed, "sample rate change 44.1 -> 96 -> 48 kHz while analysis active: no crash/exception");

            processor.setAnalysisActive (false);
        }
    }

    // ------------------------------------------------------------------
    // 8. T-009: analysis thread stop timing. setAnalysisActive(false) calls
    //    juce::Thread::stopThread(50) synchronously (roadmap Budgets:
    //    "Analysis thread stop <= 50 ms"), so timing the call itself from
    //    the outside is a valid black-box measurement of that budget.
    // ------------------------------------------------------------------
    void runAnalysisThreadStopTiming()
    {
        std::printf ("\n-- Analysis thread stop timing (T-009 Budgets: <= 50 ms) --\n");

        InvestigateProcessor processor;
        processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()));
        processor.prepareToPlay (48000.0, 128);

        constexpr int kIterations = 20;
        double worstMs = 0.0;
        bool allWithinBudget = true;

        for (int i = 0; i < kIterations; ++i)
        {
            processor.setAnalysisActive (true);

            // Give the analysis thread something to do before stopping it,
            // rather than stopping an idle thread.
            juce::AudioBuffer<float> buffer (4, 128);
            for (int ch = 0; ch < 4; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int s = 0; s < 128; ++s)
                    data[s] = 0.2f * std::sin (0.1f * (float) s + (float) ch);
            }
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            const auto start = std::chrono::steady_clock::now();
            processor.setAnalysisActive (false);
            const auto elapsedMs = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();

            worstMs = std::max (worstMs, elapsedMs);
            if (elapsedMs > 50.0)
                allWithinBudget = false;
        }

        std::printf ("       worst setAnalysisActive(false) call over %d iterations: %.3f ms\n", kIterations, worstMs);
        check (allWithinBudget, "setAnalysisActive(false) returns within 50ms every time (" + juce::String (kIterations) + " iterations)");
    }

    // ------------------------------------------------------------------
    // 9. T-009 performance re-check: processBlock CPU at 48kHz/128 samples,
    //    stereo main + stereo sidechain, analysis active (roadmap Budgets:
    //    mean <= 20us, p99 <= 50us). Also an analysis-inactive run at the
    //    same layout as a black-box proxy for "MeterBank alone" (roadmap:
    //    mean <= 15us, p99 <= 40us) -- with analysis inactive, processBlock's
    //    only work is meterBank.setRmsWindowMs()/.process() plus one atomic
    //    load, so this isolates MeterBank's cost as closely as the public
    //    AudioProcessor surface allows without reading source/dsp/ internals.
    // ------------------------------------------------------------------
    void measureProcessBlockTiming (bool analysisActive, const juce::String& label, double meanBudgetUs, double p99BudgetUs)
    {
        InvestigateProcessor processor;
        processor.setBusesLayout (makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()));
        processor.prepareToPlay (48000.0, 128);

        if (analysisActive)
            processor.setAnalysisActive (true);

        constexpr int kWarmup = 200;
        constexpr int kIterations = 5000;

        juce::AudioBuffer<float> buffer (4, 128);
        for (int ch = 0; ch < 4; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < 128; ++i)
                data[i] = 0.3f * std::sin (0.05f * (float) i + (float) ch);
        }

        juce::MidiBuffer midi;

        for (int i = 0; i < kWarmup; ++i)
            processor.processBlock (buffer, midi);

        std::vector<double> samplesUs;
        samplesUs.reserve (kIterations);

        for (int i = 0; i < kIterations; ++i)
        {
            const auto start = std::chrono::steady_clock::now();
            processor.processBlock (buffer, midi);
            const auto elapsedUs = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - start).count();
            samplesUs.push_back (elapsedUs);
        }

        if (analysisActive)
            processor.setAnalysisActive (false);

        double mean = 0.0;
        for (double v : samplesUs) mean += v;
        mean /= (double) samplesUs.size();

        std::vector<double> sorted (samplesUs);
        std::sort (sorted.begin(), sorted.end());
        const auto p99Index = (size_t) ((double) sorted.size() * 0.99);
        const double p99 = sorted[std::min (p99Index, sorted.size() - 1)];

        std::printf ("       %s: mean = %.3f us (budget <= %.1f), p99 = %.3f us (budget <= %.1f)\n",
                     label.toRawUTF8(), mean, meanBudgetUs, p99, p99BudgetUs);

        check (mean <= meanBudgetUs, label + ": mean <= " + juce::String (meanBudgetUs, 1) + "us");
        check (p99 <= p99BudgetUs, label + ": p99 <= " + juce::String (p99BudgetUs, 1) + "us");
    }
}

int main()
{
    std::printf ("Investigate render harness (T-008 black-box validation)\n");

    runBusLayoutMatrix();

    std::printf ("\n-- Null test matrix --\n");
    struct LayoutCase { const char* label; juce::AudioChannelSet main; juce::AudioChannelSet sidechain; };
    const LayoutCase layoutCases[] {
        { "stereo/stereo + stereo sidechain", juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo() },
        { "mono/mono, sidechain disabled",    juce::AudioChannelSet::mono(),   juce::AudioChannelSet::disabled() },
    };
    const double sampleRates[] { 44100.0, 96000.0 };
    const int blockSizes[] { 1, 64, 4096 };

    for (auto& lc : layoutCases)
        for (double sr : sampleRates)
            for (int bs : blockSizes)
                runNullTest (lc.label, lc.main, lc.sidechain, sr, bs);

    runLatencyTailChecks();
    runAutomationSweep();
    runStateRoundTrip();

    runNullTestWithAnalysisActive();
    runSidechainActiveAndSampleRateChange();
    runAnalysisThreadStopTiming();

    std::printf ("\n-- Performance (T-009 Budgets, 48kHz/128 samples, stereo+stereo sidechain) --\n");
    measureProcessBlockTiming (true, "processBlock (analysis active)", 20.0, 50.0);
    measureProcessBlockTiming (false, "processBlock (analysis inactive, MeterBank-only proxy)", 15.0, 40.0);

    std::printf ("\n%d/%d checks passed, %d failed.\n", totalChecks - failedChecks, totalChecks, failedChecks);
    return failedChecks == 0 ? 0 : 1;
}
