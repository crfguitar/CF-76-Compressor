#include "TopBar.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace
{
    // Boxes including their 3px grey outline (reference px).
    const Rectangle<float> badge   { 4.0f, 6.0f, 694.0f, 55.0f };
    const Rectangle<float> undoBox { 703.0f, 9.0f, 104.0f, 49.0f };
    const Rectangle<float> redoBox { 814.0f, 9.0f, 104.0f, 49.0f };
    const Rectangle<float> preset  { 924.0f, 9.0f, 742.0f, 49.0f };
    const Rectangle<float> prevBox { 1673.0f, 9.0f, 104.0f, 49.0f };
    const Rectangle<float> nextBox { 1783.0f, 9.0f, 104.0f, 49.0f };
    const Rectangle<float> abBox   { 1893.0f, 9.0f, 143.0f, 49.0f };
    const Rectangle<float> setup   { 2042.0f, 9.0f, 143.0f, 49.0f };
    const Rectangle<float> saveBox { 2191.0f, 9.0f, 143.0f, 49.0f };
    const Rectangle<float> menuBox { 2340.0f, 9.0f, 49.0f, 49.0f };

    const Colour outline  { 0xffb4b4b4 };
    const Colour boxFill  { 0xff232323 };
    const Colour textGrey { 0xffa5a5a5 };
    const Colour iconGrey { 0xffb4b4b4 };
    const Colour dimGrey  { 0xff4a4a4a };

    // Chrome badge: measured luminance for rows 6..60 (identical across its width).
    const uint8 badgeRows[] = { 14, 43, 93, 153, 205, 218, 226, 230, 230, 229, 228, 226, 223, 220, 215, 208, 199,
                                190, 181, 176, 172, 171, 170, 169, 169, 169, 166, 165, 164, 162, 160, 156, 153, 150,
                                145, 140, 135, 130, 126, 123, 122, 121, 123, 126, 130, 134, 139, 144, 147, 150, 149,
                                134, 70, 28, 1 };

    void drawBox (Graphics& g, Rectangle<float> r)
    {
        g.setColour (outline);
        g.fillRect (r);
        g.setColour (boxFill);
        g.fillRect (r.reduced (3.0f));
    }

    // Text with its baseline at y, horizontally centred on cx (ink box).
    void drawTextAt (Graphics& g, const String& text, float cx, float baseline, float px, Colour c)
    {
        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions ("Arial", px, Font::plain)), text, 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        auto ink = p.getBounds();
        p.applyTransform (AffineTransform::translation (cx - ink.getCentreX(), baseline));
        g.setColour (c);
        g.fillPath (p);
    }

    void drawUndoArrow (Graphics& g, Rectangle<float> box, bool mirrored, Colour c)
    {
        // Curved arrow: open arc on the right, arrowhead at the lower left (mirrored for redo).
        Path arc, p;
        arc.addCentredArc (0.0f, 0.0f, 13.0f, 11.0f, 0.0f, -2.6f, 1.9f, true);
        PathStrokeType (5.0f).createStrokedPath (p, arc);
        Path head;
        head.addTriangle (-24.0f, 4.0f, -6.0f, -6.0f, -7.0f, 12.0f);
        p.addPath (head);
        auto xf = AffineTransform::scale (mirrored ? -1.0f : 1.0f, 1.0f)
                      .translated (box.getCentreX() + (mirrored ? -3.0f : 3.0f), box.getCentreY() - 2.0f);
        p.applyTransform (xf);
        g.setColour (c);
        g.fillPath (p);
    }

    void drawBlockArrow (Graphics& g, Rectangle<float> box, bool right)
    {
        // Measured: 36 x 31 px block arrow centred in the box.
        Path p;
        p.startNewSubPath (-17.5f, 0.0f);
        p.lineTo (-2.5f, -15.5f);
        p.lineTo (-2.5f, -7.0f);
        p.lineTo (17.5f, -7.0f);
        p.lineTo (17.5f, 7.0f);
        p.lineTo (-2.5f, 7.0f);
        p.lineTo (-2.5f, 15.5f);
        p.closeSubPath();
        p.applyTransform (AffineTransform::scale (right ? -1.0f : 1.0f, 1.0f).translated (box.getCentreX(), 33.0f));
        g.setColour (iconGrey);
        g.fillPath (p);
    }
}

TopBar::TopBar (CF76AudioProcessor& p) : proc (p)
{
    setBounds (0, 0, Ref::width, 62);
    startTimerHz (10);
}

void TopBar::timerCallback()
{
    // Cheap poll of everything the bar displays; repaint only on change.
    auto& pm = proc.presets;
    auto sig = pm.name() + (pm.isModified() ? "*" : "") + String (pm.activeSlot())
             + (proc.undoManager.canUndo() ? "u" : "") + (proc.undoManager.canRedo() ? "r" : "");
    if (sig != lastSignature)
    {
        lastSignature = sig;
        repaint();
    }
}

void TopBar::paint (Graphics& g)
{
    // Background rows: thin grey strip at the very top, then black.
    g.fillAll (Colours::black);
    g.setColour (Colour::greyLevel (0.17f));
    g.fillRect (0, 1, Ref::width, 2);
    g.setColour (Colour::greyLevel (0.2f));
    g.fillRect (0, 0, Ref::width, 1);

    // Chrome badge with the plugin name.
    for (int i = 0; i < (int) std::size (badgeRows); ++i)
    {
        g.setColour (Colour::greyLevel ((float) badgeRows[i] / 255.0f));
        g.fillRect (badge.getX() + 1.0f, badge.getY() + (float) i, badge.getWidth() - 2.0f, 1.0f);
    }
    {
        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions ("Arial", 36.0f, Font::bold)).withExtraKerningFactor (0.14f),
                          "CF-76", 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        // Scale to the reference badge lettering: 25px cap height, bottom of the caps at y 46.
        auto ink = p.getBounds();
        auto k = 25.0f / ink.getHeight();
        p.applyTransform (AffineTransform::translation (-ink.getCentreX(), -ink.getBottom())
                              .scaled (k).translated (badge.getCentreX(), 46.0f));
        g.setColour (Colours::white.withAlpha (0.35f));
        g.fillPath (p, AffineTransform::translation (0.0f, 1.5f));
        g.setColour (Colour (0xff262626));
        g.fillPath (p);
    }

    for (auto r : { undoBox, redoBox, preset, prevBox, nextBox, abBox, setup, saveBox, menuBox })
        drawBox (g, r);

    drawUndoArrow (g, undoBox, false, proc.undoManager.canUndo() ? iconGrey : dimGrey);
    drawUndoArrow (g, redoBox, true, proc.undoManager.canRedo() ? iconGrey : dimGrey);

    // Preset name, e.g. "A: Default*", centred left of the dropdown marker (baseline at y 44).
    auto& pm = proc.presets;
    auto slotLetter = pm.activeSlot() == 0 ? "A" : "B";
    drawTextAt (g, String (slotLetter) + ": " + pm.name() + (pm.isModified() ? "*" : ""), 1271.0f, 44.0f, 32.0f, textGrey);

    g.setColour (iconGrey);
    g.fillRect (1619.0f, 19.0f, 4.0f, 29.0f);
    Path tri;
    tri.addTriangle (1630.0f, 26.0f, 1649.0f, 26.0f, 1639.5f, 41.0f);
    g.fillPath (tri);

    drawBlockArrow (g, prevBox, false);
    drawBlockArrow (g, nextBox, true);

    auto other = pm.activeSlot() == 0 ? "B" : "A";
    drawTextAt (g, String (slotLetter) + String (CharPointer_UTF8 (" \xe2\x86\x92 ")) + other, abBox.getCentreX(), 44.0f, 32.0f, textGrey);
    drawTextAt (g, String ("Setup ") + slotLetter, setup.getCentreX(), 44.0f, 32.0f, textGrey);
    drawTextAt (g, "Save", saveBox.getCentreX(), 44.0f, 32.0f, textGrey);

    g.setColour (Colour (0xffa3a3a3));
    for (float y : { 22.0f, 32.0f, 42.0f })
        g.fillRect (2350.0f, y, 29.0f, 3.0f);
}

TopBar::Item TopBar::itemAt (Point<float> p) const
{
    const std::pair<Rectangle<float>, Item> map[] = {
        { undoBox, Item::undo }, { redoBox, Item::redo }, { preset, Item::preset }, { prevBox, Item::prev },
        { nextBox, Item::next }, { abBox, Item::copyAB }, { setup, Item::setup }, { saveBox, Item::save },
        { menuBox, Item::menu } };
    for (auto& [r, item] : map)
        if (r.contains (p))
            return item;
    return Item::none;
}

void TopBar::mouseDown (const MouseEvent& e)
{
    auto& pm = proc.presets;
    switch (itemAt (e.position))
    {
        case Item::undo:   proc.undoManager.undo(); break;
        case Item::redo:   proc.undoManager.redo(); break;
        case Item::preset: showPresetMenu(); break;
        case Item::prev:   pm.step (-1); break;
        case Item::next:   pm.step (1); break;
        case Item::copyAB: pm.copyToOtherSlot(); break;
        case Item::setup:  pm.toggleSlot(); break;
        case Item::save:   if (! pm.overwrite()) saveAs(); break;
        case Item::menu:   showMainMenu(); break;
        case Item::none:   break;
    }
    timerCallback();
}

void TopBar::showPresetMenu()
{
    auto& pm = proc.presets;
    auto list = pm.entries();
    auto current = pm.currentIndex();

    PopupMenu menu;
    menu.addSectionHeader ("Factory");
    bool userHeader = false;
    for (int i = 0; i < list.size(); ++i)
    {
        if (! list[i].factory && ! userHeader)
        {
            menu.addSectionHeader ("User");
            userHeader = true;
        }
        menu.addItem (i + 1, list[i].name, true, i == current);
    }

    menu.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (preset.toNearestInt())),
                        [safe = SafePointer<TopBar> (this), list] (int result)
                        {
                            // The menu can outlive the editor (plug-in deleted while it is open).
                            if (safe != nullptr && result > 0)
                                safe->proc.presets.load (list[result - 1]);
                        });
}

void TopBar::showMainMenu()
{
    PopupMenu menu;
    menu.addItem (1, "Save As...");
    menu.addItem (2, "Load Preset File...");
    menu.addItem (3, "Show Presets Folder");
    menu.addSeparator();
    menu.addItem (4, "Reset to Default");

    menu.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (menuBox.toNearestInt())),
                        [safe = SafePointer<TopBar> (this)] (int result)
                        {
                            if (safe == nullptr) return;
                            switch (result)
                            {
                                case 1: safe->saveAs(); break;
                                case 2: safe->loadFromFile(); break;
                                case 3: PresetManager::userDirectory().createDirectory();
                                        PresetManager::userDirectory().startAsProcess(); break;
                                case 4: safe->proc.presets.resetToDefault(); break;
                                default: break;
                            }
                        });
}

void TopBar::saveAs()
{
    auto dir = PresetManager::userDirectory();
    dir.createDirectory();
    chooser = std::make_unique<FileChooser> ("Save CF-76 preset", dir.getChildFile (proc.presets.name()),
                                             "*" + PresetManager::fileExtension());
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                              | FileBrowserComponent::warnAboutOverwriting,
                          [safe = SafePointer<TopBar> (this)] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (safe != nullptr && f != File())
                                  safe->proc.presets.saveTo (f);
                          });
}

void TopBar::loadFromFile()
{
    chooser = std::make_unique<FileChooser> ("Load CF-76 preset", PresetManager::userDirectory(),
                                             "*" + PresetManager::fileExtension());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = SafePointer<TopBar> (this)] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile())
                                  safe->proc.presets.loadFile (f);
                          });
}
