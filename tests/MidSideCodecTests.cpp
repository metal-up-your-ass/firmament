#include "dsp/MidSideCodec.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("MidSideCodec encode/decode round-trips exactly for arbitrary L/R", "[dsp][codec]")
{
    const float testValues[][2] = {
        { 0.5f, 0.3f },
        { 1.0f, -1.0f },
        { -0.75f, 0.75f },
        { 0.0f, 0.0f },
        { 0.2f, 0.2f }, // already-mono input
        { -0.9f, -0.1f },
    };

    for (const auto& pair : testValues)
    {
        const auto left = pair[0];
        const auto right = pair[1];

        const auto encoded = MidSideCodec::encode (left, right);
        const auto decoded = MidSideCodec::decode (encoded.mid, encoded.side);

        CHECK (decoded.left == Catch::Approx (left).margin (1e-6));
        CHECK (decoded.right == Catch::Approx (right).margin (1e-6));
    }
}

TEST_CASE ("MidSideCodec: Mid is the average, Side is the half-difference", "[dsp][codec]")
{
    const auto encoded = MidSideCodec::encode (1.0f, 0.2f);

    CHECK (encoded.mid == Catch::Approx (0.6f));
    CHECK (encoded.side == Catch::Approx (0.4f));
}

TEST_CASE ("MidSideCodec: zeroing Side collapses decode to mono (L == R == Mid)", "[dsp][codec]")
{
    const auto encoded = MidSideCodec::encode (0.8f, -0.3f);
    const auto decoded = MidSideCodec::decode (encoded.mid, 0.0f);

    CHECK (decoded.left == Catch::Approx (encoded.mid));
    CHECK (decoded.right == Catch::Approx (encoded.mid));
    CHECK (decoded.left == Catch::Approx (decoded.right));
}

TEST_CASE ("MidSideCodec: Mid alone reconstructs the mono downmix regardless of Side", "[dsp][codec]")
{
    // L + R == 2 * Mid always, no matter what Side is - this is the
    // invariant FirmamentEngine's width scaling relies on for mono
    // compatibility (see EngineTests.cpp's mono-sum test).
    const auto encoded = MidSideCodec::encode (0.6f, -0.4f);

    for (const float sideOverride : { 0.0f, encoded.side, encoded.side * 2.0f, -encoded.side })
    {
        const auto decoded = MidSideCodec::decode (encoded.mid, sideOverride);
        CHECK ((decoded.left + decoded.right) == Catch::Approx (2.0f * encoded.mid).margin (1e-6));
    }
}
