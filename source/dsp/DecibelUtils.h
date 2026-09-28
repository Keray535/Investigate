#pragma once

#include <cmath>
#include <limits>

namespace investigate
{

/** Shared linear-to-dB conversion used by every meter in source/dsp/, so the
    "no data yet" / "true silence" sentinel is identical everywhere: negative
    infinity, matching LoudnessMeter's existing convention (roadmap
    MeterSnapshot: "All levels are reported in dBFS"). Every meter keeps its
    running accumulation entirely in the linear domain (cheap per sample) and
    converts to dB only when a getter is queried (once per block from
    MeterBank::publishSnapshot), never per sample. */
namespace Decibels
{
    /** The single "no data" / "-inf dBFS" sentinel value used by LevelMeter,
        TruePeakDetector and MeterSnapshot. Never NaN. */
    inline constexpr float negativeInfinityDb() noexcept
    {
        return -std::numeric_limits<float>::infinity();
    }

    /** 20*log10(linearAmplitude), or negativeInfinityDb() for a
        non-positive input (exact digital silence, or a defensive guard
        against a negative magnitude that should never occur since callers
        pass std::abs() results). Never NaN or +Inf for a finite,
        non-negative input. */
    inline float linearToDb (float linearAmplitude) noexcept
    {
        return linearAmplitude > 0.0f ? 20.0f * std::log10 (linearAmplitude) : negativeInfinityDb();
    }
}

} // namespace investigate
