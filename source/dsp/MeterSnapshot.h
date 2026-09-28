#pragma once

#include "DecibelUtils.h"

#include <array>
#include <cstdint>

namespace investigate
{

/** The plain data MeterBank publishes every process() call (roadmap
    "MeterSnapshot contents"). A trivially-copyable struct of arrays and
    scalars only - no owned memory, no pointers - so a reader on any thread
    can take a cheap copy (see MeterBank's seqlock-protected publish path).

    Channel index 0 is the left channel (or the only channel, when mono);
    index 1 is right. Every level is unreferenced dBFS/dBTP (RMS is *not*
    sine-referenced - the editor applies the +3.01 dB AES17 offset and the
    K-system reference, per the roadmap). `Decibels::negativeInfinityDb()`
    (negative infinity, never NaN) marks "no data yet since prepare()/
    reset()" or true digital silence, for every field where that is
    meaningful. */
struct MeterSnapshot
{
    static constexpr int maxChannels = 2;
    static constexpr float negInf = Decibels::negativeInfinityDb();

    // --- Per channel (LevelMeter + TruePeakDetector) ---
    /** Maximum |sample| over the last 50 ms, dBFS. */
    std::array<float, maxChannels> samplePeak { negInf, negInf };
    /** Maximum samplePeak reached since the last requestReset(), dBFS. */
    std::array<float, maxChannels> peakMax { negInf, negInf };
    /** RMS over the current rmsWindow, dBFS, unreferenced (a full-scale
        sine reads -3.01 dBFS here). */
    std::array<float, maxChannels> rms { negInf, negInf };
    /** Maximum interpolated true peak over the last 50 ms, dBTP. */
    std::array<float, maxChannels> truePeak { negInf, negInf };
    /** Maximum truePeak reached since the last requestReset(), dBTP. */
    std::array<float, maxChannels> truePeakMax { negInf, negInf };
    /** Count of samples with |x| >= 1.0 (0 dBFS) since the last
        requestReset(). */
    std::array<uint64_t, maxChannels> sampleClipCount {};
    /** Count of input samples whose interpolated neighbourhood reached or
        exceeded 1.0 (0 dBTP) since the last requestReset(). */
    std::array<uint64_t, maxChannels> truePeakClipCount {};

    // --- Loudness (LoudnessMeter) ---
    float momentary = negInf;
    float shortTerm = negInf;
    float integrated = negInf;
    /** In LU. 0 until at least one gated value exists (LoudnessMeter's own
        sentinel convention - LRA is a non-negative range, so negative
        infinity would misleadingly read as "very quiet" rather than "no
        data yet"). */
    float loudnessRange = 0.0f;
    float maxMomentary = negInf;
    float maxShortTerm = negInf;

    // --- Correlation (CorrelationMeter) ---
    /** -1 to +1 over a fixed 500 ms window. 0 for silence or before any
        data has arrived; never NaN. */
    float correlation = 0.0f;
    /** L/R balance in dB over a fixed 3 s window; negative means the left
        channel is louder. 0 for silence or before any data has arrived;
        never NaN. */
    float balance = 0.0f;
    /** True when the input has fewer than 2 channels: correlation and
        balance are then fixed at +1 / 0 dB (roadmap: a mono input reports a
        correlation of +1). */
    bool isMono = false;

    // --- Bookkeeping ---
    double sampleRate = 0.0;
    /** Incremented on every MeterBank::process() call, so the editor can
        detect a stopped or bypassed host by watching this stop changing
        (roadmap: "the editor uses it to detect a stopped or bypassed
        host"). Compare with != only, never <, so a uint64 wrap (not
        reachable in practice, but cheap to be exact about) stays safe. */
    uint64_t blockCounter = 0;
};

} // namespace investigate
