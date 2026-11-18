#pragma once

#include <juce_core/juce_core.h>

// Offline measurements of the DSP on a fresh processor instance (standalone dev hook:
// CF76_DSPTEST=<report file>). Returns a human-readable report with PASS/FAIL lines.
juce::String runDspTests();
