#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs for
// Firmament. See docs/architecture.md for the corresponding signal-flow
// diagram.
//
// FROZEN AS OF THE v0.1 PARAMETER LAYOUT:
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, and skew MAY still be refined during voicing/tuning milestones;
// only the IDs themselves are frozen.
namespace ParamIDs
{
    // Mid/Side width scale applied to the Side channel. 100% is an unmodified
    // pass-through of the encoded M/S signal (unity, decodes back to the
    // original L/R); 0% collapses the Side channel to silence (mono); up to
    // 200% doubles the Side channel's amplitude for an exaggerated image.
    inline constexpr auto width = "width";

    // Crossover frequency (Hz) for the optional bass-mono stage: Side-channel
    // content below this frequency is removed (forced to mono via the Mid
    // channel), Side content above it keeps the Width-scaled stereo image.
    // 0 Hz means the stage is fully bypassed (no forced mono at any
    // frequency).
    inline constexpr auto bassMonoFreq = "bassMonoFreq";

    // Output trim, applied after M/S decode back to L/R.
    inline constexpr auto output = "output";
}
