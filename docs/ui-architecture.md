# UI Architecture

Chipper should feel like one instrument with many chip profiles, not a bundle of unrelated mini-plugins. The UI is therefore data-driven: the outer workflow stays stable while each chip definition decides which controls appear inside each module.

## Stable Shell

Every chip mode uses the same top-level layout:

1. Header
2. Chip tabs or chip selector
3. Profile
4. Sources / Voices
5. Tone / Filter / Shape
6. Envelope
7. Motion
8. Output / Utility
9. Bottom global strip

The footer/status area should include a subtle generated build label so a tester can match an installed VST3 to the reported source revision.

The module contents may change, but the order should not. Users should learn where concepts live once and carry that knowledge across NES, DMG, SID, YM2149, SN76489, FM, and sampler-style modes.

## Data Model

The UI should be built from chip definitions instead of per-chip hardcoded editor branches.

```cpp
enum class ControlType
{
    knob,
    slider,
    toggle,
    segmented,
    menu,
    waveButtons,
    meter
};

struct ParameterSpec
{
    std::string id;
    std::string label;
    std::string unit;
    ControlType controlType;
    float minValue;
    float maxValue;
    float defaultValue;
};

struct ModuleSpec
{
    std::string id;
    std::string title;
    std::vector<ParameterSpec> parameters;
};

struct ChipSpec
{
    ChipMode mode;
    std::string displayName;
    std::string modelLabel;
    std::vector<ModuleSpec> modules;
};
```

The editor asks the selected chip spec what modules and controls to show. The UI should not need SID, NES, or YM2612-specific register knowledge; that belongs in the chip adapter and engine mapping layer.

## Parameter Strategy

DAW automation IDs must remain stable after plugin load. Chipper should use a hybrid parameter strategy:

- Universal automatable macros for common musical performance controls.
- A universal Play Mode parameter for how one patch uses native chip channels.
- Stable chip-specific parameters grouped by chip and module.
- Inactive chip parameters hidden from the Chipper UI, but not created or destroyed dynamically.
- Internal chip state and register snapshots saved in presets when needed.

## Patch And Channel Model

Chipper exposes one patch per plugin instance. A patch can use every native chip channel inside the selected chip core, but users should not have to compose like a tracker by default.

- **Big Mono:** all useful native channels contribute to one played sound. This is the default.
- **Chip Poly:** overlapping notes allocate across finite native chip channels where the core supports it.
- **Manual:** reserved for explicit channel assignment and tracker-style routing.
- **Clone:** reserved for Hybrid mode engine cloning, where each MIDI note can own a virtual chip stack.

Only Big Mono and Chip Poly should be visible in the VST UI until Manual and Clone have engine implementations. Chip Poly should be enabled only for chip descriptors with tested finite-channel allocation; unsupported chips resolve requests back to Big Mono. Internal enums and renderer parsing may reserve those future modes, but visible controls should not imply behavior that does not exist.

UI controls should graduate from descriptor text to real controls only when they have an engine mapping, stable parameter identity, preset recall behavior, and renderer coverage. Planned chip controls can be described in module text, but should not appear as fake knobs before the core exists.

## Module Mapping

Equivalent concepts stay in equivalent places:

- NES / RP2A03: Sources are Pulse 1, Pulse 2, Triangle, Noise, DMC; Tone becomes Shape / Mixer.
- Game Boy / DMG: Sources are Pulse 1, Pulse 2, Wave, Noise; Tone becomes Wave / Noise.
- SID / C64: Sources are Voice 1-3; Tone becomes Filter.
- YM2149 / AY: Sources are A, B, C, shared noise; Tone becomes Mixer / Envelope.
- SN76489: Sources are Tone 1-3 and Noise; Tone becomes Tone / Crunch.
- YM2612 and OPL: Sources are FM voices; Tone becomes Operators.

Authentic mode should expose chip-native behavior. Hybrid mode can add musical helpers. Inspired mode should simplify controls and clearly avoid accuracy claims.

## Current Bridge

The current `ChipDescriptor` layer is the first implemented step toward this system. It provides chip names, summaries, macro templates, chip-specific labels for the universal macro controls, implementation status, Chip Poly capability, and six stable UI module definitions per chip. The JUCE editor renders those modules in a fixed shell and updates their contents from the selected descriptor. Implemented chips show live macro/native controls; planned chips keep roadmap module text visible but disable inactive controls so the UI does not imply sound behavior that is not backed by a core. The shared Play Mode parameter is the first patch/channel control exposed globally; Game Boy / DMG, YM2149 / AY, and SN76489 currently implement Chip Poly allocation across their finite pitched channels.

Future UI work should keep expanding `ChipDescriptor` into richer module and parameter specs rather than adding chip-specific editor branches.
