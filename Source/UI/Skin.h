#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters.h"

// All UI geometry is expressed in "reference pixels": the coordinate space of
// reference/ui_*_2396.png. The editor shows the canvas at half scale by default.
namespace Ref
{
    inline constexpr int width  = 2396;
    inline constexpr int height = 865;
}

namespace Skin
{
    inline const juce::Colour labelText  { 0xffe6e6e6 };
    inline const juce::Colour activeBlue { 0xff2a6cf0 };
    inline const juce::Colour dotWhite   { 0xfff4f4f4 };
    inline const juce::Colour blueyInk   { 0xff161616 }; // panel lettering on the silver Bluey panel

    // Generated images, shared by all open editors via juce::SharedResourcePointer<Skin::Cache>.
    // Never keep juce::Image in a static: it would outlive JUCE's (Direct2D) graphics shutdown when
    // the host unloads the plug-in, hanging/crashing the host when a slot is deleted.
    struct Cache
    {
        juce::Image panel[2];
        juce::Image marble;
    };

    // Panel background (brushed metal, bevels, bottom bar) for the given revision, rendered once per cache.
    juce::Image panelImage (Cache&, Revision);

    juce::Font labelFont (float heightPx);

    // Smooth 2D value noise in [0, 1] (stains, marbling).
    float valueNoise (float x, float y, int seed);

    // Panel lettering: Arial, slightly emboldened to match the reference's heavier rendering.
    void drawLabel (juce::Graphics&, const juce::String& text, juce::Point<float> centre,
                    float heightPx, juce::Colour);

    // As drawLabel, but the ink box's right edge (rightAligned) or left edge sits on edgeX.
    void drawLabelAligned (juce::Graphics&, const juce::String& text, float edgeX, float centreY,
                           float heightPx, juce::Colour, bool rightAligned);

    // Decorations drawn straight onto the panel: indicator lamp and the CLA-76 logo.
    inline const juce::Point<float> lampCentre { 1834.5f, 167.0f };
    void drawLamp (juce::Graphics&, bool lit);
    void drawLogo (juce::Graphics&, Cache&);
}
