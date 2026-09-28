#include "TruePeakDetector.h"
#include "DecibelUtils.h"

#include <algorithm>
#include <cmath>

namespace investigate
{

namespace
{
    // ITU-R BS.1770-5 (11/2023), Annex 2, Attachment 1, "order 48, 4-phase,
    // FIR interpolating" filter table (PDF pp. 19-20). The 12 real taps per
    // phase were transcribed exactly as printed into
    // docs/research/T-007-true-peak-coefficients.md by researcher for this
    // task (not typed from memory, not copied from a third party's
    // reimplementation); reproduced here verbatim from that report. T-024:
    // this table is now exactly kItuTapsPerPhase (12) elements per phase -
    // no trailing zero padding - so process()'s 4x-tier inner loop can
    // never multiply a padding zero (see the class comment). kPhaseCoeffs
    // [p][k]: p = phase (0..3, approximating fractional sample offset p/4 -
    // phase 0 is the near-identity phase, real-tap sum ~1.0016), k = tap
    // index (0..11, top-to-bottom as printed).
    constexpr std::array<std::array<float, TruePeakDetector::kItuTapsPerPhase>, 4> kPhaseCoeffs { {
        // Phase 0
        { 0.0017089843750f,  0.0109863281250f, -0.0196533203125f,  0.0332031250000f,
         -0.0594482421875f,  0.1373291015625f,  0.9721679687500f, -0.1022949218750f,
          0.0476074218750f, -0.0266113281250f,  0.0148925781250f, -0.0083007812500f },
        // Phase 1
        { -0.0291748046875f,  0.0292968750000f, -0.0517578125000f,  0.0891113281250f,
          -0.1665039062500f,  0.4650878906250f,  0.7797851562500f, -0.2003173828125f,
           0.1015625000000f, -0.0582275390625f,  0.0330810546875f, -0.0189208984375f },
        // Phase 2
        { -0.0189208984375f,  0.0330810546875f, -0.0582275390625f,  0.1015625000000f,
          -0.2003173828125f,  0.7797851562500f,  0.4650878906250f, -0.1665039062500f,
           0.0891113281250f, -0.0517578125000f,  0.0292968750000f, -0.0291748046875f },
        // Phase 3
        { -0.0083007812500f,  0.0148925781250f, -0.0266113281250f,  0.0476074218750f,
          -0.1022949218750f,  0.9721679687500f,  0.1373291015625f, -0.0594482421875f,
           0.0332031250000f, -0.0196533203125f,  0.0109863281250f,  0.0017089843750f }
    } };

    // Independently derived Hann-windowed-sinc polyphase interpolator for
    // the 88.2/96 kHz tier (see the class comment "The 88.2/96 kHz filter
    // is not a subset of the ITU table" for the full reasoning: taps=49,
    // factor=2 - the same parametrisation libebur128 itself uses at this
    // sample-rate tier, chosen because an *odd* tap count with factor 2 is
    // structurally required to give phase 0 an exact identity response,
    // which is what correctly resolves fs/4-type inter-sample peaks).
    // h[j] = sinc((j-(taps-1)/2)/factor) * 0.5*(1-cos(2*pi*j/(taps-1))),
    // phase = j%2, tap index = j/2 - the standard fractional-delay-
    // interpolator formula (Crochiere & Rabiner, 1983), computed offline
    // (not at runtime, so no trig calls are needed on any real-time-
    // adjacent path) and transcribed here to full float precision.
    // kHannSincPhaseCoeffs[phase][tapIndex]: phase 0 is an exact identity
    // (a single 1.0 at index 12, the delay line's centre - see the class
    // comment); phase 1 is a 24-tap symmetric kernel, index 24 an exact
    // zero-pad (49 is odd, so factor-2 phase 1 only has 24 real taps).
    constexpr std::array<std::array<float, TruePeakDetector::kHannSincTapsPerPhase>, 2> kHannSincPhaseCoeffs { {
        // Phase 0: exact identity (delay by 12 samples).
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
          1.0f,
          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // Phase 1: 24-tap symmetric Hann-windowed-sinc kernel, index 24 zero-padded.
        { -0.0001183994f,  0.0011538046f, -0.0034619829f,  0.0073255944f,
          -0.0130998644f,  0.0212893930f, -0.0327143331f,  0.0489024229f,
          -0.0731549525f,  0.1141684195f, -0.2041299583f,  0.6338965872f,
           0.6338965872f, -0.2041299583f,  0.1141684195f, -0.0731549525f,
           0.0489024229f, -0.0327143331f,  0.0212893930f, -0.0130998644f,
           0.0073255944f, -0.0034619829f,  0.0011538046f, -0.0001183994f,
           0.0f }
    } };
}

void TruePeakDetector::prepare (double sampleRateIn, int maxBlockSizeIn, int numChannelsIn)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;
    maxBlockSize = std::max (1, maxBlockSizeIn);
    preparedChannels = std::clamp (numChannelsIn, 1, maxChannels);

    // See the class comment for the full reasoning: the literal ITU table
    // (4 phases) at <=48 kHz, the independently-derived Hann-sinc table (2
    // phases) at <=96 kHz, and no filtering above that. Small epsilons
    // absorb float comparison noise around the nominal 48000/96000 Hz
    // boundaries.
    if (sampleRate <= 48000.0 + 1.0)
    {
        oversampleFactor = 4;
        activePhaseCount = 4;
    }
    else if (sampleRate <= 96000.0 + 1.0)
    {
        oversampleFactor = 2;
        activePhaseCount = 2;
    }
    else
    {
        oversampleFactor = 1;
        activePhaseCount = 0;
    }

    subBlockSamples = std::max (1, static_cast<int> (std::llround (sampleRate * 0.01)));

    reset();
}

void TruePeakDetector::reset() noexcept
{
    subBlockSampleCounter = 0;

    for (auto& c : channels)
    {
        c.delay4x.fill (0.0f);
        c.delay2x.fill (0.0f);
        c.displayRing.reset();
        c.subBlockPeakLinear = 0.0f;
        c.truePeakMaxLinear = 0.0f;
        c.clipCount = 0;
    }
}

void TruePeakDetector::resetStatistics() noexcept
{
    for (auto& c : channels)
    {
        c.truePeakMaxLinear = 0.0f;
        c.clipCount = 0;
    }
}

void TruePeakDetector::process (const float* const* channelsIn, int numChannels, int numSamples) noexcept
{
    if (channelsIn == nullptr || numSamples <= 0 || preparedChannels <= 0)
        return;

    const int channelsToProcess = std::min (numChannels, preparedChannels);
    if (channelsToProcess <= 0)
        return;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int c = 0; c < channelsToProcess; ++c)
        {
            auto& st = channels[static_cast<size_t> (c)];
            const float x = channelsIn[c][n];

            // T-024: the two filtered tiers now have their own delay line
            // and computePhase loop (kItuTapsPerPhase=12 / kHannSincTapsPerPhase=25),
            // so each tier's inner loop only ever touches its own real taps -
            // no zero-padded coefficient is ever multiplied (see the class
            // comment "Polyphase structure, per-tier kernel length").
            float neighbourhoodPeak;
            if (oversampleFactor == 4)
            {
                st.push4x (x);
                neighbourhoodPeak = 0.0f;
                for (int p = 0; p < activePhaseCount; ++p)
                {
                    const float phaseOut = st.computePhase4x (kPhaseCoeffs[static_cast<size_t> (p)]);
                    neighbourhoodPeak = std::max (neighbourhoodPeak, std::abs (phaseOut));
                }
            }
            else if (oversampleFactor == 2)
            {
                st.push2x (x);
                neighbourhoodPeak = 0.0f;
                for (int p = 0; p < activePhaseCount; ++p)
                {
                    const float phaseOut = st.computePhase2x (kHannSincPhaseCoeffs[static_cast<size_t> (p)]);
                    neighbourhoodPeak = std::max (neighbourhoodPeak, std::abs (phaseOut));
                }
            }
            else
            {
                // Above 96 kHz: no interpolation, the true peak is the
                // sample peak (BS.1770's own rationale for very high
                // native rates).
                neighbourhoodPeak = std::abs (x);
            }

            st.subBlockPeakLinear = std::max (st.subBlockPeakLinear, neighbourhoodPeak);
            st.truePeakMaxLinear = std::max (st.truePeakMaxLinear, neighbourhoodPeak);

            // True-peak clip rule (documented in the class comment): once
            // per *input* sample, not once per oversampled point.
            if (neighbourhoodPeak >= 1.0f)
                ++st.clipCount;
        }

        if (++subBlockSampleCounter >= subBlockSamples)
        {
            subBlockSampleCounter = 0;
            onSubBlockComplete();
        }
    }
}

void TruePeakDetector::onSubBlockComplete() noexcept
{
    for (int c = 0; c < preparedChannels; ++c)
    {
        auto& st = channels[static_cast<size_t> (c)];
        st.displayRing.push (st.subBlockPeakLinear);
        st.subBlockPeakLinear = 0.0f;
    }
}

float TruePeakDetector::getTruePeakDb (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return Decibels::negativeInfinityDb();
    const auto& st = channels[static_cast<size_t> (channel)];
    return Decibels::linearToDb (st.displayRing.currentMax (st.subBlockPeakLinear));
}

float TruePeakDetector::getTruePeakMaxDb (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return Decibels::negativeInfinityDb();
    return Decibels::linearToDb (channels[static_cast<size_t> (channel)].truePeakMaxLinear);
}

uint64_t TruePeakDetector::getTruePeakClipCount (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return 0;
    return channels[static_cast<size_t> (channel)].clipCount;
}

} // namespace investigate
