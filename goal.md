# Project goal

Build a production-quality VST3 compressor in C++ with JUCE and CMake, modelled on the classic 1176 FET limiter and tested in FL Studio.

## Objectives

- **Faithful sound.** Model the hardware's feedback topology, attack/release ballistics, ratio behaviour, "All buttons" mode and distortion character. Check every figure against published hardware specifications with automated tests rather than by ear alone.
- **Polished, authentic UI.** Recreate the look of vintage studio hardware, with brushed-metal panels, a VU meter and illuminated buttons. Draw it entirely in code so it scales cleanly at any size.
- **Two hardware revisions.** Model an early silverface ("Bluey") and a later blackface LN ("Blacky"), each with its own voicing and skin, switchable live.
- **A plug-in ready for real sessions.** Include presets, A/B comparison and undo/redo, report latency correctly, use little CPU, and stay stable when a DAW loads and unloads the plug-in.

## Approach

Use published hardware documentation as the specification, and use pixel-level comparison tooling to guide the UI work. Back each feature with a repeatable test harness: DSP measurements, a state/preset self-test, and a VST3 host lifecycle test.
