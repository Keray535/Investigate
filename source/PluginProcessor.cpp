#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <bit>
#include <utility>

namespace investigate
{

namespace
{
    constexpr const char* kStateType          = "InvestigateState";
    constexpr int         kStateVersion       = 1;
    constexpr const char* kViewStateType      = "ViewState";
    constexpr const char* kEditorWidthProp    = "editorWidth";
    constexpr const char* kEditorHeightProp   = "editorHeight";
    constexpr const char* kMetersCollapsedProp = "metersCollapsed";

    // T-010 defaults (docs/design/ui-spec.md section 2: default size 1200 x 700).
    constexpr int  kDefaultEditorWidth  = 1200;
    constexpr int  kDefaultEditorHeight = 700;
    constexpr bool kDefaultMetersCollapsed = false;

    constexpr uint64_t freezeFlagBit = uint64_t { 1 } << 32;

    // Both floats in one 64-bit word: low half fLo, high half fHi.
    uint64_t packView (float lo, float hi) noexcept
    {
        return static_cast<uint64_t> (std::bit_cast<uint32_t> (lo))
             | (static_cast<uint64_t> (std::bit_cast<uint32_t> (hi)) << 32);
    }

    std::pair<float, float> unpackView (uint64_t bits) noexcept
    {
        return { std::bit_cast<float> (static_cast<uint32_t> (bits & 0xffffffffu)),
                 std::bit_cast<float> (static_cast<uint32_t> (bits >> 32)) };
    }
}

// ============================================================================
// AnalysisThread (T-009): the roadmap's "private class in PluginProcessor.cpp
// deriving from juce::Thread". Owns no DSP state itself - it only drives
// SpectrumEngine::processPending() with settings built fresh from the cached
// APVTS parameter atomics every poll, plus the sample rate / channel counts
// captured by InvestigateProcessor at the last prepareToPlay().
//
// Threading contract (roadmap Threading model): started by
// setAnalysisActive(true), stopped by setAnalysisActive(false); polls with
// wait(5) rather than being signalled, because the audio thread must never
// perform an OS signal (not real-time-safe). InvestigateProcessor guarantees
// this thread is always fully stopped (joined) before sampleRate/
// numMainChannels/numSidechainChannels are written again (prepareToPlay), so
// this class can hold plain references to them without its own
// synchronisation.
// ============================================================================
class InvestigateProcessor::AnalysisThread final : public juce::Thread
{
public:
    AnalysisThread (SpectrumEngine& engineIn,
                     const ParameterRefs& paramsIn,
                     const double& sampleRateIn,
                     const int& numMainChannelsIn,
                     const int& numSidechainChannelsIn,
                     std::atomic<bool>& spectrumResetRequestedIn,
                     const std::atomic<uint64_t>& viewBitsIn,
                     const std::atomic<uint64_t>& freezeBitsIn)
        : juce::Thread ("Investigate Analysis"),
          engine (engineIn),
          params (paramsIn),
          sampleRate (sampleRateIn),
          numMainChannels (numMainChannelsIn),
          numSidechainChannels (numSidechainChannelsIn),
          spectrumResetRequested (spectrumResetRequestedIn),
          viewBits (viewBitsIn),
          freezeBits (freezeBitsIn)
    {
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            // Consumed here (analysis thread), never on the audio thread -
            // SpectrumEngine::resetSecondary() is documented
            // analysis-thread-only.
            if (spectrumResetRequested.exchange (false, std::memory_order_acq_rel))
                engine.resetSecondary();

            SpectrumSettings settings;
            settings.sampleRate            = sampleRate;
            settings.fftSize               = params.getFftSize();
            settings.overlap               = params.getOverlap();
            settings.window                = params.getWindow();
            settings.channelMode           = params.getChannelMode();
            settings.avgTimeMs             = params.getAvgTimeMs();
            settings.secondary             = params.getSecondary();
            settings.numMainChannels       = numMainChannels;
            settings.numSidechainChannels  = numSidechainChannels;
            settings.freqLow               = params.getFreqLow();
            settings.freqHigh              = params.getFreqHigh();
            settings.dbLow                 = params.getDbLow();
            settings.dbHigh                = params.getDbHigh();
            settings.smoothing             = params.getSmoothing();
            settings.slopeDbPerOct         = params.getSlope();
            settings.numPoints             = SpectrumFrame::maxPoints;
            settings.showSidechain         = params.getShowSidechain();

            // One atomic load each: the packed pair can never be torn.
            const auto view = unpackView (viewBits.load (std::memory_order_acquire));
            const auto freeze = freezeBits.load (std::memory_order_acquire);
            settings.viewFreqLow  = view.first;
            settings.viewFreqHigh = view.second;
            settings.frozen       = (freeze & freezeFlagBit) != 0;
            settings.freezeEpoch  = static_cast<uint32_t> (freeze & 0xffffffffu);

            engine.processPending (settings);

            // Polls rather than being signalled (roadmap: "The audio thread
            // never signals it, because an OS event signal is not real-time
            // safe"). stopThread() still wakes this promptly: it calls
            // notify() internally after signalling threadShouldExit().
            wait (5);
        }
    }

private:
    SpectrumEngine& engine;
    const ParameterRefs& params;
    const double& sampleRate;
    const int& numMainChannels;
    const int& numSidechainChannels;
    std::atomic<bool>& spectrumResetRequested;
    const std::atomic<uint64_t>& viewBits;
    const std::atomic<uint64_t>& freezeBits;
};

void InvestigateProcessor::setSpectrumView (float fLo, float fHi) noexcept
{
    viewBits.store (packView (fLo, fHi), std::memory_order_release);
}

void InvestigateProcessor::setSpectrumFrozen (bool shouldBeFrozen) noexcept
{
    auto current = freezeBits.load (std::memory_order_acquire);

    for (;;)
    {
        // Epoch only moves on a real state change.
        if (((current & freezeFlagBit) != 0) == shouldBeFrozen)
            return;

        const auto epoch = static_cast<uint32_t> (current & 0xffffffffu) + 1u;
        const auto next = static_cast<uint64_t> (epoch) | (shouldBeFrozen ? freezeFlagBit : 0u);

        if (freezeBits.compare_exchange_weak (current, next, std::memory_order_acq_rel, std::memory_order_acquire))
            return;
    }
}

uint32_t InvestigateProcessor::getFreezeEpoch() const noexcept
{
    return static_cast<uint32_t> (freezeBits.load (std::memory_order_acquire) & 0xffffffffu);
}

InvestigateProcessor::InvestigateProcessor()
    : juce::AudioProcessor (BusesProperties()
                                 .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                 // Disabled by default: most hosts do not connect a sidechain
                                 // unless the user asks for one (roadmap: buses and channel
                                 // layouts; risk #6).
                                 .withInput ("Sidechain", juce::AudioChannelSet::disabled(), true)),
      apvts (*this, nullptr, kStateType, createParameterLayout()),
      paramRefs (apvts)
{
    apvts.state.setProperty ("stateVersion", kStateVersion, nullptr);
    ensureViewStateDefaults();

    analysisThread = std::make_unique<AnalysisThread> (spectrumEngine,
                                                         paramRefs,
                                                         preparedSampleRate,
                                                         preparedNumMainChannels,
                                                         preparedNumSidechainChannels,
                                                         spectrumResetRequested,
                                                         viewBits,
                                                         freezeBits);

    // Investigate never adds latency or a tail (roadmap Budgets); stated
    // explicitly here so a host that queries it before the first
    // processBlock call still sees 0, not an uninitialised default.
    setLatencySamples (0);
}

InvestigateProcessor::~InvestigateProcessor()
{
    // Defensive: normally the editor's destructor has already called
    // setAnalysisActive(false) by the time the processor itself is
    // destroyed, but a host could in principle destroy the processor first.
    // juce::Thread requires stopThread() before its own destructor runs.
    if (analysisThread != nullptr && analysisThread->isThreadRunning())
        analysisThread->stopThread (50);
}

void InvestigateProcessor::ensureViewStateDefaults()
{
    auto viewState = apvts.state.getOrCreateChildWithName (kViewStateType, nullptr);

    if (! viewState.hasProperty (kEditorWidthProp))
        viewState.setProperty (kEditorWidthProp, kDefaultEditorWidth, nullptr);

    if (! viewState.hasProperty (kEditorHeightProp))
        viewState.setProperty (kEditorHeightProp, kDefaultEditorHeight, nullptr);

    if (! viewState.hasProperty (kMetersCollapsedProp))
        viewState.setProperty (kMetersCollapsedProp, kDefaultMetersCollapsed, nullptr);
}

void InvestigateProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl (analysisLock);

    const int mainChannels = juce::jlimit (1, 2, getMainBusNumInputChannels());
    const int sidechainChannels = isSidechainActive()
                                       ? juce::jlimit (1, 2, getChannelCountOfBus (true, 1))
                                       : 0;

    // SpectrumEngine::prepare() fixes the FIFOs' channel counts (T-005 log)
    // and is not safe to call while the analysis thread might be consuming
    // from them; MeterBank::prepare() also resets its internal state, so
    // both are re-prepared together, with the analysis thread stopped, on
    // every prepareToPlay() call (roadmap/T-009 design requirement), not
    // only when the layout actually changed - this is the standard JUCE
    // plugin pattern and keeps the "stop before re-prepare" invariant
    // trivially true.
    if (analysisThread->isThreadRunning())
        analysisThread->stopThread (50);

    meterBank.prepare (sampleRate, samplesPerBlock, mainChannels);
    meterBank.setRmsWindowMs (paramRefs.getRmsWindowMs());

    spectrumEngine.prepare (sampleRate, samplesPerBlock, mainChannels, sidechainChannels);

    preparedSampleRate = sampleRate;
    preparedNumMainChannels = mainChannels;
    preparedNumSidechainChannels = sidechainChannels;

    // Restart only if analysis is still meant to be active (an editor is
    // open): analysisRefCount, not the thread's own prior running state, is
    // the source of truth, so this is correct even if releaseResources()
    // already stopped the thread earlier in the same session.
    if (analysisRefCount > 0)
        analysisThread->startThread();

    // Investigate never adds latency or a tail (roadmap Budgets); re-stated
    // after every prepareToPlay() in case a host queries it mid-session.
    setLatencySamples (0);
}

void InvestigateProcessor::releaseResources()
{
    const juce::ScopedLock sl (analysisLock);

    if (analysisThread->isThreadRunning())
        analysisThread->stopThread (50);
}

bool InvestigateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    // Main input and output: mono to mono, or stereo to stereo, the same
    // layout on both sides (roadmap: Buses and channel layouts).
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    // Sidechain input (bus index 1, if the host/format exposes it at all):
    // disabled, mono or stereo, independent of the main layout.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.getChannelSet (true, 1);

        if (sidechain != juce::AudioChannelSet::disabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void InvestigateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // A host may legitimately call processBlock with 0 samples (e.g. a
    // transport edge case); every meter/FIFO call below is well-defined for
    // numSamples == 0, but skipping them entirely avoids doing pointless
    // work and keeps blockCounter's "did the host actually run" semantics
    // (MeterSnapshot doc) tied to blocks that carried audio.
    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    // getBusBuffer() never allocates: it returns a lightweight AudioBuffer
    // aliasing the relevant channel pointers already inside `buffer`
    // (real-time-safe, the standard JUCE multi-bus processBlock pattern).
    // Blocks larger than the size given to prepareToPlay() are handled
    // safely here too: MeterBank/SpectrumEngine's process()/push() calls
    // have no fixed block-size ceiling (see their own class comments) - a
    // larger numSamples is simply more samples in one call, not a buffer
    // overrun.
    auto mainBus = getBusBuffer (buffer, true, 0);
    const int numMainChannels = mainBus.getNumChannels();

    meterBank.setRmsWindowMs (paramRefs.getRmsWindowMs());
    meterBank.process (mainBus.getArrayOfReadPointers(), numMainChannels, numSamples);

    // The FIFO push is skipped entirely while no editor is open (roadmap:
    // "Editor-free runs... the FIFO push is skipped, but the meters still
    // run"); the audio thread never touches SpectrumEngine at all in that
    // case, beyond this one atomic load.
    if (analysisActive.load (std::memory_order_acquire))
    {
        spectrumEngine.pushMain (mainBus.getArrayOfReadPointers(), numMainChannels, numSamples);

        if (isSidechainActive())
        {
            auto sidechainBus = getBusBuffer (buffer, true, 1);
            spectrumEngine.pushSidechain (sidechainBus.getArrayOfReadPointers(), sidechainBus.getNumChannels(), numSamples);
        }
    }

    // Bit-exact pass-through: the main buffer is never written. Any
    // sidechain channels present in the buffer are analysed only, never
    // copied to the output (roadmap: Buses and channel layouts; Signal
    // flow).
}

void InvestigateProcessor::setAnalysisActive (bool shouldBeActive)
{
    const juce::ScopedLock sl (analysisLock);

    if (shouldBeActive)
    {
        if (analysisRefCount++ == 0)
        {
            analysisActive.store (true, std::memory_order_release);
            analysisThread->startThread();
        }
    }
    else
    {
        jassert (analysisRefCount > 0);

        if (analysisRefCount > 0 && --analysisRefCount == 0)
        {
            analysisActive.store (false, std::memory_order_release);
            analysisThread->stopThread (50);

            // The zoom and freeze are not persisted: the next editor starts
            // at Fit and unfrozen (T-036). Done after the join so no poll can
            // observe a half-reset pair.
            setSpectrumView (0.0f, 0.0f);
            setSpectrumFrozen (false);
        }
    }
}

juce::AudioProcessorEditor* InvestigateProcessor::createEditor()
{
    return new InvestigateEditor (*this);
}

bool InvestigateProcessor::hasEditor() const
{
    return true;
}

const juce::String InvestigateProcessor::getName() const
{
    return JucePlugin_Name;
}

bool InvestigateProcessor::acceptsMidi() const
{
    return false;
}

bool InvestigateProcessor::producesMidi() const
{
    return false;
}

double InvestigateProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int InvestigateProcessor::getNumPrograms()
{
    return 1;
}

int InvestigateProcessor::getCurrentProgram()
{
    return 0;
}

void InvestigateProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String InvestigateProcessor::getProgramName (int /*index*/)
{
    return {};
}

void InvestigateProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

void InvestigateProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void InvestigateProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Empty data (e.g. a host creating a fresh instance with no prior save):
    // keep the defaults the constructor already set up.
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    // Corrupt data (including arbitrary random bytes) fails to parse as XML:
    // keep the current state rather than crash.
    if (xml == nullptr)
        return;

    // A payload from something other than this plugin's own state (wrong
    // root element name): ignore it rather than risk building a nonsense
    // parameter tree from it.
    if (! xml->hasTagName (apvts.state.getType()))
        return;

    auto newState = juce::ValueTree::fromXml (*xml);

    if (! newState.isValid())
        return;

    // replaceState() loads every parameter it finds by ID and leaves any
    // parameter it does not find (an older save missing a parameter that was
    // added later, or a newer save whose IDs this build does not know) at its
    // current value, which for a freshly constructed instance is the
    // default. Properties and children this build does not recognise (e.g.
    // from a newer stateVersion) simply ride along, unread.
    apvts.replaceState (newState);

    // Bring an older or missing ViewState child up to date with any property
    // this build expects but the saved state predates, without touching
    // anything the saved state did provide.
    ensureViewStateDefaults();

    // This build only ever reads and writes stateVersion 1 semantics; stamping
    // it here (rather than trusting whatever the loaded state carried) means
    // the next getStateInformation() always describes what this build
    // actually restored.
    apvts.state.setProperty ("stateVersion", kStateVersion, nullptr);
}

} // namespace investigate

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new investigate::InvestigateProcessor();
}
