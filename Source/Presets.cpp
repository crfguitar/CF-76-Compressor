#include "Presets.h"
#include "Parameters.h"

using namespace juce;

namespace
{
    // Factory presets: only the values that differ from the defaults are listed.
    struct Factory
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> list {
            { "Default", {} },
            { "Vocal Lead",        { { ParamIDs::input, -26.0f }, { ParamIDs::output, -20.0f }, { ParamIDs::attack, 3.0f }, { ParamIDs::release, 5.0f } } },
            { "Vocal Aggressive",  { { ParamIDs::input, -20.0f }, { ParamIDs::output, -24.0f }, { ParamIDs::attack, 4.0f }, { ParamIDs::release, 6.0f }, { ParamIDs::ratio, 1.0f } } },
            { "Bass Control",      { { ParamIDs::input, -28.0f }, { ParamIDs::output, -20.0f }, { ParamIDs::attack, 2.5f }, { ParamIDs::release, 4.5f } } },
            { "Acoustic Guitar",   { { ParamIDs::input, -32.0f }, { ParamIDs::output, -18.0f }, { ParamIDs::attack, 2.0f }, { ParamIDs::release, 5.0f } } },
            { "Snare Snap",        { { ParamIDs::input, -24.0f }, { ParamIDs::output, -22.0f }, { ParamIDs::attack, 1.5f }, { ParamIDs::release, 6.0f }, { ParamIDs::ratio, 1.0f } } },
            { "Drum Room Smash",   { { ParamIDs::input, -18.0f }, { ParamIDs::output, -27.0f }, { ParamIDs::attack, 5.0f }, { ParamIDs::release, 7.0f }, { ParamIDs::ratio, 4.0f }, { ParamIDs::mix, 60.0f } } },
            { "Parallel Drums",    { { ParamIDs::input, -12.0f }, { ParamIDs::output, -30.0f }, { ParamIDs::attack, 6.0f }, { ParamIDs::release, 7.0f }, { ParamIDs::ratio, 4.0f }, { ParamIDs::mix, 35.0f } } },
            { "Gentle Glue",       { { ParamIDs::input, -36.0f }, { ParamIDs::output, -14.0f }, { ParamIDs::attack, 2.0f }, { ParamIDs::release, 3.0f }, { ParamIDs::mix, 70.0f } } },
            { "Peak Limiter 20:1", { { ParamIDs::input, -24.0f }, { ParamIDs::output, -22.0f }, { ParamIDs::attack, 6.0f }, { ParamIDs::release, 5.0f }, { ParamIDs::ratio, 3.0f } } },
            { "Preamp Colour",     { { ParamIDs::compOff, 1.0f } } },
        };
        return list;
    }
}

PresetManager::PresetManager (AudioProcessorValueTreeState& s, UndoManager& u) : apvts (s), undo (u)
{
    apvts.state.addListener (this);
}

PresetManager::~PresetManager()
{
    apvts.state.removeListener (this);
}

File PresetManager::userDirectory()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("CF76").getChildFile ("Presets");
}

Array<PresetManager::Entry> PresetManager::entries() const
{
    Array<Entry> list;
    int i = 0;
    for (auto& f : factoryPresets())
        list.add ({ f.name, true, i++, {} });

    auto files = userDirectory().findChildFiles (File::findFiles, false, "*" + fileExtension());
    files.sort();
    for (auto& f : files)
        list.add ({ f.getFileNameWithoutExtension(), false, -1, f });
    return list;
}

int PresetManager::currentIndex() const
{
    auto& slot = slots[(size_t) active];
    auto list = entries();
    for (int i = 0; i < list.size(); ++i)
        if (list[i].factory ? (slot.file == File() && list[i].name == slot.name) : list[i].file == slot.file)
            return i;
    return -1;
}

void PresetManager::applyState (const ValueTree& newState, bool undoable)
{
    ignoreChangesUntil = Time::getMillisecondCounter() + 500;
    if (undoable)
        undo.beginNewTransaction();

    // Set each parameter (rather than replacing the tree) so hosts see ordinary automation changes.
    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<RangedAudioParameter*> (p);
        if (ranged == nullptr) continue;

        auto child = newState.getChildWithProperty ("id", ranged->getParameterID());
        auto value = child.isValid() ? (float) child.getProperty ("value")
                                     : ranged->convertFrom0to1 (ranged->getDefaultValue());
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
        ranged->endChangeGesture();
    }
}

void PresetManager::markClean (const String& newName, const File& file)
{
    auto& slot = slots[(size_t) active];
    slot.name = newName;
    slot.file = file;
    slot.modified = false;
}

void PresetManager::load (const Entry& e)
{
    if (e.factory)
    {
        ValueTree state ("PARAMS");
        for (auto& [id, v] : factoryPresets()[(size_t) e.factoryIndex].values)
            state.appendChild (ValueTree ("PARAM", {}, {}).setProperty ("id", id, nullptr).setProperty ("value", v, nullptr), nullptr);
        applyState (state, true);
        markClean (e.name, {});
    }
    else
    {
        loadFile (e.file);
    }
}

void PresetManager::step (int delta)
{
    auto list = entries();
    if (list.isEmpty()) return;
    auto i = currentIndex();
    i = i < 0 ? 0 : (i + delta + list.size()) % list.size();
    load (list[i]);
}

void PresetManager::resetToDefault()
{
    load (entries()[0]);
}

bool PresetManager::loadFile (const File& f)
{
    auto xml = parseXML (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    applyState (ValueTree::fromXml (*xml), true);
    markClean (f.getFileNameWithoutExtension(), f);
    return true;
}

bool PresetManager::saveTo (const File& f)
{
    auto target = f.withFileExtension (fileExtension());
    target.getParentDirectory().createDirectory();
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (target))
        return false;

    markClean (target.getFileNameWithoutExtension(), target);
    return true;
}

bool PresetManager::canOverwrite() const
{
    return slots[(size_t) active].file.existsAsFile();
}

bool PresetManager::overwrite()
{
    return canOverwrite() && saveTo (slots[(size_t) active].file);
}

void PresetManager::toggleSlot()
{
    slots[(size_t) active].state = apvts.copyState();
    active ^= 1;

    auto& next = slots[(size_t) active];
    if (! next.state.isValid())
    {
        // First visit to the other slot starts as a copy of the current setup.
        next = slots[(size_t) (active ^ 1)];
        return;
    }
    auto modified = next.modified;
    applyState (next.state, true);
    next.modified = modified;
}

void PresetManager::copyToOtherSlot()
{
    auto& other = slots[(size_t) (active ^ 1)];
    other = slots[(size_t) active];
    other.state = apvts.copyState();
}

void PresetManager::valueTreePropertyChanged (ValueTree&, const Identifier&)
{
    if (Time::getMillisecondCounter() > ignoreChangesUntil)
        slots[(size_t) active].modified = true;
}

void PresetManager::writeTo (XmlElement& xml) const
{
    auto* e = xml.createNewChildElement ("PRESETS");
    e->setAttribute ("active", active);
    for (int i = 0; i < 2; ++i)
    {
        auto& s = slots[(size_t) i];
        auto* se = e->createNewChildElement ("SLOT");
        se->setAttribute ("name", s.name);
        se->setAttribute ("file", s.file.getFullPathName());
        se->setAttribute ("modified", s.modified);
        if (i != active && s.state.isValid())
            if (auto st = s.state.createXml())
                se->addChildElement (st.release());
    }
}

void PresetManager::readFrom (const XmlElement& xml)
{
    auto* e = xml.getChildByName ("PRESETS");
    if (e == nullptr) return;

    ignoreChangesUntil = Time::getMillisecondCounter() + 500;
    active = jlimit (0, 1, e->getIntAttribute ("active"));
    int i = 0;
    for (auto* se : e->getChildWithTagNameIterator ("SLOT"))
    {
        if (i > 1) break;
        auto& s = slots[(size_t) i++];
        s.name = se->getStringAttribute ("name", "Default");
        auto path = se->getStringAttribute ("file");
        s.file = path.isNotEmpty() ? File (path) : File();
        s.modified = se->getBoolAttribute ("modified");
        if (auto* st = se->getChildByName (apvts.state.getType()))
            s.state = ValueTree::fromXml (*st);
    }
}
