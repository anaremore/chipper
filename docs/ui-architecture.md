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

The footer/status area should include a subtle generated build label so a tester can match an installed VST3 to the reported source revision. It should also show a compact MIDI CC range badge with a tooltip for the full fixed hardware-control map, keeping MIDI discoverability close to the build/status information without crowding chip controls. Long accuracy caveats belong in tooltips or docs; the visible footer text should stay short enough that build and MIDI badges never cover it.

The module contents may change, but the order should not. Users should learn where concepts live once and carry that knowledge across NES, DMG, SID, YM2149, SN76489, FM, and sampler-style modes.

## Slot Scalability

The six slots are functional zones, not six rigid equal boxes. Every chip should use the same concepts:

1. Chip Profile
2. Sources
3. Tone / Shape
4. Envelope / Level
5. Motion / Performance
6. Output / Utility

Chip manifests may choose different panel templates and layout density inside those zones. The equal six-card grid is only the default layout, not a product rule. Chips with a larger native control surface may use a chip-specific layout template while preserving the same conceptual zones and stable parameter IDs. SID is the first proof point: Profile and Voices stay near the top, Filter and Motion stay together, and the per-voice Envelope surface spans the full width because ADSR is central to using the chip. PSG, wavetable, and most sample chips fit a standard channel-bank layout. FM chips should use the same conceptual slots but need a denser operator editor or operator grid inside Tone / Shape. Sample-heavy chips such as SPC700-style and Paula modes should use sample voice banks plus sample/loop/pitch panels instead of oscillator-style controls.

The product rule is: every chip feels distinct, but no chip feels like a different plugin.

## Adaptive System Views

The adaptive UI should change structural complexity to match the selected chip's native limitations:

- **Raw 8-bit generators:** NES/RP2A03 and SN76489 should emphasize hard-edged channel strips, discrete register choices, and simple source routing. Pulse duty is a segmented register selector, not a pulse-width slider. Triangle-style channels should avoid fake volume-envelope controls when the hardware lacks them. Noise and PCM-like sources should prefer discrete period selectors, grit controls, or sample-trigger surfaces over generic pitch knobs.
- **Multi-voice analog hybrid:** SID should feel like a compact laboratory synth: three deep voice lanes, per-voice waveform/register controls, per-voice ADSR, oscillator interaction, and a global filter matrix with cutoff, resonance, mode bits, and explicit voice routing. SID waveform controls must allow the chip's combined waveform bits where the engine supports them rather than pretending each voice is a one-wave oscillator forever.
- **Two-operator FM:** OPL2/YM3812 and related FM chips should move away from oscillator strips and toward operator blocks, algorithm/feedback structure, multiplier, key-scale/rate controls, envelope stages, and graphical waveform selectors for the native mathematical shapes.

These views should reuse the same shell, typography, APVTS parameters, MIDI mapping strategy, and descriptor-driven metadata. The variation belongs in layout templates and chip-native components, not in unrelated visual skins.

## Component Strategy

Use JUCE's native plugin-safe controls for DAW-facing behavior: `ComboBox` for selectors, `Slider` for continuous automatable parameters, `Label` for readouts/status, and button-style controls for future discrete actions. Add Chipper-specific presentation components only where the chip metaphor needs structure that generic controls do not provide.

The first custom surface is the NES channel card grid inside Sources. It keeps the stable six-slot shell, but makes Pulse 1, Pulse 2, Triangle, and Noise / DMC visible as chip channels. The fourth card gates the native noise source and the DMC sample lane when external bytes are supplied; its level trim remains noise-only because DMC output follows the native DAC path. Similar chip surfaces should follow this pattern: PSG channel banks, FM operator grids, sampler voice banks, wavetable editors, and macro/readout tiles. These components should reflect stable parameters and tested engine behavior rather than presenting fake controls for planned features.

NES and DMG Pulse Duty use the first register-choice component. It presents a segmented button group over the stable `macroControl1` parameter because RP2A03 and DMG pulse duty are four-state register fields, not continuous values. NES also exposes `pulse2Duty` as a dedicated segmented register choice for the second pulse channel; `Follow` preserves the current musical template, while explicit choices write the channel-2 duty bits independently and show the resolved register bits in the live readout. DMG Sweep Shift uses `macroControl2` as a chip-aware slider over the `NR10` channel-1 sweep shift bits. NES Noise Period and DMG Noise Clock use `macroControl3` as chip-aware sliders over `$400E` and `NR43` noise fields, while YM2149 Noise Pitch uses the same stable parameter over the 5-bit shared noise period in register 6.

SID exposes three independent oscillator voices through the source cards. Voice 1 Pulse Width uses `macroControl1`; Voice 2 and Voice 3 have dedicated 12-bit pulse-width sliders/readouts mapped to their own SID register pairs. SID Cutoff uses `macroControl3`, Resonance uses `stereoSpread` as a SID-specific `$D417` register role, and the SID Filter Mode control uses a dropdown because the nine `$D418` mode-bit choices are too dense for readable segmented buttons. SID Filter Routing also uses a compact menu because its nine `$D417` low-bit choices are too wide for a segmented row.

SID Sustain uses `macroControl4` as the musical SR sustain source, SID ADSR Speed uses `envelopeDecay` for macro attack/decay/release templates, and each SID voice has its own Attack, Decay, Sustain, and Release stepped vertical override controls for `Follow` plus exact 0-15 AD/SR nibbles. Each Sustain control's `Follow` choice uses the `macroControl4` Sustain slider, while explicit values write that voice's SR high nibble directly. The compact value labels show `F->n` when a lane follows the macro-resolved nibble and `n` when a lane is explicitly pinned, so users can read the effective register state without opening dropdowns or hovering tooltips. Readouts also show the resolved V1/V2/V3 AD/SR register bytes and A/D/S/R nibbles so both the shared musical control and exact per-voice overrides feel chip-aware.

Full ADSR controls belong on chips with native ADSR or operator-envelope behavior, such as SID and FM chips; PSG/APU modes should expose their native envelope, length, attenuation, or decay controls instead of a generic ADSR panel. SID Voice 1 Wave uses the stable `waveShape` parameter, while SID Voice 2 Wave and Voice 3 Wave use dedicated stable menu parameters so the UI can expose independent oscillator waveform choices without asking users to edit control-register bytes. `Follow` uses the selected SID template or Voice 1 choice; explicit Triangle, Saw, Pulse, and Noise choices map to each voice's SID control-register waveform bits.

SN76489 Noise Bias also uses `macroControl3`, but only when Noise Mode is Follow: it biases the macro-selected PSG noise-control register and explicit Noise Mode choices override it. YM2149 Tone/Noise Mix uses the same pattern over `macroControl4`: `Noise`, `Tone`, and `Both` map to register 7 mixer bytes rather than a fake blend. NES Noise Mode, NES Pulse 2 Duty, DMG Noise Mode, DMG Wave Shape, SID ADSR nibble menus, SID per-voice waveform menus, SID Filter Mode, SID Filter Routing, YM2149 Envelope Shape, and SN76489 Noise Mode follow the same pattern through `ChipParameterSpec` metadata. Future chip controls should follow the same rule: use continuous controls only for continuous behavior, segmented/stepped controls for register-like behavior, dropdowns for wide register choices, and macro controls only when they intentionally map a musical gesture to chip-native states.

YM2149 Envelope Shape is a compact register menu over the shared CC90 parameter. It keeps the musical shortcuts `Fixed`, `Fall`, `Rise`, `Saw`, and `Triangle`, then exposes exact AY/YM register-13 shape codes `0x00` through `0x0F` for users who want hardware-level control. YM2149 Channel Mix is the first compact per-channel register menu surface. Channel A, B, and C each expose `Follow`, `Tone`, `Noise`, `Both`, and `Off`; `Follow` uses the global Tone/Noise Mix, while explicit choices override that channel's register 7 tone/noise enable bits. The UI readout reports the resolved mixer byte and A/B/C choices, and MIDI CC101-103 plus renderer flags use the same APVTS parameters.

Segmented controls must lay out only the legal choices for the active chip. The shared Noise Mode parameter has three visible choices for NES/DMG and five for SN76489. The shared YM Envelope Shape / SID Filter Mode parameter uses dropdowns for YM's full hardware envelope shape set and SID's nine `$D418` filter-bit choices. The editor reflows each row from the active `ChipParameterSpec` instead of reserving blank maximum-capacity slots.

NES, DMG, SID, YM2149, and SN76489 source cards are the first stable non-macro channel controls. They attach to universal `source1Enabled` through `source4Enabled` parameters and map to native source behavior: pulse 1, pulse 2, triangle/wave, and noise/DMC for APU-style chips; voice 1, voice 2, and voice 3 for SID; channel A, channel B, channel C, and shared noise for YM2149/AY; tone 1, tone 2, tone 3, and noise for SN76489. Each card also owns a compact source-level trim slider plus an explicit Level row with a percent/dB readout so users can balance native channels without diving into register details. For NES, source 4 enable gates both noise and the renderer-backed external DPCM lane, while source 4 level is deliberately a noise trim; `$4011` direct DAC level and DPCM byte stepping live in the DMC path. SID intentionally exposes only three source cards because the external input path is not implemented yet, and those three lanes expand to fill the available Sources module. SID source cards now host the existing per-voice waveform menus directly inside each lane, so a voice's enable, waveform register choice, trim, resolved waveform label, and CC tooltip stay together. Source cards also include compact state scopes for implemented multi-source chips: NES/DMG pulse cards show duty-resolved pulse shapes, triangle/wave cards show the appropriate native shape, SID voices show triangle/saw/pulse/noise with pulse width following the 12-bit register, YM cards show tone/noise mix state, and SN cards show tone/noise sources. SID's Envelope module uses the same state-scope idea for three resolved ADSR curves, drawing each voice's current attack, decay, sustain, and release nibbles with the same timing table used by the SID core approximation. These state scopes are not realtime audio oscilloscopes; they are honest visualizations of the chip state the engine is asked to generate. The Performance Output cell adds the first audio-derived scope: the processor writes a small post-trim mono output history through allocation-free atomics, and the editor snapshots it on the UI timer to draw the actual final plugin output. In SID mode, the Filter module prioritizes cutoff, resonance, `$D418` filter mode, and `$D417` voice routing because waveform selectors already live in the voice cards. This is the intended flexibility model: reuse the same component and APVTS/MIDI parameter surface, but allow each chip to reflow controls into the places musicians expect for that hardware. Multi-oscillator chips should reuse the same state-scope pattern where it adds clarity, chips with native envelope/operator stages should use the envelope-scope pattern where it helps users understand register timing, and audio-derived scopes should use processor telemetry rather than peeking into chip internals from the editor. SID source-card labels mirror the resolved voice waveform from the register-backed waveform controls, while tooltips include the resolved control-register waveform byte and that voice's AD/SR nibble state so macros and presets read as instrument state without hiding the emulation contract. The Sources module lays available channel strips so the enable button, trim, and readout stay visible together. The YM cards drive register 7 mixer bits and source-aware volume writes, while the SN cards drive PSG attenuation state, so muted channels cannot leak accidental constant output. Source level trims are modern continuous source gains layered over the native register model; they are intentionally labeled as levels/trims rather than perfect hardware volume registers where a chip lacks one. Future chip source cards should preserve that user promise: active cards must represent audible/native sources, not decorative labels.

Noise Mode is now chip-aware. NES/RP2A03 choices map to `$400E` bit 7 for long LFSR noise versus short-loop metallic noise, DMG choices map to `NR43` bit 3 for 15-bit versus 7-bit polynomial noise, and SN76489 choices map to the PSG noise-control bits. SID Osc Interaction reuses the same stable APVTS/CC91 slot in SID mode, but its descriptor relabels it as a Motion-module segmented register control for Off, Sync, Ring, and Both choices that write the voice control-register sync/ring bits. DMG Wave Level maps to `NR32` channel-3 output-level bits with Follow, Mute, 100%, 50%, and 25% choices, giving the Wave RAM channel a real hardware level selector instead of a generic gain slider. DMG Stereo Route lives in the Output module and maps to `NR51` choices for Follow, Both, Left, Right, and Split; the Split helper resolves to one concrete routing byte instead of a fake stereo widener. SID Model reuses the same stable APVTS/CC94 slot in SID mode as a Profile-module segmented selector for Auto, 6581, and 8580; it changes Chipper's partial filter curve and output-drive profile, not the underlying SID register bytes. SN76489 Noise Level maps to the PSG attenuation nibble for the noise channel. YM2149 Noise Pitch maps to register 6, YM2149 Tone/Noise Mix maps to register 7 mixer bits for noise-only, tone-only, or combined tone+noise output, and YM2149 Envelope Speed maps to the 16-bit envelope period register pair feeding the stepped envelope counter. Stereo Spread is an Output-module convenience for mono-native PSG chips: YM2149/AY spreads A/B/C left/center/right, SN76489 spreads Tone 1/2/3 left/center/right and keeps noise centered, and the default zero value preserves mono chip output. In each case `Follow` or `Auto` preserves the chip-specific musical template and shows the resolved register value in the readout. This establishes a broader rule: macro/preset selections are per-chip musical templates, and visible readouts should reflect the macro-resolved chip state rather than only raw control offsets.

All current APVTS parameters also have fixed MIDI CC mappings. Hardware control should follow the same parameter surface as the UI and host automation: continuous parameters track the full CC range, discrete/register parameters quantize to their legal choices, source-card toggles switch at the CC midpoint, source level trims use CC84-87, SID Osc Interaction and shared Noise Mode use CC91, DMG Wave Level uses CC92, Stereo Spread/SID Resonance uses CC93, DMG Stereo Route/SID Model uses CC94, SID Voice 2/3 Waveform use CC95/CC96, SID Voice 1 Attack/Release use CC97/CC98, SID Voice 1 Decay uses CC99, SID Voice 1 Sustain uses CC100, YM2149 Channel A/B/C Mix use CC101-103, SID Voice 2 ADSR uses CC105-108, SID Voice 3 ADSR uses CC109-112, SID Filter Routing uses CC113, SID Voice 2/3 Pulse Width use CC114/CC115, NES DMC Loop uses CC68 for the `$4010` loop bit, NES DMC Direct Level uses CC116 for the `$4011` 7-bit DAC load, NES DMC Sample Slot uses CC117 to select a preloaded `.dmc` bank slot, NES DMC Rate uses CC118 for the `$4010` DMC rate index, NES DMC Playback Mode uses CC119 for Manual Slot, Note Map, and Sample Map Only, and NES DMC Map Root uses CC69 to choose the first mapped MIDI note. NES mode surfaces DMC Direct, DMC Rate, Loop, Playback Mode, Map Root, and a compact DMC sample bank control in the Performance strip because those are more musical than a generic clock override during normal patch design; the global clock parameter remains available to host automation and MIDI. The VST can load a single `.dmc` file as a one-slot bank or stage a folder of `.dmc` files into a local checklist; checked entries form up to 32 active in-memory slots, then the active sample dropdown and CC117 select the same bank slot. The bank editor includes First 32, Invert, and Clear actions so a user can quickly curate large folders without loading, copying, or committing sample content. In Note Map, MIDI notes from the DMC Map Root upward select checked slots on the monophonic DMC lane without changing the manual slot parameter. Sample Map Only uses the same mapping but suppresses the NES pulse, triangle, and noise generators so the bank behaves as a focused DMC sample keyboard; this is a VST performance convenience, not a hardware register mode. The DMC sample status line stays compact but reports selected or mapped bytes, estimated playback duration, rate index, one-shot/loop state, and mapped key span; its tooltip adds bit count, bit-clock, and loop-bit detail so users can diagnose rate mismatch without leaving the instrument. Plugin state stores DMC paths and checked states, then reloads readable files on host project restore; it intentionally does not embed sample bytes. WAV-to-DMC conversion remains planned, not active in this build. The command-line renderer can step one external `.dmc` path at a selected DMC rate index, `$4010` loop-bit setting, and optional DMC-only channel mask today. CC74 and macro-only host automation are event-like: changing the Musical Macro parameter applies the selected chip's macro template to native controls, source enables, source levels, envelope helpers, and chip-specific choice parameters, then users may continue tweaking individual controls. When a preset or state restore changes Macro and control values together, the processor preserves that snapshot rather than reapplying the macro template. The shared control registry is JUCE-free so the renderer, descriptor tests, and plugin all read the same default CC map. The editor appends the relevant CC number to automatable control tooltips and shows a footer CC-range badge whose tooltip lists the full map, so hardware control is discoverable without cluttering the main surface. Renderer descriptor JSON exports both per-control CC metadata and a top-level `midiCcMappings` list, giving future dynamic UIs and scripts a single machine-readable parameter map. When adding a new automatable APVTS parameter or surfacing a new chip descriptor control, add its role-to-parameter mapping, default CC, UI tooltip/readout, descriptor metadata, and regression coverage at the same time so the plugin remains fully controllable from MIDI hardware.

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

The top-level Macro parameter may remain a stable automation slot, but its displayed choices and default templates should be chip-specific. For example, "Drum" on NES should resolve to pulse/triangle/noise APU behavior, while "Drum" on SN76489 should resolve to PSG attenuation and noise-register behavior. When a macro changes, the UI should update labels, segmented state, and readouts to show the effective chip settings produced by that macro. The current bridge keeps one stable macro enum for DAW automation, repopulates the visible Macro dropdown from the active chip descriptor, and writes the selected chip macro template into the stable native control parameters so the visible controls match the audible patch. Planned chips also provide chip-specific roadmap templates, but their readouts explicitly say the audio core is not integrated yet.

The compact Performance strip is the current patch-state surface, not a separate hidden control bank. It should prefer short labels, chip-aware readouts, and tooltips over explanatory prose. When a factory preset is selected, the strip names that preset before the resolved macro/register summary so users can see that the visible controls represent a preset snapshot rather than an abstract macro label alone.

SID keeps oscillator waveform menus in the three voice cards because waveform selection is a per-voice register decision. The SID Filter module should therefore keep cutoff, resonance, filter mode, and voice routing together instead of repeating a stale voice-wave summary or scattering filter controls into the performance strip. SID ADSR is per voice, so overrides need a roomy primary surface with explicit Attack, Decay, Sustain, and Release controls for each voice plus envelope previews. That surface should be treated as a full-width SID panel rather than squeezed into the default half-width grid. Output monitoring reserves enough height for the final audio scope to be legible as a waveform, not a decorative trace, with subtle amplitude guide lines and the post-trim waveform in the same Performance strip as the output level.

Macro templates are now lightweight chip-specific snapshots: they can set native macro controls, source enables, envelope helper defaults, resonance/spread helpers, and chip-specific choice parameters such as NES Pulse 2 Duty, NES Noise Mode, DMG Noise Mode, DMG Wave Shape, DMG Stereo Route, SID Filter Mode, SID Filter Routing, SID Osc Interaction, SID Model, YM Envelope Shape, and SN76489 Noise Mode. Applying a macro resets the universal source level trims to unity, resets inherited register choices to `Follow`, resets SID Voice 2/3 Pulse Width to the selected template's pulse-width value, and keeps explicit overrides available afterward, so users hear the curated voice/envelope/mixer stack first and can then override individual NES pulse duty, SID oscillator waveforms, filter routes, per-voice pulse width, per-voice ADSR nibbles, or YM channel mixer states manually or from MIDI. Factory presets are curated snapshots over those same stable parameters, not separate sound engines, and may pin source-level balances, DMG Wave Level, SID Voice 2/3 Waveform, SID Voice 2/3 Pulse Width, `$D418` Filter Mode, `$D417` Filter Routing, and per-voice SID ADSR nibble menus when a preset needs a specific mixed oscillator/filter/envelope stack. Each live-chip preset stores chip mode, accuracy tier, macro, play mode, native controls, source enables, source-level trims, chip-specific choices, DMG Wave Level, SID voice-wave/pulse-width/filter-routing/ADSR overrides where needed, and output trim. The VST applies those snapshots to APVTS parameters so DAW state remains ordinary plugin state, while `chipper_render --preset` uses the same catalog and output trim for regression coverage. Renderer debug JSON includes the active descriptor's macro, parameter, choice, output trim, and preset metadata so automated tests can verify that the UI/control vocabulary matches the engine-backed chip surface. Planned-chip preset names must stay out of the executable factory list until their cores exist.

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

- NES / RP2A03: Sources are Pulse 1, Pulse 2, Triangle, and a combined Noise / DMC lane; Tone becomes Shape / Mixer.
- Game Boy / DMG: Sources are Pulse 1, Pulse 2, Wave, Noise; Tone becomes Wave / Noise.
- SID / C64: Sources are Voice 1-3; Tone becomes Filter.
- YM2149 / AY: Sources are A, B, C, shared noise; Tone becomes Mixer / Envelope.
- SN76489: Sources are Tone 1-3 and Noise; Tone becomes Tone / Crunch.
- YM2612 and OPL: Sources are FM voices; Tone becomes Operators.

Authentic mode should expose chip-native behavior. Hybrid mode can add musical helpers. Inspired mode should simplify controls and clearly avoid accuracy claims. The requested Accuracy selector is a user preference, not a proof badge. Each chip descriptor also carries a verification disclosure with a badge, evidence text, known gaps, and explicit `hardwareValidated` / `cycleAccurate` booleans. The UI header/footer and renderer descriptor JSON must use that disclosure so a user can request Authentic behavior while still seeing "Verified partial" or "Unverified planned" when that is the honest implementation state.

## Current Bridge

The current `ChipDescriptor` layer is the first implemented step toward this system. It provides chip names, summaries, macro templates, chip-specific labels and groups for the universal macro controls, implementation status, verification disclosure, Chip Poly capability, six stable UI module definitions per chip, and first-pass `ChipParameterSpec` metadata for live controls. The JUCE editor renders those numbered concepts with a default six-card grid, but can switch to chip-specific layout templates when a chip needs more room for native controls. Implemented chips show live macro/native controls and live readouts; planned chips keep roadmap module text and chip-specific roadmap templates visible but disable inactive controls so the UI does not imply sound behavior that is not backed by a core. The shared Play Mode parameter is the first patch/channel control exposed globally; NES / RP2A03, Game Boy / DMG, SID / C64, YM2149 / AY, and SN76489 currently implement Chip Poly allocation across their finite pitched channels.

NES Pulse 1 Duty, NES Pulse 2 Duty, NES Noise Period, DMG Sweep Shift, DMG Noise Clock, SID Pulse Width/Cutoff/Filter Mode/Filter Routing/Resonance/Sustain/ADSR Speed/per-voice ADSR/per-voice Waveform/Osc Interaction/Model, YM2149 per-channel mixer choices, and SN76489 Noise Bias now present as register-backed controls, and DMG Envelope Level presents as a register-backed envelope control because it writes the NRx2 initial volume nibble rather than acting as a generic macro. Each SID voice has independent pulse width: Voice 1 uses the shared native pulse-width control, while Voice 2 and Voice 3 use dedicated APVTS/MIDI controls mapped to their own 12-bit register pairs. Each SID voice's Attack, Decay, Sustain, and Release controls live beside ADSR Speed as stepped vertical overrides over that voice's AD/SR nibbles; `Follow` uses the musical ADSR Speed mapping for attack/decay/release and the Sustain slider for sustain, while explicit values write exact 0-15 nibbles. Compact `F->n` labels show the macro-resolved nibble, and renderer JSON reports the resolved per-voice choices/register bytes.

SID per-voice waveform controls live in the voice cards as compact Voice 1/2/3 menus and write each voice's control-register waveform bits, while the readout reports the resolved hex bit value for every voice. SID Osc Interaction lives in the Motion module and writes the control-register sync/ring bits for follower voices while the debug renderer reports the resolved bitfield. SID Model lives in the Profile module and reports the resolved 6581/8580 model number plus variant filter/output coefficients in renderer JSON; this is a partial profile, not a hardware-validated analog model, and the UI labels the macro-resolved model state as Auto. SID filter controls currently expose cutoff, resonance, `$D418` LP/BP/HP/off plus combined mode-bit outputs such as notch through a dropdown, and `$D417` voice filter routing through a menu in the Filter module; external input routing and hardware-calibrated analog response remain future SID filter work.

DMG Stereo Route is the first output-module chip-register selector: it writes `NR51` directly and has renderer coverage for left, right, and split routing. SID, YM2149, and SN76489 also expose their native source cards in the same source surface, with renderer coverage for source flags and register state; the editor now keeps each source level trim visibly paired with its channel card and CC tooltip. Both PSG cores expose Stereo Spread as an Output-module modern convenience with renderer debug coverage, while zero spread remains the authentic mono baseline; SID reuses the same stable CC93/APVTS role as a chip-specific resonance control instead. SN76489 debug JSON reports whether tone channels are in the period 0/1 constant-output edge case, whether noise is clocked from Tone 3, the resolved divider, and register-origin noise resets, so raw PSG timer/latch behavior is backed by a visible register contract. YM2149 now exposes envelope shape, envelope speed, global tone/noise mix, and A/B/C mixer overrides as register-backed controls while keeping those controls in compact module surfaces so the UI remains readable. Current live labels, tooltips, readouts, register controls, verification badges, the Macro dropdown, and the factory preset dropdown are based on shared catalog/descriptor data and `PatchConfig`; the renderer also seeds unspecified controls from the selected macro template or applies a named factory preset so CLI renders match the VST behavior, and it exports descriptor metadata in debug JSON for UI/engine contract tests.

Future UI work should keep expanding `ChipDescriptor` into richer module, parameter, and layout specs. Small chip-specific editor branches are acceptable as temporary scaffolding when they prove a layout template that should later become data-driven.
