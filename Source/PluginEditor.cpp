#include "PluginEditor.h"
#include "DevTests.h"

using namespace juce;

//==============================================================================
struct Canvas::RevisionListener : AudioProcessorValueTreeState::Listener
{
    explicit RevisionListener (Canvas& c) : canvas (c) {}
    void parameterChanged (const String&, float) override
    {
        MessageManager::callAsync ([safe = Component::SafePointer<Canvas> (&canvas)]
                                   { if (safe != nullptr) safe->updateSkin(); });
    }
    Canvas& canvas;
};

Canvas::Canvas (CF76AudioProcessor& p) : proc (p)
{
    setSize (Ref::width, Ref::height);

    struct Placement { Knob& knob; const char* param; float cx, cy; };
    for (auto& k : { Placement { input,   ParamIDs::input,   285.8f,  401.5f },
                     Placement { output,  ParamIDs::output,  752.65f, 401.5f },
                     Placement { attack,  ParamIDs::attack,  1137.6f, 261.5f },
                     Placement { release, ParamIDs::release, 1137.6f, 532.6f },
                     Placement { mix,     ParamIDs::mix,     1851.75f, 790.7f },
                     Placement { trim,    ParamIDs::trim,    2149.9f, 790.8f } })
    {
        addAndMakeVisible (k.knob);
        k.knob.setCentre (k.cx, k.cy);
        attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, k.param, k.knob));
    }

    {
        ButtonStack::Spec ratio;
        const float tops[] = { 180.0f, 266.0f, 352.0f, 437.0f, 524.0f };
        const char* names[] = { "20", "12", "8", "4", "ALL" };
        for (int i = 0; i < 5; ++i)
            ratio.cells.push_back ({ { 1414.0f, tops[i], 46.0f, 82.0f }, names[i] });
        ratio.frame = { 1404.0f, 171.0f, 60.0f, 438.0f };
        ratio.labelsLeft = true;
        ratio.labelEdgeX = 1386.5f;
        ratio.labelY = { 226.0f, 311.5f, 392.5f, 476.5f, 561.5f };
        ratio.highlightSelectedLabel = true;
        // Top to bottom: 20, 12, 8, 4, All -> parameter choices 4:1=0 ... 20:1=3, All=4
        ratioButtons = std::make_unique<ButtonStack> (proc.apvts, ratio, ParamIDs::ratio, std::vector<int> { 3, 2, 1, 0, 4 });
        addAndMakeVisible (*ratioButtons);

        ButtonStack::Spec meter;
        meter.cells = { { { 2205.0f, 181.0f, 44.0f, 101.0f }, "GR" },
                        { { 2205.0f, 289.0f, 44.0f, 101.0f }, "IN" },
                        { { 2205.0f, 395.0f, 44.0f, 101.0f }, "OUT" },
                        { { 2201.0f, 504.0f, 48.0f, 100.0f }, "COMP\nOFF", true } };
        meter.frame = { 2195.0f, 171.0f, 61.0f, 438.0f };
        meter.labelsLeft = false;
        meter.labelEdgeX = 2281.0f;
        meter.labelY = { 232.5f, 339.0f, 445.5f, 539.0f, 568.5f };
        meterButtons = std::make_unique<ButtonStack> (proc.apvts, meter, ParamIDs::meter, std::vector<int> { 0, 1, 2, -1 },
                                                      ParamIDs::compOff);
        addAndMakeVisible (*meterButtons);
    }
    addAndMakeVisible (vu);
    addAndMakeVisible (toolbar);
    addMouseListener (this, true); // every panel gesture starts a new undo step
    startTimerHz (15);

    // Bottom bar (unpressed button rectangles measured from the reference).
    autoMakeup = std::make_unique<SegmentedButtons> (proc.apvts, ParamIDs::autoMakeup,
        std::vector<SegmentedButtons::Segment> { { { 322.0f, 773.0f, 61.0f, 38.0f }, {}, 0 } });
    analog = std::make_unique<SegmentedButtons> (proc.apvts, ParamIDs::analog,
        std::vector<SegmentedButtons::Segment> { { { 658.0f, 773.0f, 114.0f, 38.0f }, "50Hz", (int) Analog::hz50 },
                                                 { { 778.0f, 773.0f, 114.0f, 38.0f }, "60Hz", (int) Analog::hz60 },
                                                 { { 898.0f, 773.0f, 115.0f, 38.0f }, "OFF",  (int) Analog::off } });
    revisionButtons = std::make_unique<SegmentedButtons> (proc.apvts, ParamIDs::revision,
        std::vector<SegmentedButtons::Segment> { { { 1262.0f, 774.0f, 172.0f, 38.0f }, "BLUEY",  (int) Revision::bluey },
                                                 { { 1439.0f, 774.0f, 173.0f, 38.0f }, "BLACKY", (int) Revision::blacky } },
        2.0f);
    for (auto* c : { autoMakeup.get(), analog.get(), revisionButtons.get() })
        addAndMakeVisible (c);

    revisionListener = std::make_unique<RevisionListener> (*this);
    proc.apvts.addParameterListener (ParamIDs::revision, revisionListener.get());
    updateSkin();
}

void Canvas::mouseDown (const MouseEvent& e)
{
    if (e.eventComponent == this && e.position.getDistanceFrom (Skin::lampCentre) < 38.0f)
    {
        proc.clearOvers(); // the lamp is the over indicator; clicking it clears the latch
        return;
    }
    if (e.eventComponent != &toolbar)
        proc.undoManager.beginNewTransaction();
}

void Canvas::timerCallback()
{
    // Over lamp follows the selected meter source (IN: input, GR/OUT: output).
    auto mode = (MeterMode) (int) proc.apvts.getRawParameterValue (ParamIDs::meter)->load();
    bool lit = mode == MeterMode::in ? proc.overIn.load() : proc.overOut.load();
    if (lit != lampLit)
    {
        lampLit = lit;
        repaint (Rectangle<float> (90.0f, 90.0f).withCentre (Skin::lampCentre).getSmallestIntegerContainer());
    }
}

Revision Canvas::revision() const
{
    return (Revision) (int) proc.apvts.getRawParameterValue (ParamIDs::revision)->load();
}

void Canvas::updateSkin()
{
    auto ink = revision() == Revision::bluey ? Skin::blueyInk : Skin::labelText;
    for (auto* k : { &input, &output, &attack, &release })
        k->setInk (ink);
    ratioButtons->setInk (ink);
    meterButtons->setInk (ink);
    repaint();
}

Canvas::~Canvas()
{
    proc.apvts.removeParameterListener (ParamIDs::revision, revisionListener.get());
}

void Canvas::paint (Graphics& g)
{
    auto rev = revision();
    g.drawImageAt (Skin::panelImage (*skinCache, rev), 0, 0);
    auto panelInk = rev == Revision::bluey ? Skin::blueyInk : Skin::labelText;

    // Static captions (centres measured from the reference).
    struct Caption { const char* text; float cx, cy, px; };
    for (auto& c : { Caption { "INPUT",   289.5f,  614.5f, 30.7f },
                     Caption { "OUTPUT",  756.4f,  614.5f, 30.7f },
                     Caption { "ATTACK",  1139.0f, 149.5f, 30.7f },
                     Caption { "RELEASE", 1138.0f, 668.5f, 30.7f },
                     Caption { "RATIO",   1436.0f, 149.5f, 30.7f },
                     Caption { "METER",   2228.0f, 149.5f, 30.7f },
                     Caption { "AUTO MAKEUP", 179.5f, 792.5f, 33.5f },
                     Caption { "ANALOG",  565.0f,  792.5f, 33.5f },
                     Caption { "REVISION", 1161.0f, 794.5f, 33.5f },
                     Caption { "MIX",     1747.0f, 795.5f, 33.5f },
                     Caption { "TRIM",    2041.0f, 795.5f, 33.5f } })
        Skin::drawLabel (g, c.text, { c.cx, c.cy }, c.px, c.cy > 712.0f ? Skin::labelText : panelInk);

    Skin::drawLamp (g, lampLit);
    Skin::drawLogo (g, *skinCache);
}

//==============================================================================
CF76AudioProcessorEditor::CF76AudioProcessorEditor (CF76AudioProcessor& p)
    : AudioProcessorEditor (p), proc (p), canvas (p)
{
    addAndMakeVisible (canvas);

    setResizable (true, true);
    setResizeLimits (Ref::width / 4, Ref::height / 4, Ref::width, Ref::height);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) Ref::width / (double) Ref::height);
    setSize (Ref::width / 2, Ref::height / 2);

    // Dev hook: `CF76_SNAPSHOT=<dir> CF-76.exe` renders both skins at reference scale and exits.
    if (JUCEApplicationBase::isStandaloneApp()
        && (SystemStats::getEnvironmentVariable ("CF76_SNAPSHOT", {}).isNotEmpty()
            || SystemStats::getEnvironmentVariable ("CF76_SELFTEST", {}).isNotEmpty()
            || SystemStats::getEnvironmentVariable ("CF76_DSPTEST", {}).isNotEmpty()))
        startTimer (800);
}

void CF76AudioProcessorEditor::resized()
{
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Ref::width));
}

void CF76AudioProcessorEditor::timerCallback()
{
    stopTimer();
    auto dspReport = SystemStats::getEnvironmentVariable ("CF76_DSPTEST", {});
    if (dspReport.isNotEmpty())
    {
        File (dspReport).replaceWithText (runDspTests());
        JUCEApplicationBase::quit();
        return;
    }
    if (SystemStats::getEnvironmentVariable ("CF76_SELFTEST", {}).isNotEmpty())
    {
        runSelfTest(); // quits when its last step has run
        return;
    }
    writeSnapshots();
    JUCEApplicationBase::quit();
}

void CF76AudioProcessorEditor::runSelfTest()
{
    // Dev hook: exercises presets, A/B and undo through the same calls the toolbar makes.
    // Steps run 300 ms apart so the APVTS can sync parameters into its ValueTree (undo relies on it).
    struct Ctx { StringArray log; File tmp; };
    auto ctx = std::make_shared<Ctx>();
    ctx->tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("cf76_selftest" + PresetManager::fileExtension());

    auto& pm = proc.presets;
    auto value = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto check = [ctx] (bool ok, const String& what) { ctx->log.add ((ok ? "PASS  " : "FAIL  ") + what); };
    auto setInput = [this] (float db)
    {
        auto* p = proc.apvts.getParameter (ParamIDs::input);
        proc.undoManager.beginNewTransaction();
        p->setValueNotifyingHost (p->convertTo0to1 (db));
    };

    auto steps = std::make_shared<std::vector<std::function<void()>>>();
    auto& st = *steps;
    st.push_back ([&, check, value] {
        pm.resetToDefault();
        check (pm.name() == "Default" && value (ParamIDs::input) == -30.0f, "reset -> Default (input -30)");
        pm.step (1);
        check (pm.name() == "Vocal Lead" && value (ParamIDs::input) == -26.0f, "next -> Vocal Lead (input -26)");
        pm.step (-1);
        check (pm.name() == "Default", "prev -> Default");
        proc.undoManager.clearUndoHistory();
    });
    st.push_back ([&] { pm.step (1); });
    st.push_back ([&, check, value] {
        check (proc.undoManager.canUndo(), "preset load is undoable");
        proc.undoManager.undo();
    });
    st.push_back ([&, check, value] {
        check (value (ParamIDs::input) == -30.0f, "undo restores input -30 (got " + String (value (ParamIDs::input)) + ")");
        proc.undoManager.redo();
    });
    st.push_back ([&, check, value, setInput] {
        check (value (ParamIDs::input) == -26.0f, "redo re-applies input -26 (got " + String (value (ParamIDs::input)) + ")");
        setInput (-12.0f);
    });
    st.push_back ([&, check, value, ctx, setInput] {
        check (pm.isModified(), "edit marks preset modified (*)");
        check (pm.saveTo (ctx->tmp) && ! pm.isModified() && pm.name() == "cf76_selftest", "save as user preset");
        pm.resetToDefault();
        check (pm.loadFile (ctx->tmp) && value (ParamIDs::input) == -12.0f, "load user preset restores input -12");
        ctx->tmp.deleteFile();

        pm.toggleSlot();
        check (pm.activeSlot() == 1 && value (ParamIDs::input) == -12.0f, "Setup B starts as a copy of A");
        setInput (-40.0f);
        pm.toggleSlot();
        check (pm.activeSlot() == 0 && value (ParamIDs::input) == -12.0f, "back to A restores A (-12)");
        pm.toggleSlot();
        check (value (ParamIDs::input) == -40.0f, "B kept its own setting (-40)");
        pm.copyToOtherSlot();
        pm.toggleSlot();
        check (pm.activeSlot() == 0 && value (ParamIDs::input) == -40.0f, "B -> A copied B's setting into A");

        MemoryBlock mb;
        proc.getStateInformation (mb);
        pm.resetToDefault();
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        check (value (ParamIDs::input) == -40.0f && pm.activeSlot() == 0, "host state save/restore");
        pm.step (1);
    });
    st.push_back ([] {});
    st.push_back ([] {});
    st.push_back ([&, check] { check (! pm.isModified(), "no false '*' after a preset load settles (900 ms)"); });

    // Auto Makeup: Input drives Output (asynchronously), Output never drives Input.
    auto setParam = [this] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    st.push_back ([&, setParam] { pm.resetToDefault(); });
    st.push_back ([&, setParam] { setParam (ParamIDs::autoMakeup, 1.0f); });
    st.push_back ([&, setParam] { setParam (ParamIDs::input, -20.0f); });
    st.push_back ([&, check, value, setParam] {
        auto expected = -18.0f - (CF76AudioProcessor::referenceOutputDb (-20.0f, Ratio::r4)
                                  - CF76AudioProcessor::referenceOutputDb (-30.0f, Ratio::r4));
        check (std::abs (value (ParamIDs::output) - expected) < 0.15f,
               "Auto Makeup: Input -30 -> -20 moves Output to " + String (value (ParamIDs::output), 1)
               + " (expected " + String (expected, 1) + ")");
        setParam (ParamIDs::output, -24.0f);
    });
    st.push_back ([&, check, value, setParam] {
        check (value (ParamIDs::input) == -20.0f, "Auto Makeup: moving Output leaves Input alone");
        setParam (ParamIDs::input, -30.0f);
    });
    st.push_back ([&, check, value] {
        auto expected = -24.0f + (CF76AudioProcessor::referenceOutputDb (-20.0f, Ratio::r4)
                                  - CF76AudioProcessor::referenceOutputDb (-30.0f, Ratio::r4));
        check (std::abs (value (ParamIDs::output) - expected) < 0.15f,
               "Auto Makeup keeps the new offset after a manual Output move (" + String (value (ParamIDs::output), 1) + ")");
    });

    auto out = File (SystemStats::getEnvironmentVariable ("CF76_SELFTEST", {}));
    auto run = std::make_shared<std::function<void (size_t)>>();
    *run = [steps, run, ctx, out] (size_t i)
    {
        if (i == steps->size())
        {
            out.replaceWithText (ctx->log.joinIntoString (newLine));
            JUCEApplicationBase::quit();
            return;
        }
        (*steps)[i]();
        Timer::callAfterDelay (300, [run, i] { (*run) (i + 1); });
    };
    (*run) (0);
}

void CF76AudioProcessorEditor::writeSnapshots()
{
    File dir (SystemStats::getEnvironmentVariable ("CF76_SNAPSHOT", {}));
    dir.createDirectory();

    // Control settings visible in the reference shots.
    auto set = [this] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set (ParamIDs::input, -30.0f);
    set (ParamIDs::output, -24.0f);
    set (ParamIDs::attack, 2.65f);
    set (ParamIDs::release, 6.78f);
    set (ParamIDs::ratio, 0.0f);
    set (ParamIDs::meter, 0.0f);
    set (ParamIDs::analog, 0.0f);
    set (ParamIDs::mix, 100.0f);
    set (ParamIDs::trim, 0.0f);
    set (ParamIDs::autoMakeup, 1.0f);

    for (auto [rev, name] : { std::pair { 0.0f, "bluey" }, std::pair { 1.0f, "blacky" } })
    {
        set (ParamIDs::revision, rev);
        canvas.updateSkin();
        canvas.getVU().setReading (0.0f);
        auto img = canvas.createComponentSnapshot (canvas.getLocalBounds(), true, 1.0f);
        FileOutputStream out (dir.getChildFile (String ("snap_") + name + ".png"));
        out.setPosition (0);
        out.truncate();
        PNGImageFormat().writeImageToStream (img, out);
    }
}
