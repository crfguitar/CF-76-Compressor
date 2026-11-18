#include "Skin.h"
#include "PanelLighting.h"

using namespace juce;

namespace
{
    PixelRGB gridSample (const LightingGrid& g, float x, float y)
    {
        // Bilinear interpolation between cell centres.
        auto fx = jlimit (0.0f, (float) g.w - 1.0f, (x - (float) g.x0) / (float) g.cell - 0.5f);
        auto fy = jlimit (0.0f, (float) g.h - 1.0f, (y - (float) g.y0) / (float) g.cell - 0.5f);
        auto ix = jmin ((int) fx, g.w - 2), iy = jmin ((int) fy, g.h - 2);
        auto tx = fx - (float) ix, ty = fy - (float) iy;

        uint8 out[3];
        for (int c = 0; c < 3; ++c)
        {
            auto at = [&] (int i, int j) { return (float) g.rgb[(size_t) ((j * g.w + i) * 3 + c)]; };
            auto top = at (ix, iy) + (at (ix + 1, iy) - at (ix, iy)) * tx;
            auto bot = at (ix, iy + 1) + (at (ix + 1, iy + 1) - at (ix, iy + 1)) * tx;
            out[c] = (uint8) jlimit (0, 255, roundToInt (top + (bot - top) * ty));
        }
        PixelRGB p;
        p.setARGB (255, out[0], out[1], out[2]);
        return p;
    }

    // Anisotropic noise: smooth along x (long brush strokes), nearly independent per row.
    struct BrushNoise
    {
        BrushNoise (int w, int h, float amount, int64 seed) : width (w), data ((size_t) (w * h))
        {
            Random rng (seed);
            const int knot = 48;
            std::vector<float> row ((size_t) (w / knot + 2));
            for (int y = 0; y < h; ++y)
            {
                const float rowAmp = amount * (0.4f + 0.6f * rng.nextFloat());
                const float rowBias = (rng.nextFloat() - 0.5f) * amount;
                for (auto& v : row) v = (rng.nextFloat() - 0.5f) * 2.0f * rowAmp;
                for (int x = 0; x < w; ++x)
                {
                    auto k = x / knot;
                    auto t = (float) (x % knot) / (float) knot;
                    t = t * t * (3.0f - 2.0f * t);
                    data[(size_t) (y * w + x)] = rowBias + row[(size_t) k] + (row[(size_t) k + 1] - row[(size_t) k]) * t;
                }
            }
            // light vertical smoothing so streaks are 2-3px tall
            for (int y = h - 1; y > 0; --y)
                for (int x = 0; x < w; ++x)
                    data[(size_t) (y * w + x)] = 0.6f * data[(size_t) (y * w + x)] + 0.4f * data[(size_t) ((y - 1) * w + x)];
        }

        float at (int x, int y) const { return data[(size_t) (y * width + x)]; }

        int width;
        std::vector<float> data;
    };

    PixelRGB addNoise (PixelRGB p, float n)
    {
        auto f = [n] (uint8 c) { return (uint8) jlimit (0, 255, roundToInt ((float) c + n)); };
        p.setARGB (255, f (p.getRed()), f (p.getGreen()), f (p.getBlue()));
        return p;
    }

    PixelRGB profileSample (const RowProfile& p, int y)
    {
        auto i = (size_t) ((y - p.y0) * 3);
        PixelRGB px;
        px.setARGB (255, p.rgb[i], p.rgb[i + 1], p.rgb[i + 2]);
        return px;
    }

    void fillRegion (Image::BitmapData& bd, const BrushNoise& noise, Rectangle<int> area,
                     const LightingGrid& grid, const RowProfile& top, const RowProfile& bottom)
    {
        for (int y = area.getY(); y < area.getBottom(); ++y)
        {
            for (int x = area.getX(); x < area.getRight(); ++x)
            {
                PixelRGB p;
                if (y < grid.y0)
                {
                    // Bevel rows: exact measured profile, shifted by the panel's horizontal lighting.
                    auto base = profileSample (top, y);
                    auto edge = gridSample (grid, (float) x, (float) grid.y0);
                    auto mid  = gridSample (grid, (float) Ref::width * 0.5f, (float) grid.y0);
                    auto fade = jlimit (0.0f, 1.0f, (float) (y - top.y0 - 30) / 10.0f);
                    p = addNoise (base, fade * (float) (edge.getGreen() - mid.getGreen()));
                }
                else if (y >= grid.y1)
                {
                    auto base = profileSample (bottom, y);
                    auto edge = gridSample (grid, (float) x, (float) grid.y1);
                    auto mid  = gridSample (grid, (float) Ref::width * 0.5f, (float) grid.y1);
                    auto fade = jlimit (0.0f, 1.0f, 1.0f - (float) (y - grid.y1) / 20.0f);
                    p = addNoise (base, fade * (float) (edge.getGreen() - mid.getGreen()));
                }
                else
                {
                    p = gridSample (grid, (float) x, (float) y);
                }

                if (y >= 60 && y < 700)
                    p = addNoise (p, noise.at (x, y));

                *(PixelRGB*) bd.getPixelPointer (x, y) = p;
            }
        }
    }

    Image renderPanel (Revision rev)
    {
        Image img (Image::RGB, Ref::width, Ref::height, true);
        {
            Image::BitmapData bd (img, Image::BitmapData::writeOnly);
            const bool blacky = rev == Revision::blacky;
            BrushNoise noise (Ref::width, Ref::height, blacky ? 4.0f : 2.5f, 76);

            if (blacky)
            {
                fillRegion (bd, noise, { 0, 56, Ref::width, Ref::height - 56 },
                            Lighting::blackyPanel, Lighting::blackyTop, Lighting::blackyBottom);
            }
            else
            {
                fillRegion (bd, noise, { 0, 56, Ref::width, Ref::height - 56 },
                            Lighting::blueyPanel, Lighting::blueyTop, Lighting::blueyBottom);
                const auto& s = Lighting::blueySection;
                fillRegion (bd, noise, { s.x0, 56, s.x1 - s.x0, 705 - 56 },
                            s, Lighting::sectionTop, Lighting::sectionBottom);
            }
        }

        Graphics g (img);

        if (rev == Revision::bluey)
        {
            // Seams around the blue meter plate (measured): black gap + edge highlight on the left,
            // edge highlight + dark gap + shadow on the silver to the right.
            const float top = 58.0f, h = 705.0f - 58.0f;
            g.setColour (Colour (0xff050505));
            g.fillRect (1519.5f, top, 3.0f, h);
            g.setGradientFill (ColourGradient (Colour (0xffc1d7e2), 1523.0f, 0.0f, Colour (0x00c1d7e2), 1527.5f, 0.0f, false));
            g.fillRect (1522.5f, top, 5.0f, h);
            g.setGradientFill (ColourGradient (Colour (0x00668ca0), 2130.0f, 0.0f, Colour (0xb0668ca0), 2134.0f, 0.0f, false));
            g.fillRect (2130.0f, top, 5.0f, h);
            g.setColour (Colour (0xff040404));
            g.fillRect (2135.5f, top, 6.0f, h);
            g.setColour (Colour (0xff5f615f));
            g.fillRect (2141.5f, top, 6.0f, h);
        }

        // Toolbar strip placeholder (the real toolbar is drawn by its own component).
        g.setColour (Colours::black);
        g.fillRect (0, 0, Ref::width, 56);
        return img;
    }
}

Image Skin::panelImage (Cache& cache, Revision rev)
{
    auto& img = cache.panel[(int) rev];
    if (! img.isValid())
        img = renderPanel (rev);
    return img;
}

Font Skin::labelFont (float heightPx)
{
    return Font (FontOptions ("Arial", heightPx, Font::plain));
}

void Skin::drawLabel (Graphics& g, const String& text, Point<float> centre, float heightPx, Colour colour)
{
    GlyphArrangement glyphs;
    glyphs.addLineOfText (labelFont (heightPx), text, 0.0f, 0.0f);
    Path path;
    glyphs.createPath (path);

    // Centre on the glyph ink box, which is what the reference measurements describe.
    auto ink = path.getBounds();
    path.applyTransform (AffineTransform::translation (centre - ink.getCentre()));

    g.setColour (colour);
    g.fillPath (path);
    g.strokePath (path, PathStrokeType (heightPx * 0.04f));
}

namespace
{
    Path glyphPath (const String& text, float heightPx, const String& typeface = "Arial")
    {
        GlyphArrangement glyphs;
        glyphs.addLineOfText (Font (FontOptions (typeface, heightPx, Font::plain)), text, 0.0f, 0.0f);
        Path path;
        glyphs.createPath (path);
        return path;
    }
}

void Skin::drawLabelAligned (Graphics& g, const String& text, float edgeX, float centreY,
                             float heightPx, Colour colour, bool rightAligned)
{
    auto path = glyphPath (text, heightPx);
    auto ink = path.getBounds();
    auto dx = rightAligned ? edgeX - ink.getRight() : edgeX - ink.getX();
    path.applyTransform (AffineTransform::translation (dx, centreY - ink.getCentreY()));
    g.setColour (colour);
    g.fillPath (path);
    g.strokePath (path, PathStrokeType (heightPx * 0.04f));
}

float Skin::valueNoise (float x, float y, int seed)
{
    auto hash = [seed] (int ix, int iy)
    {
        auto h = (uint32) (ix * 374761393 + iy * 668265263 + seed * 1442695041);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
    };
    auto ix = (int) std::floor (x), iy = (int) std::floor (y);
    auto fx = x - (float) ix, fy = y - (float) iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    auto a = hash (ix, iy), b = hash (ix + 1, iy), c = hash (ix, iy + 1), d = hash (ix + 1, iy + 1);
    return jmap (fy, jmap (fx, a, b), jmap (fx, c, d));
}

void Skin::drawLamp (Graphics& g, bool lit)
{
    const auto c = lampCentre;
    auto disc = [&c] (float r) { return Rectangle<float> (2 * r, 2 * r).withCentre (c); };

    Path outer;
    outer.addEllipse (disc (38.0f));
    DropShadow (Colours::black.withAlpha (0.85f), 12, { 3, 5 }).drawForPath (g, outer);

    // Chrome bezel ring: bright outer lip, darker inner step.
    g.setColour (Colour::greyLevel (0.05f));
    g.fillEllipse (disc (38.5f));
    g.setGradientFill (ColourGradient (Colour::greyLevel (0.88f), c.x - 25.0f, c.y - 30.0f,
                                       Colour::greyLevel (0.30f), c.x + 25.0f, c.y + 30.0f, false));
    g.fillEllipse (disc (36.5f));
    g.setGradientFill (ColourGradient (Colour::greyLevel (0.52f), c.x - 20.0f, c.y - 20.0f,
                                       Colour::greyLevel (0.36f), c.x + 20.0f, c.y + 20.0f, false));
    g.fillEllipse (disc (33.5f));
    g.setColour (Colour (0xff6f7a62).withAlpha (0.6f)); // faint greenish tint seen on the ring
    g.drawEllipse (disc (29.0f), 1.5f);
    g.setColour (Colour::greyLevel (0.04f));
    g.fillEllipse (disc (24.0f));

    // Smoked glass dome with a specular spot upper-left; glows red while an over is latched.
    if (lit)
    {
        g.setGradientFill (ColourGradient (Colour (0xffff5a3c), c.x - 4.0f, c.y - 4.0f,
                                           Colour (0xff6a0c06), c.x + 18.0f, c.y + 18.0f, true));
        g.fillEllipse (disc (22.0f));
        g.setGradientFill (ColourGradient (Colour (0x60ff3020), c.x, c.y, Colour (0x00ff3020), c.x + 40.0f, c.y, true));
        g.fillEllipse (disc (40.0f));
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff7d6f6f), c.x - 6.0f, c.y - 6.0f,
                                           Colour (0xff1c1414), c.x + 16.0f, c.y + 18.0f, true));
        g.fillEllipse (disc (22.0f));
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), c.x - 8.0f, c.y - 10.0f,
                                       Colours::white.withAlpha (0.0f), c.x - 1.0f, c.y - 3.0f, true));
    g.fillEllipse (Rectangle<float> (14.0f, 12.0f).withCentre ({ c.x - 8.0f, c.y - 10.0f }));
}

void Skin::drawLogo (Graphics& g, Cache& cache)
{
    // Heavy condensed "CF-76" in the style of the original badge: glyph heights and spacing follow
    // the measured CLA-76 lettering, re-centred on the same spot (x 1828).
    struct GlyphBox { const char* ch; Rectangle<float> box; };
    const GlyphBox glyphs[] = {
        { "C", { 1726.0f, 611.0f, 44.0f, 68.0f } },
        { "F", { 1779.0f, 612.0f, 33.0f, 65.0f } },
        { "-", { 1817.0f, 645.0f, 24.0f, 11.0f } },
        { "7", { 1846.0f, 612.0f, 35.0f, 65.0f } },
        { "6", { 1886.0f, 611.0f, 43.0f, 68.0f } },
    };

    Path logo;
    for (auto& gb : glyphs)
    {
        auto p = glyphPath (gb.ch, 100.0f, "Impact");
        p.applyTransform (p.getTransformToScaleToFit (gb.box, false));
        logo.addPath (p);
    }

    if (! cache.marble.isValid()) cache.marble = []
    {
        Image img (Image::RGB, 260, 80, false);
        {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly); // scoped: commits on destruction
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                auto fx = (float) x, fy = (float) y;
                auto turb = valueNoise (fx / 22.0f, fy / 22.0f, 3) * 0.6f + valueNoise (fx / 9.0f, fy / 9.0f, 5) * 0.4f;
                auto vein = std::pow (1.0f - std::abs (std::sin ((fx * 0.035f + fy * 0.06f + turb * 5.0f) * 3.14159f)), 9.0f);
                auto speck = valueNoise (fx / 3.0f, fy / 3.0f, 9);
                auto v = 250.0f - 60.0f * vein - 10.0f * speck;
                PixelRGB p;
                p.setARGB (255, (uint8) v, (uint8) v, (uint8) jmin (255.0f, v + 2.0f));
                *(PixelRGB*) bd.getPixelPointer (x, y) = p;
            }
        }
        return img;
    }();

    DropShadow (Colours::black.withAlpha (0.6f), 6, { 2, 3 }).drawForPath (g, logo);
    g.setColour (Colour (0xfff7f7f7));
    g.fillPath (logo);

    Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (logo);
    g.setOpacity (0.85f);
    g.drawImageAt (cache.marble, 1700, 605);
}
