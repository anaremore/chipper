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

## Slot Scalability

The six slots are functional zones, not six rigid equal boxes. Every chip should use the same concepts:

1. Chip Profile
2. Sources
3. Tone / Shape
4. Envelope / Level
5. Motion / Performance
6. Output / Utility

Chip manifests may choose different panel templates and layout density inside those zones. PSG, wavetable, and most sample chips fit a standard channel-bank layout. FM chips should use the same conceptual slots but need a denser operator editor or operator grid inside Tone / Shape. Sample-heavy chips such as SPC700-style and Paula modes should use sample voice banks plus sample/loop/pitch panels instead of oscillator-style controls.

The product rule is: every chip feels distinct, but no chip feels like a different plugin.

## Component Strategy

Use JUCE's native plugin-safe controls for DAW-facing behavior: `ComboBox` for selectors, `Slider` for continuous automatable parameters, `Label` for readouts/status, and button-style controls for future discrete actions. Add Chipper-specific presentation components only where the chip metaphor needs structure that generic controls do not provide.

The first custom surface is the NES channel card grid inside Sources. It keeps the stable six-slot shell, but makes Pulse 1, Pulse 2, Triangle, and Noise visible as chip channels. Similar chip surfaces should follow this pattern: PSG channel banks, FM operator grids, sampler voice banks, wavetable editors, and macro/readout tiles. These components should reflect stable parameters and tested engine behavior rather than presenting fake controls for planned features.

NES and DMG Pulse Duty use the first register-choice component. It presents a segmented button group over the stable `macroControl1` parameter because RP2A03 and DMG pulse duty are four-state register fields, not continuous values. Future chip controls should follow the same rule: use continuous controls only for continuous behavior, segmented/stepped controls for register-like behavior, and macro controls only when they intentionally map a musical gesture to chip-native states.

NES and DMG source cards are the first stable non-macro channel controls. They attach to universal `source1Enabled` through `source4Enabled` parameters and map to native source enable behavior: pulse 1, pulse 2, triangle/wave, and noise. Future chip source cards should preserve that user promise: active cards must represent audible/native sources, not decorative labels.

SN76489 Noise Mode is the next register-choice surface. Its segmented choices map to the PSG noise-control bits, while `Macro` preserves the chip-specific macro template and shows the resolved register value in the readout. This establishes a broader rule: macro/preset selections are per-chip musical templates, and visible readouts should reflect the macro-resolved chip state rather than only raw control offsets.

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

enum class ParameterKind
{
    continuous,
    discreteChoice,
    steppedNumeric,
    booleanToggle,
    macro,
    chipRegister,
    hiddenInternal
};

struct ParameterSpec
{
    std::string id;
    std::string label;
    std::string unit;
    ControlType controlType;
    ParameterKind kind;
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
- Universal source-enable parameters for finite native channels or source lanes.
- A universal Play Mode parameter for how one patch uses native chip channels.
- Stable chip-specific parameters grouped by chip and module.
- Inactive chip parameters hidden from the Chipper UI, but not created or destroyed dynamically.
- Internal chip state and register snapshots saved in presets when needed.

The top-level Macro parameter may remain a stable automation slot, but its displayed choices and default templates should be chip-specific. For example, "Drum" on NES should resolve to pulse/triangle/noise APU behavior, while "Drum" on SN76489 should resolve to PSG attenuation and noise-register behavior. When a macro changes, the UI should update labels, segmented state, and readouts to show the effective chip settings produced by that macro. The current bridge keeps one stable macro enum for DAW automation, repopulates the visible Macro dropdown from the active chip descriptor, and writes the selected chip macro template into the stable native control parameters so the visible controls match the audible patch.

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

The current `ChipDescriptor` layer is the first implemented step toward this system. It provides chip names, summaries, macro templates, chip-specific labels and groups for the universal macro controls, implementation status, Chip Poly capability, and six stable UI module definitions per chip. The JUCE editor renders those numbered modules in a fixed shell and updates their contents from the selected descriptor. Implemented chips show live macro/native controls and live readouts; planned chips keep roadmap module text visible but disable inactive controls so the UI does not imply sound behavior that is not backed by a core. The shared Play Mode parameter is the first patch/channel control exposed globally; NES / RP2A03, Game Boy / DMG, YM2149 / AY, and SN76489 currently implement Chip Poly allocation across their finite pitched channels. Current live readouts, visible controls, and the Macro dropdown are based on the active chip descriptor and `PatchConfig`; the renderer also seeds unspecified controls from the selected macro template so CLI renders match the VST macro behavior.

Future UI work should keep expanding `ChipDescriptor` into richer module and parameter specs rather than adding chip-specific editor branches.
