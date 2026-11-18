#include "Segmented.h"

using namespace juce;

namespace
{
    void drawCreamButton (Graphics& g, Rectangle<float> r, bool on)
    {
        // Contact shadow under the button.
        g.setColour (Colour (0xff0a0a0a));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 4.0f);

        // Body: lit cream when on, khaki when off; bright top lip, darker bottom edge.
        auto body = on ? Colour (0xfff8f0ce) : Colour (0xffaea680);
        ColourGradient fill (on ? Colour (0xfffffbe6) : Colour (0xffd9d2b7), r.getX(), r.getY(),
                             body.darker (0.55f), r.getX(), r.getBottom(), false);
        fill.addColour (0.12, body);
        fill.addColour (0.82, body.darker (on ? 0.02f : 0.05f));
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
}

SegmentedButtons::SegmentedButtons (AudioProcessorValueTreeState& s, String id,
                                    std::vector<Segment> segs, float pressedDrop)
    : state (s), paramId (std::move (id)), segments (std::move (segs)), drop (pressedDrop)
{
    isBool = dynamic_cast<AudioParameterBool*> (state.getParameter (paramId)) != nullptr;

    Rectangle<float> area;
    for (auto& seg : segments)
        area = area.isEmpty() ? seg.bounds : area.getUnion (seg.bounds);
    auto bounds = area.expanded (4.0f, 6.0f).withBottom (area.getBottom() + drop + 6.0f).getSmallestIntegerContainer();
    setBounds (bounds);
    origin = bounds.getPosition().toFloat();

    state.addParameterListener (paramId, this);
}

SegmentedButtons::~SegmentedButtons()
{
    state.removeParameterListener (paramId, this);
}

void SegmentedButtons::parameterChanged (const String&, float)
{
    MessageManager::callAsync ([safe = SafePointer<SegmentedButtons> (this)] { if (safe != nullptr) safe->repaint(); });
}

bool SegmentedButtons::isOn (const Segment& seg) const
{
    auto v = state.getRawParameterValue (paramId)->load();
    return isBool ? v > 0.5f : (int) v == seg.value;
}

void SegmentedButtons::paint (Graphics& g)
{
    g.addTransform (AffineTransform::translation (-origin));
    for (auto& seg : segments)
    {
        auto on = isOn (seg);
        auto r = seg.bounds.translated (0.0f, on ? drop : 0.0f);
        drawCreamButton (g, r, on);
        if (seg.text.isNotEmpty())
            Skin::drawLabel (g, seg.text, r.getCentre().translated (0.0f, -0.5f), 30.7f, Colour (0xff15140f));
    }
}

void SegmentedButtons::mouseDown (const MouseEvent& e)
{
    auto p = e.position + origin;
    for (auto& seg : segments)
    {
        if (! seg.bounds.expanded (0.0f, drop).contains (p))
            continue;

        if (auto* param = dynamic_cast<RangedAudioParameter*> (state.getParameter (paramId)))
        {
            param->beginChangeGesture();
            if (isBool)
                param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
            else
                param->setValueNotifyingHost (param->convertTo0to1 ((float) seg.value));
            param->endChangeGesture();
        }
        return;
    }
}
