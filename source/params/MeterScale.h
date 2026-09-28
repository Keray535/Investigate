#pragma once

namespace investigate
{

/** Matches the `meterScale` parameter's choice order exactly
    (docs/roadmap.md parameter table: dBFS, K-20, K-14, K-12). There is no
    corresponding DSP enum: `MeterSnapshot` always reports dBFS, and the
    K-system offset is applied on display only (roadmap: "the editor applies
    ... the K-system reference when meterScale is K-20, K-14 or K-12"), so
    this enum lives in source/params/ rather than source/dsp/. */
enum class MeterScale
{
    dBFS,
    k20,
    k14,
    k12
};

} // namespace investigate
