#pragma once
#include <JuceHeader.h>

namespace nfdeesser
{
// Manuals ship embedded in the plug-in binary. On first open (or if the cached copy's size differs, e.g. after an
// update) they are written once to a per-user support folder and handed to the OS's default PDF viewer.
struct ManualManager
{
    static void openManual(const char* data, int size, const juce::String& filename);
};
}
