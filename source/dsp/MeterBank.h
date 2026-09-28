#pragma once

#include "CorrelationMeter.h"
#include "LevelMeter.h"
#include "LoudnessMeter.h"
#include "MeterSnapshot.h"
#include "TruePeakDetector.h"

#include <atomic>
#include <cstdint>

namespace investigate
{

/** Owns every audio-thread meter (LevelMeter, TruePeakDetector,
    LoudnessMeter, CorrelationMeter), aggregates their readouts into a
    MeterSnapshot every process() call, and publishes it lock-free for any
    thread to read (task T-007). This is the only audio-thread entry point
    T-009's InvestigateProcessor calls: prepare(), process(),
    setRmsWindowMs(), requestReset() and getSnapshot() - the roadmap's
    "MeterBank, MeterSnapshot" module row and its threading model (audio
    thread writes, any thread reads). Pure DSP: depends only on the other
    source/dsp/ meter classes and the standard library, no editor or
    AudioProcessor dependency.

    - **process()** runs the four meters in sequence, increments
      blockCounter, and publishes a fresh MeterSnapshot. Real-time-safe: no
      allocation, no locks; every meter it calls is itself real-time-safe
      (see their own class comments).
    - **requestReset()** is callable from any thread: a single
      std::atomic<bool> store. process() consumes it first (an atomic
      exchange) and, if set, calls resetStatistics() on LevelMeter,
      TruePeakDetector and LoudnessMeter - exactly the roadmap's reset
      list (peak max, TP max, both clip counts, integrated loudness, LRA,
      M max and S max). CorrelationMeter is deliberately not touched
      (correlation/balance are continuous readouts, not "since reset"
      quantities - see its own class comment), and RMS is not touched for
      the same reason.
    - **getSnapshot()** uses a seqlock (a version counter, odd = a write is
      in progress, even = stable), not a plain two-slot double buffer.
      T-002 section 7 describes double buffering (option (a)) as
      sufficient "because the audio thread is the sole writer and reads
      are tear-free once the index swap is acquire/release-ordered" - but
      that argument implicitly assumes a reader finishes its copy before
      the writer comes back around to the buffer it just read from. With
      only two buffers and a fixed alternation (A, B, A, B, ...), nothing
      actually enforces that: a reader thread that stalls (pre-emption,
      a debugger breakpoint, a slow adversarial test hammering both
      process() and getSnapshot() from two threads as fast as possible)
      for longer than **one** process() call can have the buffer it is
      mid-copying from start being overwritten again by the very next
      process() call - a genuine data race the "sole writer, acquire/
      release index" argument does not actually rule out for an
      unbounded stall. The task's own design requirement gives the exact
      fallback for this: "If the design cannot guarantee that, use a
      seqlock-style version counter with retry on the reader side" - used
      here because the double-buffer guarantee cannot be made for an
      arbitrary reader stall duration, whereas the seqlock's
      retry-until-a-stable-version-is-observed protocol is correct for
      *any* stall duration, at the cost of a cheap spin-retry on the
      reader - which the task explicitly allows, since every reader runs
      on a non-real-time thread. */
class MeterBank
{
public:
    MeterBank() = default;

    /** Prepares every owned meter for `numChannels` (clamped to 1..2, the
        roadmap's mono-mono or stereo-stereo main bus) at `sampleRate`, and
        resets blockCounter, the pending reset flag and the published
        snapshot. May allocate (only LoudnessMeter::prepare() does, for its
        per-channel filter state). */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Runs every meter over `numSamples` new samples on each channel of
        `channels`, applies any pending requestReset(), increments
        blockCounter, and publishes a fresh snapshot. Real-time-safe: no
        allocation, no locks. Intended to be called once per processBlock,
        after juce::ScopedNoDenormals has already been engaged by the
        caller (the roadmap has InvestigateProcessor do this once per
        processBlock, ahead of the whole MeterBank). */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Forwards to LevelMeter::setRmsWindowMs(). Real-time-safe: a single
        atomic store, callable from any thread. */
    void setRmsWindowMs (float windowMs) noexcept;

    /** Callable from any thread: sets an atomic flag that the next
        process() call consumes and acts on (see the class comment for
        exactly what is reset). Real-time-safe: a single atomic store. */
    void requestReset() noexcept;

    /** Returns a torn-free copy of the most recently published snapshot.
        Real-time-safe (a bounded spin-retry, never a lock or a wait on
        another thread) and callable from any thread, including the audio
        thread itself, though its only intended callers are the analysis/
        message threads (roadmap threading model). Before the first
        process() call, returns a default-constructed MeterSnapshot
        (blockCounter == 0, every level at its -inf/0 sentinel). */
    MeterSnapshot getSnapshot() const noexcept;

private:
    void publishSnapshot() noexcept;

    LevelMeter levelMeter;
    TruePeakDetector truePeakDetector;
    LoudnessMeter loudnessMeter;
    CorrelationMeter correlationMeter;

    std::atomic<bool> resetRequested { false };

    double sampleRate = 48000.0;
    int preparedChannels = 2;
    uint64_t blockCounter = 0;

    // Seqlock storage - see the class comment for the correctness argument.
    // `snapshot`'s fields are plain (not std::atomic) for cheap writes;
    // `version`'s acquire/release pairing is what makes a reader's
    // retry-verified copy safe (the standard seqlock idiom, used
    // throughout systems and audio code; this project targets Windows/
    // MSVC/x86-64 only, per the roadmap, where this idiom is
    // well-established practice).
    MeterSnapshot snapshot {};
    mutable std::atomic<uint32_t> version { 0 };
};

} // namespace investigate
