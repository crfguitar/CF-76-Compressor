#include "Knob.h"

using namespace juce;

namespace
{
    Point<float> polar (Point<float> c, float r, float angleRad)
    {
        return { c.x + r * std::sin (angleRad), c.y - r * std::cos (angleRad) };
    }

    std::vector<float> evenAngles (float from, float to, int count)
    {
        std::vector<float> a;
        for (int i = 0; i < count; ++i)
            a.push_back (from + (to - from) * (float) i / (float) (count - 1));
        return a;
    }
}

//==============================================================================
// Specs (all numbers measured from reference/ui_blacky_2396.png)

KnobSpec KnobSpec::bigInputOutput()
{
    KnobSpec s;
    s.body = Body::chickenHead;
    s.startDeg = -160.0f;
    s.endDeg = 160.0f;
    s.dotRing = 144.7f;
    s.dotDiameter = 8.0f;
    s.dotAngles = evenAngles (-160.0f, 180.0f, 18); // every 20 deg, including straight down
    s.labels = { { String (CharPointer_UTF8 ("\xe2\x88\x9e")), -56.8f, 168.0f, 0 },
                 { "48", -153.3f, 89.0f, 2 },
                 { "36", -171.8f, -25.0f, 4 },
                 { "30", -118.8f, -127.0f, 6 },
                 { "24", 0.7f, -165.5f, 8 },
                 { "18", 112.2f, -127.0f, 10 },
                 { "12", 170.2f, -25.5f, 12 },
                 { "6", 143.2f, 88.5f, 14 },
                 { "0", 57.0f, 165.0f, 16 } };
    s.labelFontPx = 30.7f;
    s.extent = 200.0f;
    return s;
}

KnobSpec KnobSpec::attackRelease()
{
    KnobSpec s;
    s.body = Body::small;
    const float step = 360.0f / 7.0f;
    s.startDeg = -3.0f * step;
    s.endDeg = 3.0f * step;
    s.dotRing = 83.0f;
    s.dotDiameter = 7.5f;
    s.dotAngles = evenAngles (s.startDeg, s.endDeg, 7);
    s.labels = { { "1", -60.1f, 89.0f, 0 },
                 { "3", -77.1f, -66.5f, 2 },
                 { "5", 90.4f, -66.0f, 4 },
                 { "7", 72.4f, 89.0f, 6 } };
    s.labelFontPx = 30.7f;
    s.extent = 112.0f;
    return s;
}

KnobSpec KnobSpec::mixTrim (String minText, String maxText)
{
    KnobSpec s;
    s.body = Body::tiny;
    s.startDeg = -150.0f;
    s.endDeg = 150.0f;
    s.dotRing = 57.7f;
    s.dotDiameter = 3.6f;
    s.dotAngles = evenAngles (-150.0f, 150.0f, 11);
    const bool trim = minText.startsWith ("-");
    s.labels = { { minText, trim ? -44.4f : -49.75f, 51.8f, -1 },
                 { maxText, trim ? 65.1f : 63.25f, 51.8f, -1 } };
    s.labelFontPx = 19.5f;
    s.highlightNearest = false;
    s.extent = 80.0f;
    return s;
}

//==============================================================================

Knob::Knob (KnobSpec s) : spec (std::move (s))
{
    setSliderStyle (RotaryVerticalDrag);
    setTextBoxStyle (NoTextBox, true, 0, 0);
    setMouseDragSensitivity (300);
    setVelocityBasedMode (false);
    setRotaryParameters (degreesToRadians (spec.startDeg) + MathConstants<float>::twoPi,
                         degreesToRadians (spec.endDeg) + MathConstants<float>::twoPi, true);
    setBufferedToImage (false);
}

void Knob::setCentre (float cx, float cy)
{
    // Component bounds are integers, so keep the sub-pixel remainder for painting.
    auto e = spec.extent;
    setBounds (Rectangle<float> (cx - e, cy - e, 2 * e, 2 * e).getSmallestIntegerContainer());
    getProperties().set ("cx", cx - (float) getX());
    getProperties().set ("cy", cy - (float) getY());
}

void Knob::setInk (Colour c)
{
    if (c != ink) { ink = c; repaint(); }
}

float Knob::pointerAngleDeg() const
{
    auto t = (float) const_cast<Knob*> (this)->valueToProportionOfLength (getValue());
    return spec.startDeg + (spec.endDeg - spec.startDeg) * t;
}

int Knob::activeDot() const
{
    if (! spec.highlightNearest)
        return -1;
    auto a = pointerAngleDeg();
    int best = 0;
    for (int i = 1; i < (int) spec.dotAngles.size(); ++i)
        if (std::abs (spec.dotAngles[(size_t) i] - a) < std::abs (spec.dotAngles[(size_t) best] - a))
            best = i;
    return best;
}

bool Knob::hitTest (int x, int y)
{
    Point<float> c ((float) getProperties()["cx"], (float) getProperties()["cy"]);
    return c.getDistanceFrom ({ (float) x, (float) y }) < spec.dotRing;
}

void Knob::paint (Graphics& g)
{
    Point<float> c ((float) getProperties()["cx"], (float) getProperties()["cy"]);
    auto angle = degreesToRadians (pointerAngleDeg());

    drawScale (g, c, activeDot());

    switch (spec.body)
    {
        case KnobSpec::Body::chickenHead: drawChickenHead (g, c, angle); break;
        case KnobSpec::Body::small:       drawSmall (g, c.translated (2.0f, 2.0f), angle, 66.0f); break;
        case KnobSpec::Body::tiny:        drawSmall (g, c, angle, 46.0f); break;
    }
}

void Knob::drawScale (Graphics& g, Point<float> c, int lit) const
{
    for (int i = 0; i < (int) spec.dotAngles.size(); ++i)
    {
        auto d = spec.dotDiameter;
        if (spec.body == KnobSpec::Body::tiny && i == (int) spec.dotAngles.size() / 2)
            d = 6.8f; // centre detent dot is larger on MIX/TRIM
        auto p = polar (c, spec.dotRing, degreesToRadians (spec.dotAngles[(size_t) i]));
        g.setColour (i == lit ? Skin::activeBlue : ink.withAlpha (spec.body == KnobSpec::Body::tiny ? 0.7f : 1.0f));
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }

    for (auto& l : spec.labels)
        Skin::drawLabel (g, l.text, { c.x + l.dx, c.y + l.dy }, spec.labelFontPx,
                         l.dotIndex >= 0 && l.dotIndex == lit ? Skin::activeBlue : ink);
}

namespace
{
    // Brightness of a surface whose outward normal points along `normalRad` (0 = up),
    // lit from the upper left. Returns 0..1.
    float lambert (float normalRad)
    {
        const float lightRad = -MathConstants<float>::pi * 0.25f;
        return 0.5f + 0.5f * std::cos (normalRad - lightRad);
    }

    Colour grey (float level) { return Colour::greyLevel (jlimit (0.0f, 1.0f, level)); }
}

void Knob::drawChickenHead (Graphics& g, Point<float> c, float angle) const
{
    const float skirt = 121.0f, dome = 93.0f, boss = 54.0f;

    // Drop shadow cast to the lower right (dims the scale dots at ~140-160 deg).
    {
        Path shadow;
        shadow.addEllipse (Rectangle<float> (2 * skirt, 2 * skirt).withCentre (c.translated (14.0f, 18.0f)));
        DropShadow (Colours::black.withAlpha (0.9f), 30, {}).drawForPath (g, shadow);
    }

    // Skirt: brushed conical ring, lit from the upper left.
    g.setGradientFill (ColourGradient (grey (0.36f), c.x - skirt * 0.7f, c.y - skirt * 0.7f,
                                       grey (0.06f), c.x + skirt * 0.7f, c.y + skirt * 0.7f, false));
    g.fillEllipse (Rectangle<float> (2 * skirt, 2 * skirt).withCentre (c));
    for (float r = dome + 3.0f; r < skirt - 2.0f; r += 2.5f) // concentric machining marks
    {
        g.setColour (Colours::white.withAlpha (0.025f + 0.02f * std::sin (r * 1.7f)));
        g.drawEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c), 1.0f);
    }
    g.setGradientFill (ColourGradient (grey (0.55f), c.x - skirt * 0.5f, c.y - skirt,
                                       Colours::transparentBlack, c.x + skirt * 0.3f, c.y + skirt * 0.2f, false));
    g.drawEllipse (Rectangle<float> (2 * skirt - 2.0f, 2 * skirt - 2.0f).withCentre (c), 2.0f);

    // Glossy black dome with a soft specular highlight to the left of the boss.
    {
        g.setGradientFill (ColourGradient (grey (0.11f), c.x - dome * 0.5f, c.y - dome * 0.6f,
                                           grey (0.0f), c.x + dome * 0.5f, c.y + dome * 0.7f, false));
        g.fillEllipse (Rectangle<float> (2 * dome, 2 * dome).withCentre (c));
        g.setColour (grey (0.0f).withAlpha (0.8f));
        g.drawEllipse (Rectangle<float> (2 * dome, 2 * dome).withCentre (c), 2.0f);

        Point<float> hl (c.x - 64.0f, c.y - 6.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), hl.x, hl.y,
                                           Colours::white.withAlpha (0.0f), hl.x + 20.0f, hl.y, true));
        g.fillEllipse (Rectangle<float> (40.0f, 80.0f).withCentre (hl));
    }

    // Flat boss.
    g.setGradientFill (ColourGradient (grey (0.21f), c.x, c.y - boss, grey (0.13f), c.x, c.y + boss, false));
    g.fillEllipse (Rectangle<float> (2 * boss, 2 * boss).withCentre (c));
    g.setColour (grey (0.04f));
    g.drawEllipse (Rectangle<float> (2 * boss, 2 * boss).withCentre (c), 2.0f);

    // Fin ("chicken head"): narrow at the pointing end, wide at the tail, two faces meeting at a ridge.
    {
        const float tipR = 126.0f, tailR = 130.0f, tipW = 30.0f, tailW = 76.0f;
        auto xf = AffineTransform::rotation (angle).translated (c);

        Path outline;
        outline.startNewSubPath (-tipW * 0.5f, -tipR + 8.0f);
        outline.quadraticTo (0.0f, -tipR - 4.0f, tipW * 0.5f, -tipR + 8.0f);
        outline.lineTo (tailW * 0.5f, tailR - 14.0f);
        outline.quadraticTo (0.0f, tailR + 4.0f, -tailW * 0.5f, tailR - 14.0f);
        outline.closeSubPath();
        outline.applyTransform (xf);

        DropShadow (Colours::black.withAlpha (0.85f), 16, { 7, 9 }).drawForPath (g, outline);

        // Each face slopes away from the ridge; shade it by its normal.
        auto face = [&] (float side)
        {
            Path p;
            p.startNewSubPath (0.0f, -tipR - 2.0f);
            p.lineTo (side * tipW * 0.5f, -tipR + 8.0f);
            p.lineTo (side * tailW * 0.5f, tailR - 14.0f);
            p.lineTo (0.0f, tailR + 2.0f);
            p.closeSubPath();
            p.applyTransform (xf);

            auto b = lambert (angle + side * MathConstants<float>::halfPi);
            auto top = polar (c, tipR, angle), bottom = polar (c, -tailR, angle);
            g.setGradientFill (ColourGradient (grey (0.08f + 0.30f * b), top.x, top.y,
                                               grey (0.03f + 0.16f * b), bottom.x, bottom.y, false));
            g.fillPath (p);
        };
        face (-1.0f);
        face (1.0f);

        g.setColour (Colours::white.withAlpha (0.18f));
        g.drawLine (Line<float> (polar (c, tipR - 4.0f, angle), polar (c, -tailR + 6.0f, angle)), 1.2f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.strokePath (outline, PathStrokeType (1.2f));
    }

    // White index line at the pointing end.
    g.setColour (Colour (0xfff2f2f2));
    g.drawLine (Line<float> (polar (c, 95.0f, angle), polar (c, 124.0f, angle)), 7.0f);
}

void Knob::drawSmall (Graphics& g, Point<float> c, float angle, float radius) const
{
    const bool tiny = spec.body == KnobSpec::Body::tiny;

    // Cylinder seen slightly from above: the base wall shows below the top face.
    Path base;
    base.addEllipse (Rectangle<float> (2 * radius, 2 * radius).withCentre (c.translated (0.0f, 3.0f)));
    DropShadow (Colours::black.withAlpha (0.85f), tiny ? 12 : 20, { 6, 9 }).drawForPath (g, base);
    g.setColour (grey (0.02f));
    g.fillPath (base);

    const float face = tiny ? radius * 0.82f : radius - 1.0f;
    auto fc = c.translated (0.0f, -1.0f);

    if (tiny)
    {
        g.setGradientFill (ColourGradient (grey (0.42f), c.x - radius * 0.6f, c.y - radius,
                                           grey (0.06f), c.x + radius * 0.4f, c.y + radius, false));
        g.fillEllipse (Rectangle<float> (2 * radius, 2 * radius).withCentre (c));
    }

    g.setGradientFill (ColourGradient (grey (0.16f), fc.x - face * 0.6f, fc.y - face * 0.7f,
                                       grey (0.01f), fc.x + face * 0.5f, fc.y + face * 0.8f, false));
    g.fillEllipse (Rectangle<float> (2 * face, 2 * face).withCentre (fc));

    // Raised bar across the face along the pointer axis.
    const float barW = tiny ? 30.0f : 44.0f;
    const float barFront = tiny ? radius + 18.0f : face, barBack = tiny ? radius * 0.85f : face;
    Path bar;
    bar.addRectangle (-barW * 0.5f, -barFront, barW, barFront + barBack);
    bar.applyTransform (AffineTransform::rotation (angle).translated (fc));

    Graphics::ScopedSaveState state (g);
    if (! tiny)
    {
        Path clip;
        clip.addEllipse (Rectangle<float> (2 * face, 2 * face).withCentre (fc));
        g.reduceClipRegion (clip);
    }
    DropShadow (Colours::black.withAlpha (0.9f), 10, { 4, 6 }).drawForPath (g, bar);
    g.setColour (grey (tiny ? 0.08f : 0.10f));
    g.fillPath (bar);

    // Highlight on the bar edge that faces the light.
    for (float side : { -1.0f, 1.0f })
    {
        auto b = lambert (angle + side * MathConstants<float>::halfPi);
        auto off = polar ({}, barW * 0.5f - 1.0f, angle + side * MathConstants<float>::halfPi);
        auto from = polar (fc, barFront, angle) + off, to = polar (fc, -barBack, angle) + off;
        g.setColour (Colours::white.withAlpha (0.45f * b * b));
        g.drawLine ({ from, to }, 1.5f);
    }

    if (tiny)
    {
        g.setColour (Colour (0xffe6dcae));
        g.drawLine (Line<float> (polar (fc, 5.0f, angle), polar (fc, 32.0f, angle)), 3.0f);
    }
    else
    {
        g.setColour (Colour (0xfff2f2f2));
        g.drawLine (Line<float> (polar (fc, 17.0f, angle), polar (fc, 58.0f, angle)), 7.0f);
    }
}
