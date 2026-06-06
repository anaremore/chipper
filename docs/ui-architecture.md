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

The footer/status area should include a subtle generated build label so a tester can match an installed VST3 to the reported source revision. It should also show a compact MIDI CC range badge with a tooltip for the full fixed hardware-control map, keeping MIDI discoverability close to the build/status information without crowding chip controls.

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

NES and DMG Pulse Duty use the first register-choice component. It presents a segmented button group over the stable `macroControl1` parameter because RP2A03 and DMG pulse duty are four-state register fields, not continuous values. DMG Sweep Shift uses `macroControl2` as a chip-aware slider over the `NR10` channel-1 sweep shift bits. NES Noise Period and DMG Noise Clock use `macroControl3` as chip-aware sliders over `$400E` and `NR43` noise fields, while YM2149 Noise Pitch uses the same stable parameter over the 5-bit shared noise period in register 6. SN76489 Noise Bias also uses `macroControl3`, but only when Noise Mode is Macro: it biases the macro-selected PSG noise-control register and explicit Noise Mode choices override it. YM2149 Tone/Noise Mix uses the same pattern over `macroControl4`: `Noise`, `Tone`, and `Both` map to register 7 mixer bytes rather than a fake blend. NES Noise Mode, DMG Noise Mode, DMG Wave Shape, YM2149 Envelope Shape, and SN76489 Noise Mode follow the same pattern through `ChipParameterSpec` metadata. SN76489 Noise Level uses `macroControl4` as a chip-aware slider over the 4-bit PSG attenuation register, and YM2149 Envelope Speed uses the stable `envelopeDecay` parameter as a chip-aware slider over registers 11/12, with zero preserving the macro/default period. The YM/AY envelope renderer advances a 32-count YM-style envelope counter and resets that counter on envelope-shape writes, so future envelope UI readouts can show resolved stepped chip state rather than a generic smooth decay. Future chip controls should follow the same rule: use continuous controls only for continuous behavior, segmented/stepped controls for register-like behavior, and macro controls only when they intentionally map a musical gesture to chip-native states.

Segmented controls must lay out only the legal choices for the active chip. The shared Noise Mode parameter has three visible choices for NES/DMG and five for SN76489, so the editor reflows the same button row from the active `ChipParameterSpec` instead of reserving blank maximum-capacity slots.

NES, DMG, YM2149, and SN76489 source cards are the first stable non-macro channel controls. They attach to universal `source1Enabled` through `source4Enabled` parameters and map to native source behavior: pulse 1, pulse 2, triangle/wave, and noise for APU-style chips; channel A, channel B, channel C, and shared noise for YM2149/AY; tone 1, tone 2, tone 3, and noise for SN76489. Each card also owns a compact `source1Level` through `source4Level` trim slider plus a percent/dB readout so users can balance native channels without diving into register details. The Sources module lays these as four horizontal channel strips so the enable button, trim, and readout stay visible together. The YM cards drive register 7 mixer bits and source-aware volume writes, while the SN cards drive PSG attenuation state, so muted channels cannot leak accidental constant output. Source level trims are modern continuous source gains layered over the native register model; they are intentionally labeled as levels/trims rather than perfect hardware volume registers where a chip lacks one. Future chip source cards should preserve that user promise: active cards must represent audible/native sources, not decorative labels.

Noise Mode is now chip-aware. NES/RP2A03 choices map to `$400E` bit 7 for long LFSR noise versus short-loop metallic noise, DMG choices map to `NR43` bit 3 for 15-bit versus 7-bit polynomial noise, and SN76489 choices map to the PSG noise-control bits. DMG Wave Level maps to `NR32` channel-3 output-level bits with Macro, Mute, 100%, 50%, and 25% choices, giving the Wave RAM channel a real hardware level selector instead of a generic gain slider. DMG Stereo Route lives in the Output module and maps to `NR51` choices for Macro, Both, Left, Right, and Split; the Split helper resolves to one concrete routing byte instead of a fake stereo widener. SN76489 Noise Level maps to the PSG attenuation nibble for the noise channel. YM2149 Noise Pitch maps to register 6, YM2149 Tone/Noise Mix maps to register 7 mixer bits for noise-only, tone-only, or combined tone+noise output, and YM2149 Envelope Speed maps to the 16-bit envelope period register pair feeding the stepped envelope counter. Stereo Spread is an Output-module convenience for mono-native PSG chips: YM2149/AY spreads A/B/C left/center/right, SN76489 spreads Tone 1/2/3 left/center/right and keeps noise centered, and the default zero value preserves mono chip output. In each case `Macro` preserves the chip-specific musical template and shows the resolved register value in the readout. This establishes a broader rule: macro/preset selections are per-chip musical templates, and visible readouts should reflect the macro-resolved chip state rather than only raw control offsets.

All current APVTS parameters also have fixed MIDI CC mappings. Hardware control should follow the same parameter surface as the UI and host automation: continuous parameters track the full CC range, discrete/register parameters quantize to their legal choices, source-card toggles switch at the CC midpoint, source level trims use CC84-87, DMG Wave Level uses CC92, Stereo Spread uses CC93, and DMG Stereo Route uses CC94. CC74 and macro-only host automation are event-like: changing the Musical Macro parameter applies the selected chip's macro template to native controls, source enables, source levels, envelope helpers, and chip-specific choice parameters, then users may continue tweaking individual controls. When a preset or state restore changes Macro and control values together, the processor preserves that snapshot rather than reapplying the macro template. The shared control registry is JUCE-free so the renderer, descriptor tests, and plugin all read the same default CC map. The editor appends the relevant CC number to automatable control tooltips and shows a footer CC-range badge whose tooltip lists the full map, so hardware control is discoverable without cluttering the main surface. Renderer descriptor JSON exports both per-control CC metadata and a top-level `midiCcMappings` list, giving future dynamic UIs and scripts a single machine-readable parameter map. When adding a new automatable APVTS parameter or surfacing a new chip descriptor control, add its role-to-parameter mapping, default CC, UI tooltip/readout, descriptor metadata, and regression coverage at the same time so the plugin remains fully controllable from MIDI hardware.

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

struct ChipParameterSpec
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

The editor asks the selected chip spec what modules and controls to show. The UI should not need SID, NES, or YM2612-specific register knowledge; that belongs in the chip adapter and engine mapping layer. The current implementation has started this migration with `ChipParameterSpec`, `ParameterKind`, and `ControlSurface` declarations in `ChipDescriptor`; live macro-control labels, source-card help, and segmented/register surfaces are now descriptor-driven, while planned chips intentionally omit live specs.

## Parameter Strategy

DAW automation IDs must remain stable after plugin load. Chipper should use a hybrid parameter strategy:

- Universal automatable macros for common musical performance controls.
- Universal source-enable and source-level parameters for finite native channels or source lanes.
- A universal Play Mode parameter for how one patch uses native chip channels.
- Stable chip-specific parameters grouped by chip and module.
- Inactive chip parameters hidden from the Chipper UI, but not created or destroyed dynamically.
- Internal chip state and register snapshots saved in presets when needed.

The top-level Macro parameter may remain a stable automation slot, but its displayed choices and default templates should be chip-specific. For example, "Drum" on NES should resolve to pulse/triangle/noise APU behavior, while "Drum" on SN76489 should resolve to PSG attenuation and noise-register behavior. When a macro changes, the UI should update labels, segmented state, and readouts to show the effective chip settings produced by that macro. The current bridge keeps one stable macro enum for DAW automation, repopulates the visible Macro dropdown from the active chip descriptor, and writes the selected chip macro template into the stable native control parameters so the visible controls match the audible patch.

Macro templates are now lightweight chip-specific snapshots: they can set native macro controls, source enables, envelope helper defaults, and chip-specific choice parameters such as NES Noise Mode, DMG Noise Mode, DMG Wave Shape, DMG Stereo Route, YM Envelope Shape, and SN76489 Noise Mode. Applying a macro or factory preset resets the universal source level trims to unity so users hear the curated channel balance first, then can rebalance channels manually or from MIDI CC84-87. Factory presets are curated snapshots over those same stable parameters, not separate sound engines. Each live-chip preset stores chip mode, accuracy tier, macro, play mode, native controls, source enables, chip-specific choices, and output trim. The VST applies those snapshots to APVTS parameters so DAW state remains ordinary plugin state, while `chipper_render --preset` uses the same catalog and output trim for regression coverage. Renderer debug JSON includes the active descriptor's macro, parameter, choice, output trim, and preset metadata so automated tests can verify that the UI/control vocabulary matches the engine-backed chip surface. Planned-chip preset names must stay out of the executable factory list until their cores exist.

`chipper_render --list-descriptors --debug descriptors.json` exports the same chip descriptors, macro templates, parameter specs, default MIDI CC map, and factory preset catalog without rendering audio. `chipper_render --describe-chip nes --debug nes.json` exports the same schema for one chip. This is the machine-readable contract for future dynamic UI work: chip-aware controls should be proven in descriptor metadata before the editor grows more chip-specific surfaces.

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

The current `ChipDescriptor` layer is the first implemented step toward this system. It provides chip names, summaries, macro templates, chip-specific labels and groups for the universal macro controls, implementation status, Chip Poly capability, six stable UI module definitions per chip, and first-pass `ChipParameterSpec` metadata for live controls. The JUCE editor renders those numbered modules in a fixed shell and updates their contents from the selected descriptor. Implemented chips show live macro/native controls and live readouts; planned chips keep roadmap module text visible but disable inactive controls so the UI does not imply sound behavior that is not backed by a core. The shared Play Mode parameter is the first patch/channel control exposed globally; NES / RP2A03, Game Boy / DMG, YM2149 / AY, and SN76489 currently implement Chip Poly allocation across their finite pitched channels. NES Noise Period, DMG Sweep Shift, DMG Noise Clock, and SN76489 Noise Bias now present as register-backed controls, and DMG Envelope Level presents as a register-backed envelope control because it writes the NRx2 initial volume nibble rather than acting as a generic macro. DMG Stereo Route is the first output-module chip-register selector: it writes `NR51` directly and has renderer coverage for left, right, and split routing. YM2149 and SN76489 also expose their A/B/C/noise and tone/noise source cards in the same source surface, with renderer coverage for mixer or attenuation state plus source flags; the editor now keeps each source level trim visibly paired with its channel card and CC tooltip. Both PSG cores expose Stereo Spread as an Output-module modern convenience with renderer debug coverage, while zero spread remains the authentic mono baseline. SN76489 debug JSON reports whether tone channels are in the period 0/1 constant-output edge case, whether noise is clocked from Tone 3, the resolved divider, and register-origin noise resets, so raw PSG timer/latch behavior is backed by a visible register contract. YM2149 now exposes both envelope shape and envelope speed as register-backed controls while keeping those controls in separate modules so the UI remains readable. Current live labels, tooltips, readouts, segmented register controls, the Macro dropdown, and the factory preset dropdown are based on shared catalog/descriptor data and `PatchConfig`; the renderer also seeds unspecified controls from the selected macro template or applies a named factory preset so CLI renders match the VST behavior, and it exports descriptor metadata in debug JSON for UI/engine contract tests.

Future UI work should keep expanding `ChipDescriptor` into richer module and parameter specs rather than adding chip-specific editor branches.
