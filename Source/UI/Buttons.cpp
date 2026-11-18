#include "Buttons.h"

using namespace juce;

namespace
{
    // Pillow shading for a button face: lighter in the middle, falling off toward the edges.
    void pillow (Graphics& g, Rectangle<float> r, Colour edge, Colour centre)
    {
        g.setColour (edge);
        g.fillRoundedRectangle (r, 6.0f);

        ColourGradient v (edge, r.getCentreX(), r.getY(), edge, r.getCentreX(), r.getBottom(), false);
        v.addColour (0.5, centre);
        Graphics::ScopedSaveState s (g);
        g.setGradientFill (v);
        g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.12f, 2.0f), 6.0f);
    }

    // Mottled "frosted lamp" texture used by lit buttons: soft blotches rather than speckle.
    void mottle (Graphics& g, Rectangle<float> r, float amount)
    {
        for (float y = r.getY(); y < r.getBottom(); y += 2.0f)
            for (float x = r.getX(); x < r.getRight(); x += 2.0f)
            {
                auto n = Skin::valueNoise (x / 5.0f, y / 5.0f, 21) - 0.5f;
                g.setColour ((n > 0 ? Colours::white : Colours::black).withAlpha (amount * std::abs (n) * 2.0f));
                g.fillRect (x, y, 2.0f, 2.0f);
            }
    }
}

void drawPushButton (Graphics& g, Rectangle<float> r, bool lit, bool red)
{
    // Dark bottom/right shadow edge, then the face inset from the top/left highlight.
    g.setColour (Colour::greyLevel (0.08f));
    g.fillRoundedRectangle (r, 7.0f);

    auto face = r.withTrimmedBottom (6.0f).withTrimmedRight (5.0f);

    if (red)
    {
        auto base = lit ? Colour (0xffd8402e) : Colour (0xff9a1717);
        pillow (g, face, base.darker (0.05f), lit ? base.brighter (0.3f) : Colour (0xffa12626));
        if (lit) mottle (g, face.reduced (4.0f), 0.18f);
    }
    else if (lit)
    {
        pillow (g, face, Colour (0xff9e9d9d), Colour (0xffdcdbdb));
        mottle (g, face.reduced (4.0f), 0.12f);
    }
    else
    {
        pillow (g, face, Colour::greyLevel (0.157f), Colour::greyLevel (0.196f));
    }

    // Top/left bevel highlight.
    auto hi = red ? Colour (0xffcb8887) : Colour::greyLevel (lit ? 0.78f : 0.55f);
    ColourGradient top (hi, face.getX(), face.getY(), hi.withAlpha (0.0f), face.getX(), face.getY() + 6.0f, false);
    g.setGradientFill (top);
    g.fillRoundedRectangle (face.withHeight (7.0f), 6.0f);
    ColourGradient left (hi, face.getX(), face.getY(), hi.withAlpha (0.0f), face.getX() + 6.0f, face.getY(), false);
    g.setGradientFill (left);
    g.fillRoundedRectangle (face.withWidth (7.0f), 6.0f);
}

//==============================================================================
ButtonStack::ButtonStack (AudioProcessorValueTreeState& s, Spec sp, String choiceParam,
                          std::vector<int> cellValue, String toggleParam)
    : state (s), spec (std::move (sp)), choiceId (std::move (choiceParam)),
      toggleId (std::move (toggleParam)), values (std::move (cellValue))
{
    // Component covers the frame plus the label column.
    auto area = spec.frame;
    if (spec.labelsLeft) area = area.withLeft (spec.labelEdgeX - 70.0f);
    else                 area = area.withRight (spec.labelEdgeX + 100.0f);
    area = area.withTop (area.getY() - 4.0f).withBottom (area.getBottom() + 14.0f).withRight (area.getRight() + 14.0f);

    auto bounds = area.getSmallestIntegerContainer();
    setBounds (bounds);
    origin = bounds.getPosition().toFloat();

    state.addParameterListener (choiceId, this);
    if (toggleId.isNotEmpty())
        state.addParameterListener (toggleId, this);
}

ButtonStack::~ButtonStack()
{
    state.removeParameterListener (choiceId, this);
    if (toggleId.isNotEmpty())
        state.removeParameterListener (toggleId, this);
}

void ButtonStack::setInk (Colour c)
{
    if (c != ink) { ink = c; repaint(); }
}

void ButtonStack::parameterChanged (const String&, float)
{
    MessageManager::callAsync ([safe = SafePointer<ButtonStack> (this)] { if (safe != nullptr) safe->repaint(); });
}

bool ButtonStack::isLit (int cell) const
{
    if (spec.cells[(size_t) cell].red)
        return state.getRawParameterValue (toggleId)->load() > 0.5f;
    return (int) state.getRawParameterValue (choiceId)->load() == values[(size_t) cell];
}

void ButtonStack::paint (Graphics& g)
{
    g.addTransform (AffineTransform::translation (-origin));

    // Frame: soft cast shadow to the right, black well with a thin bevel line.
    auto frame = spec.frame;
    {
        Path p;
        p.addRectangle (frame);
        DropShadow (Colours::black.withAlpha (0.55f), 14, { 8, 4 }).drawForPath (g, p);
    }
    g.setColour (Colour::greyLevel (0.26f));
    g.fillRect (frame);
    g.setColour (Colour::greyLevel (0.09f));
    g.fillRect (frame.reduced (1.5f, 1.5f));
    g.setColour (Colour::greyLevel (0.27f));
    g.drawRect (frame.reduced (7.0f, 4.0f), 1.0f);

    for (int i = 0; i < (int) spec.cells.size(); ++i)
    {
        auto& c = spec.cells[(size_t) i];
        if (c.red)
        {
            g.setColour (Colour::greyLevel (0.09f));
            g.fillRect (c.bounds.expanded (4.0f, 3.0f));
        }
        drawPushButton (g, c.bounds, isLit (i), c.red);
    }

    // Labels: one or more lines per cell, ink box centred on the measured positions.
    size_t line = 0;
    for (int i = 0; i < (int) spec.cells.size(); ++i)
    {
        auto lit = spec.highlightSelectedLabel && isLit (i);
        for (auto& text : StringArray::fromTokens (spec.cells[(size_t) i].label, "\n", {}))
        {
            if (line >= spec.labelY.size()) break;
            Skin::drawLabelAligned (g, text, spec.labelEdgeX, spec.labelY[line++], 30.7f,
                                    lit ? Skin::activeBlue : ink, spec.labelsLeft);
        }
    }
}

void ButtonStack::mouseDown (const MouseEvent& e)
{
    auto p = e.position + origin;
    for (int i = 0; i < (int) spec.cells.size(); ++i)
    {
        if (! spec.cells[(size_t) i].bounds.contains (p))
            continue;

        if (spec.cells[(size_t) i].red)
        {
            if (auto* t = state.getParameter (toggleId))
            {
                t->beginChangeGesture();
                t->setValueNotifyingHost (t->getValue() > 0.5f ? 0.0f : 1.0f);
                t->endChangeGesture();
            }
        }
        else if (auto* c = dynamic_cast<RangedAudioParameter*> (state.getParameter (choiceId)))
        {
            c->beginChangeGesture();
            c->setValueNotifyingHost (c->convertTo0to1 ((float) values[(size_t) i]));
            c->endChangeGesture();
        }
        return;
    }
}
