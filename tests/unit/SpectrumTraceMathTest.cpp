// T-029 unit tests for the trace helpers in source/gui/SpectrumMath.h:
// computeColumnTops (fill bar tops), PolylineSimplifier (stroke vertex
// reduction) and the pacing rule. Pure functions, no component is built.

#include "gui/SpectrumMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using namespace investigate::SpectrumMath;

namespace
{
constexpr float kInf = std::numeric_limits<float>::infinity();

std::vector<float> tops (const std::vector<float>& xs, const std::vector<float>& ys,
                         float left, float right, float ppu, int cols)
{
    std::vector<float> t ((size_t) std::max (cols, 1), -12345.0f);
    computeColumnTops (xs.data(), ys.data(), (int) xs.size(), left, right, ppu, t.data(), cols);
    return t;
}

using V = PolylineSimplifier::Vertex;

std::vector<V> simplify (const std::vector<V>& in, float tolerance)
{
    PolylineSimplifier s (tolerance);
    std::vector<V> out;
    V v;

    for (const auto& p : in)
        if (s.add (p.x, p.y, v))
            out.push_back (v);

    if (s.finish (v))
        out.push_back (v);

    return out;
}

// Distance from p to the segment ab.
float distToSegment (V p, V a, V b)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len2 = dx * dx + dy * dy;
    float t = len2 > 0.0f ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0f;
    t = std::clamp (t, 0.0f, 1.0f);
    return std::hypot (p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

float distToPolyline (V p, const std::vector<V>& poly)
{
    if (poly.size() == 1)
        return std::hypot (p.x - poly[0].x, p.y - poly[0].y);

    float best = kInf;

    for (size_t i = 0; i + 1 < poly.size(); ++i)
        best = std::min (best, distToSegment (p, poly[i], poly[i + 1]));

    return best;
}
}

//==============================================================================
TEST_CASE ("computeColumnTops: monotone line, top is the lower of the two edge values", "[gui][tracemath][tops]")
{
    std::vector<float> xs, ys;

    for (int i = 0; i <= 100; ++i)
    {
        xs.push_back ((float) i);
        ys.push_back (80.0f - 0.5f * (float) i);
    }

    const auto t = tops (xs, ys, 0.0f, 100.0f, 1.0f, 100);

    for (int c = 0; c < 100; ++c)
        CHECK (t[(size_t) c] == Catch::Approx (std::min (ys[(size_t) c], ys[(size_t) c + 1])).margin (1.0e-4));
}

TEST_CASE ("computeColumnTops: jagged input, one point per column", "[gui][tracemath][tops]")
{
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> dist (0.0f, 200.0f);
    std::vector<float> xs, ys;

    for (int i = 0; i <= 64; ++i)
    {
        xs.push_back ((float) i);
        ys.push_back (dist (rng));
    }

    const auto t = tops (xs, ys, 0.0f, 64.0f, 1.0f, 64);

    for (int c = 0; c < 64; ++c)
        CHECK (t[(size_t) c] == Catch::Approx (std::min (ys[(size_t) c], ys[(size_t) c + 1])).margin (1.0e-4));
}

TEST_CASE ("computeColumnTops: a steep segment fills every column it crosses", "[gui][tracemath][tops]")
{
    // y = 10 x - 100 from (10, 0) to (14, 40): column c spans y 10(c-10) to 10(c-9).
    const auto t = tops ({ 10.0f, 14.0f }, { 0.0f, 40.0f }, 0.0f, 20.0f, 1.0f, 20);

    CHECK (t[10] == Catch::Approx (0.0f).margin (1.0e-4));
    CHECK (t[11] == Catch::Approx (10.0f).margin (1.0e-4));
    CHECK (t[12] == Catch::Approx (20.0f).margin (1.0e-4));
    CHECK (t[13] == Catch::Approx (30.0f).margin (1.0e-4));
    CHECK (t[14] == Catch::Approx (40.0f).margin (1.0e-4));

    for (int c : { 0, 5, 9, 15, 19 })
        CHECK (t[(size_t) c] == kInf);
}

TEST_CASE ("computeColumnTops: segments not fully inside [left, right] are skipped", "[gui][tracemath][tops]")
{
    SECTION ("starts left of the range")
    {
        const auto t = tops ({ -5.0f, 5.0f }, { 10.0f, 20.0f }, 0.0f, 10.0f, 1.0f, 10);
        for (float v : t) CHECK (v == kInf);
    }
    SECTION ("ends right of the range")
    {
        const auto t = tops ({ 5.0f, 15.0f }, { 10.0f, 20.0f }, 0.0f, 10.0f, 1.0f, 10);
        for (float v : t) CHECK (v == kInf);
    }
    SECTION ("inner segment survives while the outer ones are skipped")
    {
        const auto t = tops ({ -5.0f, 2.0f, 4.0f, 15.0f }, { 0.0f, 30.0f, 30.0f, 0.0f }, 0.0f, 10.0f, 1.0f, 10);
        CHECK (t[1] == kInf);
        CHECK (t[2] == Catch::Approx (30.0f));
        CHECK (t[3] == Catch::Approx (30.0f));
        CHECK (t[4] == Catch::Approx (30.0f));
        CHECK (t[5] == kInf);
    }
}

TEST_CASE ("computeColumnTops: fewer than two points or no columns", "[gui][tracemath][tops]")
{
    SECTION ("n = 0")
    {
        std::vector<float> t (8, 1.0f);
        computeColumnTops (nullptr, nullptr, 0, 0.0f, 8.0f, 1.0f, t.data(), 8);
        for (float v : t) CHECK (v == kInf);
    }
    SECTION ("n = 1")
    {
        const auto t = tops ({ 3.0f }, { 5.0f }, 0.0f, 8.0f, 1.0f, 8);
        for (float v : t) CHECK (v == kInf);
    }
    SECTION ("numCols = 0 and negative do not write")
    {
        float sentinel = 42.0f;
        const float xs[] { 1.0f, 2.0f }, ys[] { 3.0f, 4.0f };
        computeColumnTops (xs, ys, 2, 0.0f, 8.0f, 1.0f, &sentinel, 0);
        computeColumnTops (xs, ys, 2, 0.0f, 8.0f, 1.0f, &sentinel, -3);
        CHECK (sentinel == 42.0f);
    }
    SECTION ("numCols = 1 collects everything in the column")
    {
        const auto t = tops ({ 0.0f, 0.4f, 0.9f }, { 5.0f, 2.0f, 9.0f }, 0.0f, 1.0f, 1.0f, 1);
        CHECK (t[0] == Catch::Approx (2.0f));
    }
}

TEST_CASE ("computeColumnTops: sparse points interpolate across the gap", "[gui][tracemath][tops]")
{
    // y = 2 x over x 0..50: column c has its smallest y at its left edge, 2 c.
    const auto t = tops ({ 0.0f, 50.0f }, { 0.0f, 100.0f }, 0.0f, 50.0f, 1.0f, 50);

    for (int c = 0; c < 50; ++c)
        CHECK (t[(size_t) c] == Catch::Approx (2.0f * (float) c).margin (1.0e-3));
}

TEST_CASE ("computeColumnTops: a one-point spike keeps its peak in its column", "[gui][tracemath][tops]")
{
    const auto t = tops ({ 0.0f, 10.0f, 10.5f, 11.0f, 20.0f }, { 50.0f, 50.0f, 0.0f, 50.0f, 50.0f },
                         0.0f, 20.0f, 1.0f, 20);

    CHECK (t[10] == Catch::Approx (0.0f).margin (1.0e-4));
    CHECK (t[9] == Catch::Approx (50.0f));
    CHECK (t[12] == Catch::Approx (50.0f));
}

TEST_CASE ("computeColumnTops: fractional pixelsPerUnit and a non-zero left edge", "[gui][tracemath][tops]")
{
    // Two pixels per unit: columns 0.5 wide starting at left = 100.
    const auto t = tops ({ 100.0f, 104.0f }, { 7.0f, 7.0f }, 100.0f, 104.0f, 2.0f, 8);
    for (float v : t) CHECK (v == Catch::Approx (7.0f));

    // Slope 8 per unit: column c (width 0.5) top = 8 * 0.5 c = 4 c.
    const auto s = tops ({ 100.0f, 104.0f }, { 0.0f, 32.0f }, 100.0f, 104.0f, 2.0f, 8);

    for (int c = 0; c < 8; ++c)
        CHECK (s[(size_t) c] == Catch::Approx (4.0f * (float) c).margin (1.0e-3));
}

//==============================================================================
TEST_CASE ("PolylineSimplifier: first vertex is emitted at once, nothing more for a lone point", "[gui][tracemath][simplifier]")
{
    PolylineSimplifier s (0.1f);
    V v;
    REQUIRE (s.add (3.0f, 4.0f, v));
    CHECK (v.x == 3.0f);
    CHECK (v.y == 4.0f);
    CHECK_FALSE (s.finish (v));
}

TEST_CASE ("PolylineSimplifier: collinear runs collapse to their end points", "[gui][tracemath][simplifier]")
{
    SECTION ("horizontal")
    {
        std::vector<V> in;
        for (int i = 0; i < 100; ++i) in.push_back ({ (float) i, 5.0f });
        const auto out = simplify (in, 0.04f);
        REQUIRE (out.size() == 2);
        CHECK (out.front().x == 0.0f);
        CHECK (out.back().x == 99.0f);
    }
    SECTION ("sloped")
    {
        std::vector<V> in;
        for (int i = 0; i < 100; ++i) in.push_back ({ (float) i * 0.5f, 3.0f + 0.25f * (float) i });
        const auto out = simplify (in, 0.04f);
        REQUIRE (out.size() == 2);
        CHECK (out.back().x == Catch::Approx (49.5f));
        CHECK (out.back().y == Catch::Approx (3.0f + 0.25f * 99.0f));
    }
    SECTION ("vertical")
    {
        std::vector<V> in;
        for (int i = 0; i < 50; ++i) in.push_back ({ 2.0f, (float) i });
        const auto out = simplify (in, 0.04f);
        REQUIRE (out.size() == 2);
        CHECK (out.back().y == 49.0f);
    }
    SECTION ("two corners give four vertices")
    {
        std::vector<V> in;
        for (int i = 0; i <= 10; ++i) in.push_back ({ (float) i, 0.0f });
        for (int i = 1; i <= 10; ++i) in.push_back ({ 10.0f, (float) i * 2.0f });
        for (int i = 1; i <= 10; ++i) in.push_back ({ 10.0f + (float) i, 20.0f });
        const auto out = simplify (in, 0.04f);
        REQUIRE (out.size() == 4);
        CHECK (out[1].x == 10.0f);
        CHECK (out[1].y == 0.0f);
        CHECK (out[2].y == 20.0f);
        CHECK (out[2].x == 10.0f);
    }
}

TEST_CASE ("PolylineSimplifier: a sine stays within twice the tolerance of the original", "[gui][tracemath][simplifier]")
{
    const float tolerance = GENERATE (0.01f, 0.04f, 0.2f, 1.0f);
    const float step = GENERATE (0.05f, 0.5f, 2.0f);

    std::vector<V> in;

    for (int i = 0; i < 4000; ++i)
        in.push_back ({ (float) i * step, 50.0f * std::sin ((float) i * step * 0.02f) + 3.0f * std::sin ((float) i * step * 0.31f) });

    const auto out = simplify (in, tolerance);

    INFO ("tolerance " << tolerance << ", step " << step << ", out " << out.size() << " of " << in.size());
    REQUIRE (out.size() >= 2);
    CHECK (out.size() <= in.size());
    CHECK (out.front().x == in.front().x);
    CHECK (out.front().y == in.front().y);
    CHECK (out.back().x == in.back().x);
    CHECK (out.back().y == in.back().y);

    float worst = 0.0f;
    for (const auto& p : in)
        worst = std::max (worst, distToPolyline (p, out));

    INFO ("worst distance " << worst);
    CHECK (worst <= 2.0f * tolerance + 1.0e-3f);

    // Smooth input must actually be reduced.
    if (step <= 0.5f && tolerance >= 0.2f)
        CHECK (out.size() < in.size());
}

TEST_CASE ("PolylineSimplifier: a spike up and back survives", "[gui][tracemath][simplifier]")
{
    SECTION ("perpendicular spike")
    {
        std::vector<V> in;
        for (int i = 0; i < 20; ++i) in.push_back ({ (float) i, 0.0f });
        in.push_back ({ 20.0f, 30.0f });
        for (int i = 21; i < 40; ++i) in.push_back ({ (float) i, 0.0f });

        const auto out = simplify (in, 0.5f);
        const bool kept = std::any_of (out.begin(), out.end(), [] (V p) { return p.x == 20.0f && p.y == 30.0f; });
        CHECK (kept);
        CHECK (out.back().x == 39.0f);
    }
    SECTION ("turn back along the line")
    {
        const std::vector<V> in { { 0.0f, 0.0f }, { 10.0f, 0.0f }, { 5.0f, 0.0f }, { 20.0f, 0.0f } };
        const auto out = simplify (in, 0.04f);
        const bool kept = std::any_of (out.begin(), out.end(), [] (V p) { return p.x == 10.0f; });
        CHECK (kept);
        CHECK (out.back().x == 20.0f);
    }
    SECTION ("spike as the very first segment")
    {
        const std::vector<V> in { { 0.0f, 0.0f }, { 1.0f, 40.0f }, { 2.0f, 0.0f }, { 3.0f, 0.0f } };
        const auto out = simplify (in, 0.5f);
        const bool kept = std::any_of (out.begin(), out.end(), [] (V p) { return p.y == 40.0f; });
        CHECK (kept);
    }
}

TEST_CASE ("PolylineSimplifier: tolerance 0 drops only exactly collinear vertices", "[gui][tracemath][simplifier]")
{
    SECTION ("exactly collinear collapses")
    {
        std::vector<V> in;
        for (int i = 0; i < 30; ++i) in.push_back ({ (float) i, 2.0f * (float) i });
        CHECK (simplify (in, 0.0f).size() == 2);
    }
    SECTION ("a tiny deviation is kept")
    {
        const std::vector<V> in { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.001f }, { 3.0f, 0.0f } };
        CHECK (simplify (in, 0.0f).size() == in.size());
    }
}

TEST_CASE ("PolylineSimplifier: random jagged input passes through unchanged", "[gui][tracemath][simplifier]")
{
    // Alternating low and high random values: no three consecutive vertices can
    // be near-collinear (fully uniform noise is not used: about 1 % of vertices
    // of such noise are legitimately within the strip of their neighbours).
    std::mt19937 rng (12345);
    std::uniform_real_distribution<float> low (0.0f, 20.0f), high (80.0f, 100.0f);
    std::vector<V> in;

    for (int i = 0; i < 2048; ++i)
        in.push_back ({ (float) i * 0.7f, (i & 1) ? high (rng) : low (rng) });

    const auto out = simplify (in, 0.04f);
    REQUIRE (out.size() == in.size());

    for (size_t i = 0; i < in.size(); ++i)
    {
        CHECK (out[i].x == in[i].x);
        CHECK (out[i].y == in[i].y);
    }
}

TEST_CASE ("PolylineSimplifier: vertex count never exceeds input, ends preserved, for random input", "[gui][tracemath][simplifier]")
{
    const auto seed = GENERATE (1u, 2u, 3u, 4u, 5u);
    const float tolerance = GENERATE (0.0f, 0.04f, 3.0f);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (0.0f, 10.0f);
    std::uniform_int_distribution<int> len (1, 300);

    for (int rep = 0; rep < 20; ++rep)
    {
        const int n = len (rng);
        std::vector<V> in;
        float x = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            x += (rng() % 5 == 0) ? 0.0f : dist (rng) * 0.1f; // some repeated x
            in.push_back ({ x, (rng() % 4 == 0) ? 5.0f : dist (rng) });
        }

        const auto out = simplify (in, tolerance);
        REQUIRE_FALSE (out.empty());
        CHECK (out.size() <= in.size());
        CHECK (out.front().x == in.front().x);
        CHECK (out.front().y == in.front().y);

        if (n >= 2 && (in.back().x != in.front().x || in.back().y != in.front().y))
        {
            CHECK (out.back().x == in.back().x);
            CHECK (out.back().y == in.back().y);
        }
    }
}

TEST_CASE ("PolylineSimplifier: duplicates do not break it", "[gui][tracemath][simplifier]")
{
    SECTION ("repeated first point")
    {
        const std::vector<V> in { { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 5.0f, 5.0f }, { 5.0f, 5.0f }, { 10.0f, 10.0f } };
        const auto out = simplify (in, 0.04f);
        REQUIRE (out.size() >= 2);
        CHECK (out.size() <= in.size());
        CHECK (out.front().x == 0.0f);
        CHECK (out.back().x == 10.0f);
        CHECK (out.back().y == 10.0f);
    }
    SECTION ("all identical")
    {
        const std::vector<V> in (20, V { 3.0f, 3.0f });
        const auto out = simplify (in, 0.04f);
        REQUIRE (out.size() >= 1);
        CHECK (out.size() <= 2);
        CHECK (out.front().x == 3.0f);
    }
    SECTION ("duplicate of the pending vertex before a corner")
    {
        const std::vector<V> in { { 0.0f, 0.0f }, { 5.0f, 0.0f }, { 5.0f, 0.0f }, { 5.0f, 8.0f }, { 5.0f, 8.0f } };
        const auto out = simplify (in, 0.04f);
        CHECK (out.front().x == 0.0f);
        CHECK (out.back().y == 8.0f);
        const bool corner = std::any_of (out.begin(), out.end(), [] (V p) { return p.x == 5.0f && p.y == 0.0f; });
        CHECK (corner);
    }
}

TEST_CASE ("PolylineSimplifier: finish is idempotent", "[gui][tracemath][simplifier]")
{
    PolylineSimplifier s (0.04f);
    V v;
    s.add (0.0f, 0.0f, v);
    s.add (1.0f, 1.0f, v);
    s.add (2.0f, 2.0f, v);
    REQUIRE (s.finish (v));
    CHECK (v.x == 2.0f);
    CHECK_FALSE (s.finish (v));
}

//==============================================================================
TEST_CASE ("pacing::countLayers for every flag combination", "[gui][tracemath][pacing]")
{
    using pacing::countLayers;
    //                    dual   secondary sidechain  expected
    CHECK (countLayers (false, false, false) == 2);
    CHECK (countLayers (false, false, true)  == 4);
    CHECK (countLayers (false, true,  false) == 3);
    CHECK (countLayers (false, true,  true)  == 5);
    CHECK (countLayers (true,  false, false) == 3);
    CHECK (countLayers (true,  false, true)  == 5);
    CHECK (countLayers (true,  true,  false) == 5);
    CHECK (countLayers (true,  true,  true)  == 7);
}

TEST_CASE ("pacing::isPaced boundaries", "[gui][tracemath][pacing]")
{
    using pacing::isPaced;

    static_assert (pacing::minPlotAreaPx == 2.0e6);
    static_assert (pacing::minIntervalMs == 30.0);

    CHECK_FALSE (isPaced (1.99999e6, 6));
    CHECK_FALSE (isPaced (2.0e6, 6));        // strictly greater
    CHECK (isPaced (2.000001e6, 6));
    CHECK (isPaced (2.0e6 + 1.0, 2));
    CHECK_FALSE (isPaced (0.0, 0));
    CHECK_FALSE (isPaced (1280.0 * 720.0, 5));
    CHECK_FALSE (isPaced (1600.0 * 1000.0, 6));  // the 1920 x 1080 window's data rect is about 1.6 Mpx
    CHECK (isPaced (2560.0 * 1440.0, 2));
    CHECK (isPaced (1280.0 * 720.0 * 4.0, 2));   // 1280 x 720 at 200 %
}

TEST_CASE ("pacing::isPaced: seven layers pace at any size", "[gui][tracemath][pacing]")
{
    using pacing::isPaced;
    CHECK (isPaced (1.0, 7));
    CHECK (isPaced (0.0, 7));
    CHECK (isPaced (1.0, 8));
    CHECK_FALSE (isPaced (1.0, 6));

    for (bool dual : { false, true })
        for (bool sec : { false, true })
            for (bool sc : { false, true })
            {
                const bool all = dual && sec && sc;
                CHECK (isPaced (100.0, pacing::countLayers (dual, sec, sc)) == all);
            }
}
