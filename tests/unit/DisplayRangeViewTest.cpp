// T-035 acceptance criterion 1: DisplayRange::clampViewFrequency and
// clampViewDb (range, minimum span, (0,0) -> home, idempotence).

#include "dsp/DisplayRange.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <random>

using namespace investigate;
using Catch::Matchers::WithinAbs;

namespace
{
    struct Home { double lo, hi; };
}

TEST_CASE ("DisplayRange constants", "[T-035][display-range]")
{
    CHECK_THAT (DisplayRange::minViewSpanOctaves, WithinAbs (1.0 / 6.0, 1e-15));
    CHECK (DisplayRange::minViewSpanDb == 6.0);
}

TEST_CASE ("clampViewFrequency: random inputs stay inside home, span >= minimum, idempotent", "[T-035][display-range][AC1]")
{
    const Home homes[] = { { 20.0, 20000.0 }, { 20.0, 22050.0 }, { 10.0, 24000.0 }, { 1000.0, 2000.0 },
                           { 100.0, 110.0 } /* home span below the minimum */ };

    std::mt19937_64 rng (12345);
    std::uniform_real_distribution<double> uni (0.0, 1.0);

    for (const auto& h : homes)
    {
        const double homeSpan = std::log2 (h.hi / h.lo);
        const double minSpan = std::min (DisplayRange::minViewSpanOctaves, homeSpan);

        for (int i = 0; i < 200000; ++i)
        {
            double lo, hi;
            const int kind = i % 4;
            if (kind == 0)       // anywhere, also outside the home range
            {
                lo = h.lo * std::exp2 ((uni (rng) * 2.0 - 0.5) * (homeSpan + 4.0));
                hi = h.lo * std::exp2 ((uni (rng) * 2.0 - 0.5) * (homeSpan + 4.0));
            }
            else if (kind == 1)  // inside the home range
            {
                lo = h.lo * std::exp2 (uni (rng) * homeSpan);
                hi = h.lo * std::exp2 (uni (rng) * homeSpan);
            }
            else if (kind == 2)  // tiny spans
            {
                lo = h.lo * std::exp2 (uni (rng) * homeSpan);
                hi = lo * (1.0 + 1.0e-6 * uni (rng));
            }
            else                 // huge spans
            {
                lo = std::exp2 (-10.0 * uni (rng)) * h.lo;
                hi = std::exp2 (std::log2 (h.hi) + 10.0 * uni (rng));
            }

            const auto r = DisplayRange::clampViewFrequency (lo, hi, h.lo, h.hi);
            INFO ("home " << h.lo << ".." << h.hi << " in " << lo << ".." << hi << " out " << r.low << ".." << r.high);

            REQUIRE (r.low >= h.lo);
            REQUIRE (r.high <= h.hi);
            REQUIRE (r.low < r.high);
            REQUIRE (std::log2 (r.high / r.low) >= minSpan * (1.0 - 1.0e-9));

            const auto again = DisplayRange::clampViewFrequency (r.low, r.high, h.lo, h.hi);
            REQUIRE (again.low == r.low);
            REQUIRE (again.high == r.high);
        }
    }
}

TEST_CASE ("clampViewFrequency: (0, 0) and invalid input give the home range", "[T-035][display-range][AC1]")
{
    const auto z = DisplayRange::clampViewFrequency (0.0, 0.0, 20.0, 20000.0);
    CHECK (z.low == 20.0);
    CHECK (z.high == 20000.0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const auto& in : { std::pair { nan, nan }, std::pair { 0.0, 1000.0 }, std::pair { -5.0, 1000.0 },
                            std::pair { 100.0, inf }, std::pair { 100.0, 0.0 } })
    {
        const auto r = DisplayRange::clampViewFrequency (in.first, in.second, 20.0, 20000.0);
        INFO (in.first << ", " << in.second);
        CHECK (r.low == 20.0);
        CHECK (r.high == 20000.0);
    }
}

TEST_CASE ("clampViewFrequency: legal views pass through, narrow views widen about the centre, outside views slide", "[T-035][display-range][AC1]")
{
    SECTION ("legal view unchanged")
    {
        const auto r = DisplayRange::clampViewFrequency (200.0, 800.0, 20.0, 20000.0);
        CHECK (r.low == 200.0);
        CHECK (r.high == 800.0);
    }
    SECTION ("view equal to home")
    {
        const auto r = DisplayRange::clampViewFrequency (20.0, 20000.0, 20.0, 20000.0);
        CHECK (r.low == 20.0);
        CHECK (r.high == 20000.0);
    }
    SECTION ("view larger than home gives home")
    {
        const auto r = DisplayRange::clampViewFrequency (1.0, 100000.0, 20.0, 20000.0);
        CHECK (r.low == 20.0);
        CHECK (r.high == 20000.0);
    }
    SECTION ("reversed pair is swapped")
    {
        const auto r = DisplayRange::clampViewFrequency (800.0, 200.0, 20.0, 20000.0);
        CHECK (r.low == 200.0);
        CHECK (r.high == 800.0);
    }
    SECTION ("narrow view widens to 1/6 octave around its log centre")
    {
        const auto r = DisplayRange::clampViewFrequency (1000.0, 1000.001, 20.0, 20000.0);
        const double centre = 0.5 * (std::log2 (r.low) + std::log2 (r.high));
        CHECK_THAT (centre, WithinAbs (0.5 * (std::log2 (1000.0) + std::log2 (1000.001)), 1e-9));
        CHECK_THAT (std::log2 (r.high / r.low), WithinAbs (1.0 / 6.0, 1e-9));
    }
    SECTION ("view poking out below is slid in, keeping its span")
    {
        const auto r = DisplayRange::clampViewFrequency (15.0, 18.0, 20.0, 20000.0);
        CHECK (r.low == 20.0);
        CHECK_THAT (std::log2 (r.high / r.low), WithinAbs (std::log2 (18.0 / 15.0), 1e-9));
    }
    SECTION ("view poking out above is slid in, keeping its span")
    {
        const auto r = DisplayRange::clampViewFrequency (15000.0, 40000.0, 20.0, 20000.0);
        CHECK (r.high == 20000.0);
        CHECK_THAT (std::log2 (r.high / r.low), WithinAbs (std::log2 (40000.0 / 15000.0), 1e-9));
    }
}

TEST_CASE ("clampViewDb: random inputs stay inside home, span >= minimum, idempotent", "[T-035][display-range][AC1]")
{
    const Home homes[] = { { -90.0, 6.0 }, { -120.0, 12.0 }, { -18.0, -6.0 }, { 0.0, 3.0 } /* span below the minimum */ };

    std::mt19937_64 rng (777);
    std::uniform_real_distribution<double> uni (-1.0, 1.0);

    for (const auto& h : homes)
    {
        const double homeSpan = h.hi - h.lo;
        const double minSpan = std::min (DisplayRange::minViewSpanDb, homeSpan);

        for (int i = 0; i < 200000; ++i)
        {
            double lo = h.lo + uni (rng) * (homeSpan + 100.0);
            double hi = (i % 3 == 0) ? lo + 1.0e-6 * std::abs (uni (rng)) : h.lo + uni (rng) * (homeSpan + 100.0);
            if (lo == 0.0 && hi == 0.0)
                continue;

            const auto r = DisplayRange::clampViewDb (lo, hi, h.lo, h.hi);
            INFO ("home " << h.lo << ".." << h.hi << " in " << lo << ".." << hi << " out " << r.low << ".." << r.high);

            REQUIRE (r.low >= h.lo);
            REQUIRE (r.high <= h.hi);
            REQUIRE (r.high - r.low >= minSpan * (1.0 - 1.0e-9));

            const auto again = DisplayRange::clampViewDb (r.low, r.high, h.lo, h.hi);
            REQUIRE (again.low == r.low);
            REQUIRE (again.high == r.high);
        }
    }
}

TEST_CASE ("clampViewDb: (0, 0) and invalid input give the home range; legal views pass through", "[T-035][display-range][AC1]")
{
    const auto z = DisplayRange::clampViewDb (0.0, 0.0, -90.0, 6.0);
    CHECK (z.low == -90.0);
    CHECK (z.high == 6.0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto n = DisplayRange::clampViewDb (nan, 0.0, -90.0, 6.0);
    CHECK (n.low == -90.0);
    CHECK (n.high == 6.0);

    const auto legal = DisplayRange::clampViewDb (-60.0, -20.0, -90.0, 6.0);
    CHECK (legal.low == -60.0);
    CHECK (legal.high == -20.0);

    const auto narrow = DisplayRange::clampViewDb (-40.0, -39.0, -90.0, 6.0);
    CHECK_THAT (narrow.high - narrow.low, WithinAbs (6.0, 1e-9));
    CHECK_THAT (0.5 * (narrow.low + narrow.high), WithinAbs (-39.5, 1e-9));

    const auto slid = DisplayRange::clampViewDb (-100.0, -80.0, -90.0, 6.0);
    CHECK (slid.low == -90.0);
    CHECK_THAT (slid.high - slid.low, WithinAbs (20.0, 1e-9));
}
