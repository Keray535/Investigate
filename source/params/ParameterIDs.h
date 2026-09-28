#pragma once

namespace investigate::ParamIDs
{

/** Every APVTS parameter ID, exactly as they appear in the roadmap parameter
    table (docs/roadmap.md, Architecture > Parameter table). Every parameter
    is built with `juce::ParameterID { id, 1 }` (ParameterLayout.cpp): a new
    parameter after release gets a new ID here and a version hint > 1, never
    a renumbered or reused ID (CLAUDE.md). */

constexpr const char* channelMode    = "channelMode";
constexpr const char* fftSize        = "fftSize";
constexpr const char* overlap        = "overlap";
constexpr const char* window         = "window";
constexpr const char* avgTime        = "avgTime";
constexpr const char* smoothing      = "smoothing";
constexpr const char* slope          = "slope";
constexpr const char* secondary      = "secondary";
constexpr const char* freqLow        = "freqLow";
constexpr const char* freqHigh       = "freqHigh";
constexpr const char* dbLow          = "dbLow";
constexpr const char* dbHigh         = "dbHigh";
constexpr const char* showSidechain  = "showSidechain";
constexpr const char* meterScale     = "meterScale";
constexpr const char* rmsWindow      = "rmsWindow";

} // namespace investigate::ParamIDs
