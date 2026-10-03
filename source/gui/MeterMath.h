#pragma once

#include "../dsp/DecibelUtils.h"
#include "../params/MeterScale.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string_view>

/** Pure meter arithmetic for the editor (docs/design/ui-spec.md sections 8 and
    12): scale mapping, number formatting, the display ballistics and the
    correlation filter. Nothing here touches a component, so the unit tests
    can pin every rule without a GUI, and nothing allocates, so the vblank
    path can call it every frame. */
namespace investigate::MeterMath
{

//==============================================================================
// Scale

/** The AES17 sine reference: a full-scale sine reads -3.0103 dBFS RMS, so the
    K-system meters add this before placing 0 at -N dBFS (ui-spec 8.2). */
inline constexpr float aes17OffsetDb = 3.0103f;

/** The level bars span a fixed -60 to +6 dBFS in every scale mode. */
inline constexpr float barFloorDb = -60.0f;
inline constexpr float barCeilDb = 6.0f;

/** At or below this a level is shown as "-inf" (ui-spec 8.3). */
inline constexpr float silenceDb = -120.0f;

/** 0 for dBFS, else the K-system reference N in dBFS (20, 14 or 12). */
constexpr int kReference (MeterScale scale) noexcept
{
    switch (scale)
    {
        case MeterScale::k20: return 20;
        case MeterScale::k14: return 14;
        case MeterScale::k12: return 12;
        case MeterScale::dBFS: break;
    }

    return 0;
}

constexpr bool isKScale (MeterScale scale) noexcept { return scale != MeterScale::dBFS; }

/** The meterScale parameter's choice index to the enum, clamped. */
constexpr MeterScale scaleFromIndex (int index) noexcept
{
    return index <= 0 ? MeterScale::dBFS
         : index == 1 ? MeterScale::k20
         : index == 2 ? MeterScale::k14
                      : MeterScale::k12;
}

/** Where the RMS bar ends on the fixed dBFS axis: the AES17 offset is added in
    the K modes, nothing in dBFS (ui-spec 8.2). Silence stays -inf. */
inline float rmsBarDb (float rmsDbfs, MeterScale scale) noexcept
{
    return isKScale (scale) ? rmsDbfs + aes17OffsetDb : rmsDbfs;
}

/** The RMS value in display units: dBFS as reported, or K units where 0 is
    -N dBFS (ui-spec 8.3). A -20 dBFS peak sine (-23.01 reported) reads 0 on
    K-20. */
inline float rmsDisplay (float rmsDbfs, MeterScale scale) noexcept
{
    return isKScale (scale) ? rmsDbfs + aes17OffsetDb + (float) kReference (scale) : rmsDbfs;
}

/** The zone boundaries on the dBFS axis (ui-spec 8.2): safe below `warmFrom`,
    warm up to `hotFrom`, hot from there. */
struct Zones
{
    float warmFrom = -12.0f;
    float hotFrom = -3.0f;
};

constexpr Zones zonesFor (MeterScale scale) noexcept
{
    return isKScale (scale) ? Zones { (float) -kReference (scale), (float) (-kReference (scale) + 4) }
                            : Zones { -12.0f, -3.0f };
}

struct Tick
{
    float dbfs = 0.0f;  // position on the bar axis
    int label = 0;      // the number printed, in display units
};

struct TickSet
{
    std::array<Tick, 32> ticks {};
    int count = 0;
};

/** ui-spec 8.2: dBFS every 6 dB from +6 to -60, or every 12 when 6 dB is under
    20 px; K modes every 4 K units (8 when under 20 px), labelled in K units. */
inline TickSet computeTicks (MeterScale scale, double trackHeightPx) noexcept
{
    TickSet set;
    const double pxPerDb = trackHeightPx / (double) (barCeilDb - barFloorDb);

    auto add = [&] (int dbfs, int label)
    {
        if (set.count < (int) set.ticks.size())
            set.ticks[(size_t) set.count++] = { (float) dbfs, label };
    };

    if (! isKScale (scale))
    {
        const int step = 6.0 * pxPerDb >= 20.0 ? 6 : 12;

        for (int v = 6; v >= -60; v -= step)
            add (v, v);
    }
    else
    {
        const int n = kReference (scale);
        const int step = 4.0 * pxPerDb >= 20.0 ? 4 : 8;

        for (int k = (6 + n) / step * step; k >= -60 + n; k -= step)
            add (k - n, k);
    }

    return set;
}

/** The track in whole physical pixels: `topPx` is +6 dBFS, `bottomPx` is
    -60 dBFS (exclusive). */
struct BarGeometry
{
    int topPx = 0;
    int bottomPx = 0;

    int heightPx() const noexcept { return bottomPx - topPx; }

    /** The pixel row of a level; anything at or below the floor, -inf or NaN
        gives `bottomPx`, anything above the ceiling `topPx`. */
    int yFor (float db) const noexcept
    {
        if (! (db > barFloorDb))
            return bottomPx;

        if (db >= barCeilDb)
            return topPx;

        return topPx + (int) std::lround (((double) barCeilDb - (double) db) / (double) (barCeilDb - barFloorDb)
                                          * (double) heightPx());
    }
};

//==============================================================================
// Number formatting (ui-spec 8.3 to 8.5). Text is built into a fixed buffer,
// so formatting never allocates and the panel can compare the text before it
// builds a juce::String.

class FixedText
{
public:
    static constexpr int capacity = 24;

    std::string_view view() const noexcept { return { chars.data(), (size_t) length }; }
    bool operator== (const FixedText& other) const noexcept { return view() == other.view(); }
    bool operator!= (const FixedText& other) const noexcept { return ! (*this == other); }
    bool empty() const noexcept { return length == 0; }
    juce::String toString() const { return juce::String::fromUTF8 (chars.data(), length); }

    FixedText& append (std::string_view s) noexcept
    {
        const int n = std::min ((int) s.size(), capacity - length);

        for (int i = 0; i < n; ++i)
            chars[(size_t) (length + i)] = s[(size_t) i];

        length += n;
        return *this;
    }

    FixedText& appendMinus() noexcept { return append ("\xE2\x88\x92"); }

    /** `tenthsAbs` / 10 with one decimal ("12.4"). */
    FixedText& appendTenths (int tenthsAbs) noexcept
    {
        char tmp[16];
        const int n = std::snprintf (tmp, sizeof (tmp), "%d.%d", tenthsAbs / 10, tenthsAbs % 10);
        return append ({ tmp, (size_t) std::max (0, n) });
    }

    FixedText& appendHundredths (int hundredthsAbs) noexcept
    {
        char tmp[16];
        const int n = std::snprintf (tmp, sizeof (tmp), "%d.%02d", hundredthsAbs / 100, hundredthsAbs % 100);
        return append ({ tmp, (size_t) std::max (0, n) });
    }

private:
    std::array<char, capacity> chars {};
    int length = 0;
};

inline constexpr std::string_view negInfText = "\xE2\x88\x92" "inf";
inline constexpr std::string_view dashText = "\xE2\x80\x94";

/** value x 10 rounded half away from zero (so -0.04 gives 0 and +0.05 gives
    1); finite inputs only, clamped to a safe range. */
inline int roundToTenths (double v) noexcept
{
    return (int) std::lround (std::clamp (v, -99999.0, 99999.0) * 10.0);
}

inline bool isSilent (float db) noexcept { return ! std::isfinite (db) || db <= silenceDb; }

/** Signed, one decimal: "-6.0", "+0.3", "0.0"; "-inf" when non-finite or at or
    below -120 (peak, TP, RMS in the table). */
inline FixedText formatLevel (float db) noexcept
{
    FixedText t;

    if (isSilent (db))
        return t.append (negInfText);

    const int tenths = roundToTenths ((double) db);

    if (tenths == 0)
        return t.append ("0.0");

    if (tenths > 0)
        t.append ("+");
    else
        t.appendMinus();

    return t.appendTenths(std::abs (tenths));
}

/** A level already in display units: the same format as `formatLevel`. The
    RMS row passes `rmsDisplay (...)`; the silence test is on the raw dBFS
    value, which the caller checks. */
inline FixedText formatRms (float rmsDbfs, MeterScale scale) noexcept
{
    return isSilent (rmsDbfs) ? formatLevel (rmsDbfs) : formatLevel (rmsDisplay (rmsDbfs, scale));
}

/** Unsigned except for a minus: "12.4", "-0.4", "0.0". */
inline FixedText formatUnsigned1 (double v) noexcept
{
    FixedText t;
    const int tenths = roundToTenths (v);

    if (tenths == 0)
        return t.append ("0.0");

    if (tenths < 0)
        t.appendMinus();

    return t.appendTenths (std::abs (tenths));
}

/** Headroom = 0 dBTP minus TP max: "1.2", "-0.4"; a dash while TP max is -inf.
    `negative` is true when the shown value is below zero (error colour). */
inline FixedText formatHeadroom (float truePeakMaxDb, bool& negative) noexcept
{
    negative = false;

    if (isSilent (truePeakMaxDb))
        return FixedText().append (dashText);

    negative = roundToTenths (-(double) truePeakMaxDb) < 0;
    return formatUnsigned1 (-(double) truePeakMaxDb);
}

/** Crest = peak over the window minus RMS (raw dBFS); a dash if either is -inf. */
inline FixedText formatCrest (float windowPeakDb, float rmsDbfs) noexcept
{
    if (isSilent (windowPeakDb) || isSilent (rmsDbfs))
        return FixedText().append (dashText);

    return formatUnsigned1 ((double) windowPeakDb - (double) rmsDbfs);
}

/** "0" to "9999", then "9999+". */
inline FixedText formatCount (uint64_t n) noexcept
{
    FixedText t;

    if (n > 9999u)
        return t.append ("9999+");

    char tmp[8];
    const int len = std::snprintf (tmp, sizeof (tmp), "%u", (unsigned) n);
    return t.append ({ tmp, (size_t) std::max (0, len) });
}

/** Momentary and short-term loudness: "-14.2"; "-inf" when non-finite or
    below -99.9. */
inline FixedText formatLoudnessLive (float lufs) noexcept
{
    if (! std::isfinite (lufs) || lufs < -99.9f)
        return FixedText().append (negInfText);

    return formatUnsigned1 ((double) lufs);
}

/** The two maxima: like the live values, "-inf" below -99.9 LUFS (a reset in
    silence leaves the true value, for example -153.7); a dash while non-finite. */
inline FixedText formatLoudnessMax (float lufs) noexcept
{
    if (! std::isfinite (lufs))
        return FixedText().append (dashText);

    return formatLoudnessLive (lufs);
}

/** Integrated loudness: one decimal, a dash while non-finite. */
inline FixedText formatLoudnessHeld (float lufs) noexcept
{
    if (! std::isfinite (lufs))
        return FixedText().append (dashText);

    return formatUnsigned1 ((double) lufs);
}

/** LRA in LU (the unit is drawn separately); a dash while I is a dash. */
inline FixedText formatLra (float lra, float integratedLufs) noexcept
{
    if (! std::isfinite (integratedLufs) || ! std::isfinite (lra))
        return FixedText().append (dashText);

    return formatUnsigned1 ((double) std::max (0.0f, lra));
}

/** Signed, two decimals: "+0.82", "-0.15", "0.00". */
inline FixedText formatCorrelation (float correlation) noexcept
{
    FixedText t;
    const double c = std::isfinite (correlation) ? std::clamp ((double) correlation, -1.0, 1.0) : 0.0;
    const int hundredths = (int) std::lround (c * 100.0);

    if (hundredths == 0)
        return t.append ("0.00");

    if (hundredths > 0)
        t.append ("+");
    else
        t.appendMinus();

    return t.appendHundredths (std::abs (hundredths));
}

/** "L 1.2 dB" (the snapshot balance is negative: left is louder), "R 0.8 dB",
    "C 0.0 dB" when |b| < 0.05, "L >60 dB" beyond 60. */
inline FixedText formatBalance (float balanceDb) noexcept
{
    FixedText t;
    const double b = std::isfinite (balanceDb) ? (double) balanceDb : 0.0;
    const double a = std::abs (b);

    if (a < 0.05)
        return t.append ("C 0.0 dB");

    t.append (b < 0.0 ? "L " : "R ");

    if (a > 60.0)
        return t.append (">60 dB");

    return t.appendTenths (roundToTenths (a)).append (" dB");
}

//==============================================================================
/** Editor update cap (ui-spec 8.1: at most 60 Hz). Keeps a rolling deadline so
    a 144 Hz vblank averages exactly 60 updates per second and a 60 Hz vblank
    with jitter is not thinned. */
struct UpdateRateLimiter
{
    static constexpr double periodMs = 1000.0 / 60.0;
    static constexpr double toleranceMs = 1.0;

    bool shouldUpdate (double nowMs) noexcept
    {
        if (nowMs < nextMs - toleranceMs)
            return false;

        nextMs = (nowMs - nextMs > periodMs) ? nowMs + periodMs : nextMs + periodMs;
        return true;
    }

    double nextMs = -1.0e12;
};

//==============================================================================
// Ballistics (ui-spec 8.2). Editor side only: the snapshot is never changed.

/** Clamps a snapshot level to a finite range or -inf: NaN and anything at or
    below -200 dB become -inf, +inf becomes +200. */
inline float sanitizeLevel (float db) noexcept
{
    if (std::isnan (db) || db <= -200.0f)
        return Decibels::negativeInfinityDb();

    return std::min (db, 200.0f);
}

/** Instant rise, then a linear fall (20 dB/s for the peak bar). */
struct FallingLevel
{
    static constexpr float fallDbPerSecond = 20.0f;

    float value = Decibels::negativeInfinityDb();

    void reset() noexcept { value = Decibels::negativeInfinityDb(); }

    void update (float input, double dtSeconds) noexcept
    {
        value = std::max (input, value - fallDbPerSecond * (float) dtSeconds);
    }
};

/** Holds the highest level for 1500 ms, then falls at 30 dB/s, never below
    `floorDb` (the bar for the peak tick, the input itself for the TP value). */
struct HeldLevel
{
    static constexpr double holdMs = 1500.0;
    static constexpr float fallDbPerSecond = 30.0f;

    float value = Decibels::negativeInfinityDb();
    double heldSinceMs = 0.0;

    void reset() noexcept { value = Decibels::negativeInfinityDb(); heldSinceMs = 0.0; }

    void update (float input, double nowMs, double dtSeconds, float floorDb) noexcept
    {
        if (input >= value)
        {
            value = input;
            heldSinceMs = nowMs;
        }
        else if (nowMs - heldSinceMs > holdMs)
        {
            value = std::max (floorDb, value - fallDbPerSecond * (float) dtSeconds);
        }

        value = std::max (value, floorDb);
    }
};

/** The fixed 256-entry ring of (time, sample peak) the crest factor needs
    (ui-spec 8.3): the window is the RMS window, 50 ms to 3 s. Entries are
    spaced at least `windowMs / 240` apart (and 8 ms), so the ring always
    covers the window; the sample peak is itself a 50 ms maximum, so spacing
    below that loses nothing. */
class CrestRing
{
public:
    static constexpr int size = 256;

    void reset() noexcept
    {
        head = 0;
        count = 0;
        lastPushMs = -1.0e12;
    }

    void push (double nowMs, float samplePeakDb, double windowMs) noexcept
    {
        const double spacing = std::max (8.0, windowMs / 240.0);

        if (nowMs - lastPushMs < spacing)
            return;

        lastPushMs = nowMs;
        times[(size_t) head] = nowMs;
        values[(size_t) head] = samplePeakDb;
        head = (head + 1) % size;
        count = std::min (count + 1, size);
    }

    /** The highest entry not older than `windowMs`, or -inf. */
    float maxWithin (double nowMs, double windowMs) const noexcept
    {
        float best = Decibels::negativeInfinityDb();

        for (int i = 0; i < count; ++i)
        {
            const auto idx = (size_t) i;

            if (nowMs - times[idx] <= windowMs)
                best = std::max (best, values[idx]);
        }

        return best;
    }

private:
    std::array<double, size> times {};
    std::array<float, size> values {};
    int head = 0;
    int count = 0;
    double lastPushMs = -1.0e12;
};

/** One-pole display filter on the correlation bar, tau = 50 ms (ui-spec 8.5). */
struct CorrelationFilter
{
    static constexpr double tauSeconds = 0.05;

    float value = 0.0f;

    void reset (float v = 0.0f) noexcept { value = v; }

    float update (float target, double dtSeconds) noexcept
    {
        const float t = std::isfinite (target) ? std::clamp (target, -1.0f, 1.0f) : 0.0f;
        const float a = 1.0f - (float) std::exp (-std::max (0.0, dtSeconds) / tauSeconds);
        value += (t - value) * a;
        return value;
    }
};

/** ui-spec 8.7: the snapshot is stale when `blockCounter` has not changed for
    250 ms. Compared with != only. */
struct StaleDetector
{
    static constexpr double staleAfterMs = 250.0;

    uint64_t lastCounter = 0;
    double lastChangeMs = 0.0;
    bool started = false;
    bool stale = false;

    void reset() noexcept { started = false; stale = false; }

    bool update (uint64_t counter, double nowMs) noexcept
    {
        if (! started || counter != lastCounter)
        {
            started = true;
            lastCounter = counter;
            lastChangeMs = nowMs;
            stale = false;
        }
        else
        {
            stale = nowMs - lastChangeMs >= staleAfterMs;
        }

        return stale;
    }
};

//==============================================================================
// Pixel quantisation (T-029 rule 4): a bar is repainted only when one of these
// whole-pixel values changes.

/** Physical-pixel tick thickness for a logical thickness (never under 1 px). */
inline int thicknessPx (float logicalPx, float pixelScale) noexcept
{
    return std::max (1, (int) std::lround ((double) logicalPx * (double) pixelScale));
}

} // namespace investigate::MeterMath
