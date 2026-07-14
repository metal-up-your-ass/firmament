#pragma once

// Stateless Mid/Side encode/decode helpers, factored out of FirmamentEngine
// so the core M/S identity (encode then decode with an unscaled Side channel
// reconstructs the original L/R exactly, modulo floating-point rounding) can
// be exercised directly by unit tests (tests/MidSideCodecTests.cpp) without
// any DSP state or juce::AudioProcessor involved.
//
// Convention: M = (L + R) * 0.5, S = (L - R) * 0.5, so that decode is simply
// L = M + S, R = M - S - the standard "equal-power-free" (i.e. plain
// arithmetic mean/difference) M/S convention, which is what makes the mono
// sum (L + R == 2 * M) exactly independent of any scaling later applied to
// S (see FirmamentEngine's Width control).
namespace MidSideCodec
{
    struct MidSide
    {
        float mid = 0.0f;
        float side = 0.0f;
    };

    inline MidSide encode (float left, float right) noexcept
    {
        return { (left + right) * 0.5f, (left - right) * 0.5f };
    }

    struct LeftRight
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    inline LeftRight decode (float mid, float side) noexcept
    {
        return { mid + side, mid - side };
    }
}
