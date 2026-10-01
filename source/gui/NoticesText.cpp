#include "NoticesText.h"

#include <BinaryData.h>

namespace investigate::NoticesText
{

namespace
{
    Block heading (const juce::String& t) { return { Block::Kind::heading, t }; }
    Block notice (const juce::String& t)  { return { Block::Kind::notice, t }; }

    Block licenseFrom (const char* data, int size, const char* fileName)
    {
        if (data == nullptr || size <= 0)
            return { Block::Kind::licenseText, juce::String ("License text missing: ") + fileName };

        return { Block::Kind::licenseText, juce::String::fromUTF8 (data, size) };
    }
}

std::vector<Block> build (const juce::String& juceVersion)
{
    std::vector<Block> blocks;

    blocks.push_back (heading ("Fonts"));
    blocks.push_back (notice ("Inter 4.1. Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter). "
                              "Licensed under the SIL Open Font License, Version 1.1."));
    blocks.push_back (notice ("JetBrains Mono 2.304. Copyright 2020 The JetBrains Mono Project Authors "
                              "(https://github.com/JetBrains/JetBrainsMono). "
                              "Licensed under the SIL Open Font License, Version 1.1."));
    blocks.push_back (notice ("The font software is not sold separately. The full license texts are below and in the "
                              "installed THIRD-PARTY-NOTICES.txt."));

    blocks.push_back (heading ("Software"));

    // The JUCE line is a placeholder until the M5 licence-tier decision (T-019).
    blocks.push_back (notice (juceVersion + ". Copyright (c) Raw Material Software Limited. "
                              "[AGPLv3 / commercial licence - pending the final licence decision]"));
    blocks.push_back (notice ("Steinberg VST 3 SDK 3.8.0 (MIT licence). Copyright (c) 2025 Steinberg Media Technologies GmbH. "
                              "VST is a registered trademark of Steinberg Media Technologies GmbH."));
    blocks.push_back (notice ("pffft (marton78 fork). A BSD-style license. The copyright notices, conditions and "
                              "disclaimer follow in full."));
    blocks.push_back (notice ("JUCE bundles zlib, libpng, the Independent JPEG Group library, HarfBuzz (MIT) and "
                              "SheenBidi (Apache-2.0). Their license texts are in the installed THIRD-PARTY-NOTICES.txt."));

    blocks.push_back (heading ("SIL Open Font License 1.1: Inter"));
    blocks.push_back (licenseFrom (BinaryData::OFLInter_txt, BinaryData::OFLInter_txtSize, "OFL-Inter.txt"));

    blocks.push_back (heading ("SIL Open Font License 1.1: JetBrains Mono"));
    blocks.push_back (licenseFrom (BinaryData::OFLJetBrainsMono_txt, BinaryData::OFLJetBrainsMono_txtSize,
                                   "OFL-JetBrainsMono.txt"));

    blocks.push_back (heading ("pffft license"));
    blocks.push_back (licenseFrom (BinaryData::LICENSEpffft_txt, BinaryData::LICENSEpffft_txtSize, "LICENSE-pffft.txt"));

    return blocks;
}

juce::String toPlainText (const std::vector<Block>& blocks)
{
    juce::String out;

    for (const auto& b : blocks)
        out << b.text << "\n";

    return out;
}

} // namespace investigate::NoticesText
