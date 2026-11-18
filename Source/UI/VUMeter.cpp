#include "VUMeter.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace
{
    // All geometry in reference px (measured from reference/ui_blacky_2396.png).
    const Rectangle<float> bezel  { 1564.0f, 231.0f, 537.0f, 326.0f };
    const Rectangle<float> recess { 1579.0f, 242.0f, 507.0f, 302.0f };
    const Rectangle<float> face   { 1597.0f, 254.0f, 471.0f, 204.0f };
    const Rectangle<float> plate  { 1597.0f, 458.0f, 471.0f, 70.0f };
    const Point<float> arcCentre  { 1837.4f, 703.0f };   // centre of the printed scale arcs
    const Point<float> pivot      { 1837.4f, 549.5f };   // needle pivot (hidden behind the plate)
    const Point<float> screw      { 1834.0f, 492.5f };
    constexpr float redArcR = 364.0f, blackArcR = 375.0f;

    // Scale marks: VU value -> angle about arcCentre (degrees, 0 = up).
    struct Mark { float vu, deg; const char* text; float tx, ty; };
    const Mark marks[] = {
        { -20.0f, -26.2f, "-20", 1629.0f, 328.0f },
        { -10.0f, -20.0f, "-10", 1675.5f, 309.0f },
        {  -7.0f, -15.6f, "-7",  1714.5f, 299.0f },
        {  -5.0f, -11.2f, "-5",  1748.0f, 293.0f },
        {  -3.0f,  -4.0f, "-3",  1792.0f, 286.0f },
        {  -2.0f,  -1.25f, "-2", 1824.5f, 286.0f },
        {  -1.0f,   3.0f, "-1",  1862.0f, 287.5f },
        {   0.0f,   8.8f, "0",   1901.6f, 288.0f },
        {   1.0f,  13.3f, "1",   1939.0f, 294.0f },
        {   2.0f,  19.5f, "2",   1984.5f, 305.5f },
        {   3.0f,  26.2f, "3",   2033.5f, 328.0f },
    };

    const Colour faceCream  { 0xffc9bb7c };
    const Colour scaleBlack { 0xff1c1a14 };
    const Colour scaleRed   { 0xffc8281c };

    Point<float> onArc (float r, float deg)
    {
        auto t = degreesToRadians (deg);
        return { arcCentre.x + r * std::sin (t), arcCentre.y - r * std::cos (t) };
    }

    float vuToDeg (float vu)
    {
        if (vu <= marks[0].vu) return marks[0].deg + (vu - marks[0].vu) * 0.3f;
        for (size_t i = 0; i + 1 < std::size (marks); ++i)
            if (vu <= marks[i + 1].vu)
                return jmap (vu, marks[i].vu, marks[i + 1].vu, marks[i].deg, marks[i + 1].deg);
        return marks[std::size (marks) - 1].deg + (vu - 3.0f) * 2.0f;
    }

    Path arcPath (float r, float fromDeg, float toDeg)
    {
        Path p;
        p.addCentredArc (arcCentre.x, arcCentre.y, r, r, 0.0f, degreesToRadians (fromDeg), degreesToRadians (toDeg), true);
        return p;
    }

    Image renderFace()
    {
        // Face paper: cream with soft stains, shadowed by the bezel on top/left/right/bottom.
        auto w = (int) face.getWidth(), h = (int) face.getHeight();
        Image img (Image::RGB, w, h, false);
        // BitmapData must be destroyed before the image is used: on Direct2D images the
        // pixel writes are only committed when it goes out of scope.
        {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                float shade = 1.0f;
                shade *= jmap (jlimit (0.0f, 1.0f, (float) y / 30.0f), 0.74f, 1.0f);            // top band
                shade *= jmap (jlimit (0.0f, 1.0f, (float) x / 24.0f), 0.62f, 1.0f);            // left wall
                shade *= jmap (jlimit (0.0f, 1.0f, (float) (w - x) / 46.0f), 0.62f, 1.0f);      // right wall
                shade *= jmap (jlimit (0.0f, 1.0f, (float) (h - y) / 40.0f), 0.66f, 1.0f);      // bottom
                auto stain = 0.6f * Skin::valueNoise ((float) x / 40.0f, (float) y / 18.0f, 7)
                           + 0.4f * Skin::valueNoise ((float) x / 9.0f, (float) y / 6.0f, 11);
                shade *= 0.95f + 0.08f * stain;
                auto c = faceCream;
                auto px = PixelRGB();
                px.setARGB (255, (uint8) jlimit (0, 255, roundToInt (c.getRed() * shade)),
                                 (uint8) jlimit (0, 255, roundToInt (c.getGreen() * shade)),
                                 (uint8) jlimit (0, 255, roundToInt (c.getBlue() * shade)));
                *(PixelRGB*) bd.getPixelPointer (x, y) = px;
            }
        }
        }
        return img;
    }
}

VUMeter::VUMeter (CF76AudioProcessor& p) : proc (p)
{
    setBounds (bezel.expanded (24.0f).getSmallestIntegerContainer());
    setInterceptsMouseClicks (false, false);
    startTimerHz (60);
}

void VUMeter::timerCallback()
{
    // VU ballistics: ~300 ms to reach 99% => one-pole time constant of ~65 ms.
    const float dt = 1.0f / 60.0f, coeff = 1.0f - std::exp (-dt / 0.065f);
    auto mode = (MeterMode) (int) proc.apvts.getRawParameterValue (ParamIDs::meter)->load();

    float target;
    if (mode == MeterMode::gr)
    {
        target = proc.meterGrDb.load();
    }
    else
    {
        // 0 VU = -18 dBFS RMS (manual: -18 dBFS = +4 dBu = 0 VU).
        auto rmsDb = mode == MeterMode::in ? proc.meterInRmsDb.load() : proc.meterOutRmsDb.load();
        smoothedLin += coeff * (Decibels::decibelsToGain (rmsDb) - smoothedLin);
        target = Decibels::gainToDecibels (smoothedLin, -100.0f) + 18.0f;
    }

    auto next = mode == MeterMode::gr ? reading + coeff * (target - reading) : target;
    next = jlimit (-23.0f, 4.5f, next);
    if (std::abs (next - reading) > 0.005f)
    {
        reading = next;
        repaint();
    }
}

void VUMeter::paint (Graphics& g)
{
    g.addTransform (AffineTransform::translation (-getPosition().toFloat()));

    // Bezel with cast shadow.
    {
        Path p;
        p.addRoundedRectangle (bezel, 5.0f);
        DropShadow (Colours::black.withAlpha (0.8f), 22, { 6, 8 }).drawForPath (g, p);
    }
    g.setGradientFill (ColourGradient (Colour::greyLevel (0.24f), bezel.getX(), bezel.getY(),
                                       Colour::greyLevel (0.13f), bezel.getRight(), bezel.getBottom(), false));
    g.fillRoundedRectangle (bezel, 5.0f);
    g.setColour (Colour::greyLevel (0.16f));
    g.fillRect (bezel.reduced (4.0f));
    g.setColour (Colour::greyLevel (0.3f));
    g.drawRoundedRectangle (bezel.reduced (0.5f), 5.0f, 1.0f);

    // Brown recess walls.
    g.setColour (Colour (0xff221b0f));
    g.fillRect (recess);
    g.setGradientFill (ColourGradient (Colour (0xff2c2310), recess.getX(), recess.getY(),
                                       Colour (0xff483a2e), recess.getX(), recess.getBottom(), false));
    g.fillRect (recess.reduced (3.0f));

    // Face.
    if (! faceCache.isValid())
        faceCache = renderFace();
    g.drawImageAt (faceCache, (int) face.getX(), (int) face.getY());

    // Scale arcs, end chevrons and ticks.
    g.setColour (scaleBlack);
    g.strokePath (arcPath (blackArcR, -26.2f, 8.8f), PathStrokeType (2.6f));
    g.drawLine (Line<float> (onArc (374.0f, -26.2f), onArc (398.6f, -28.45f)), 2.6f);
    g.setColour (scaleRed);
    g.strokePath (arcPath (blackArcR, 8.8f, 26.2f), PathStrokeType (2.6f));
    g.strokePath (arcPath (redArcR, -27.05f, 24.8f), PathStrokeType (2.4f));
    g.drawLine (Line<float> (onArc (363.0f, -27.05f), onArc (395.7f, -29.9f)), 2.4f);
    g.drawLine (Line<float> (onArc (364.0f, 24.8f), onArc (398.3f, 27.3f)), 2.4f);

    for (auto& m : marks)
    {
        if (m.vu <= -20.0f || m.vu >= 3.0f || m.vu == 0.0f) continue;
        g.setColour (m.vu < 0.0f ? scaleBlack : scaleRed);
        g.drawLine (Line<float> (onArc (blackArcR, m.deg), onArc (blackArcR + 17.0f, m.deg)), 3.0f);
    }

    // Zero marker ring.
    {
        auto c = onArc (388.7f, 8.8f);
        g.setColour (Colour (0xff3a0e08));
        g.drawEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (c), 2.6f);
    }

    for (auto& m : marks)
        Skin::drawLabel (g, m.text, { m.tx, m.ty }, 26.5f, m.vu < 0.0f ? scaleBlack : scaleRed);

    // Needle: from the hidden pivot through the scale point, clipped to the face window.
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face.toNearestInt());
        auto tip = onArc (397.0f, vuToDeg (reading));
        auto dir = (tip - pivot) / tip.getDistanceFrom (pivot);
        g.setColour (Colours::black.withAlpha (0.18f));
        g.drawLine (Line<float> (pivot.translated (3.0f, 4.0f), tip.translated (3.0f, 4.0f)), 2.5f);
        g.setColour (Colour (0xffc0201a));
        g.drawLine (Line<float> (pivot, tip + dir * 2.0f), 2.6f);
    }

    // Glass glare near the top of the window.
    {
        Point<float> glare (face.getCentreX() - 80.0f, face.getY() + 14.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.2f), glare.x, glare.y,
                                           Colours::white.withAlpha (0.0f), glare.x + 120.0f, glare.y, true));
        g.fillEllipse (Rectangle<float> (240.0f, 40.0f).withCentre (glare));
    }

    // Lower plate with highlight edge and screw.
    g.setColour (Colour (0xff98958a));
    g.fillRect (plate.withHeight (3.0f));
    g.setColour (Colour::greyLevel (0.157f));
    g.fillRect (plate.withTrimmedTop (5.0f));
    g.setColour (Colour::greyLevel (0.22f));
    g.fillRect (plate.withY (plate.getBottom() - 3.0f).withHeight (2.0f));

    {
        auto r = Rectangle<float> (40.0f, 40.0f).withCentre (screw);
        Path head;
        head.addEllipse (r);
        DropShadow (Colours::black.withAlpha (0.8f), 8, { 2, 3 }).drawForPath (g, head);
        g.setGradientFill (ColourGradient (Colour::greyLevel (0.35f), r.getX(), r.getY(),
                                           Colour::greyLevel (0.02f), r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (Colour::greyLevel (0.0f));
        auto cross = [&] (float deg)
        {
            auto t = degreesToRadians (deg);
            auto d = Point<float> (std::cos (t), std::sin (t)) * 12.0f;
            g.drawLine (Line<float> (screw - d * 0.85f, screw + d * 0.85f), 3.5f);
        };
        cross (35.0f);
        cross (125.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawEllipse (r.reduced (1.5f), 1.0f);
    }
}
