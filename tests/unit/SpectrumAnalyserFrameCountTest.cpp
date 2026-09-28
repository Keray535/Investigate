// T-004 acceptance criterion 7: for N input samples, fftSize F and hop H,
// frames produced == floor((N - F) / H) + 1 for N >= F, else 0, +-1, for
// overlaps 1/2/4/8.
//
// Also checks the design log's explicit invariant: a single large
// pushBlock() call produces the same frame count (and the same resulting
// power spectrum) as the same samples split across many small pushBlock()
// calls.

#include "SpectrumTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    int64_t expectedFrameCount (int64_t n, int fftSize, int hop)
    {
        if (n < fftSize)
            return 0;
        return (n - fftSize) / hop + 1; // integer division == floor for non-negative operands
    }
}

TEST_CASE ("Frame count matches floor((N-F)/H)+1 for all supported overlaps", "[dsp][spectrum][framing][T-004]")
{
    const int overlap = GENERATE (1, 2, 4, 8);
    CAPTURE (overlap);

    const int fftSize = 4096;
    const int hop = fftSize / overlap;

    SpectrumAnalyser::Settings s;
    s.sampleRate = 48000.0;
    s.fftSize = fftSize;
    s.overlap = overlap;
    s.window = WindowFunctions::Type::hann;
    s.channelMode = ChannelMode::mid;
    s.avgTimeMs = 0.0f;
    s.numMainChannels = 1;
    s.numSidechainChannels = 0;

    const int64_t n = GENERATE_COPY (0, 1, fftSize - 1, fftSize, fftSize + hop - 1, fftSize + hop, fftSize + 5 * hop);
    CAPTURE (n);

    SpectrumAnalyser analyser;
    analyser.prepare (s);

    auto samples = makeWhiteNoise (0.1, static_cast<int> (n), 7);
    pushMono (analyser, samples);

    const int64_t expected = expectedFrameCount (n, fftSize, hop);
    REQUIRE (std::abs (analyser.getFramesProduced() - expected) <= 1);
}

TEST_CASE ("Frame count and resulting spectrum are identical regardless of pushBlock chunking", "[dsp][spectrum][framing][T-004]")
{
    const int fftSize = 4096;
    const int overlap = 4;
    const int hop = fftSize / overlap;
    const int64_t n = fftSize + 5 * hop;

    SpectrumAnalyser::Settings s;
    s.sampleRate = 48000.0;
    s.fftSize = fftSize;
    s.overlap = overlap;
    s.window = WindowFunctions::Type::hann;
    s.channelMode = ChannelMode::mid;
    s.avgTimeMs = 0.0f;
    s.numMainChannels = 1;
    s.numSidechainChannels = 0;

    auto samples = makeWhiteNoise (0.2, static_cast<int> (n), 11);

    SpectrumAnalyser wholeBlock;
    wholeBlock.prepare (s);
    pushMono (wholeBlock, samples); // one call

    const int chunk = GENERATE (1, 17, 64, 512);
    CAPTURE (chunk);

    SpectrumAnalyser chunked;
    chunked.prepare (s);
    pushMono (chunked, samples, chunk); // many calls

    REQUIRE (wholeBlock.getFramesProduced() == chunked.getFramesProduced());
    REQUIRE (wholeBlock.getFramesProduced() > 1);

    const float* pa = wholeBlock.getPrimaryPower();
    const float* pb = chunked.getPrimaryPower();
    for (int k = 0; k < wholeBlock.getNumBins(); ++k)
        REQUIRE (pa[k] == pb[k]); // bit-exact: same samples, same frame boundaries, no floating-point-order differences expected
}
