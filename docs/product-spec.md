# Chipper Product Spec

Review status: planning docs synced on 2026-06-14 after the fixed-regression and helper-envelope cleanup. The focused release gate passed 26/26 on 2026-06-14. FM held-note fade-out, NES DMC one-shot loop-off, and misleading helper-envelope wording are release-gate concerns, not active feature requests, unless reproduced in the current build. Current product evolution should prioritize chip-aware source editors, preset/state workflows, truthful verification, and release confidence.

## Concise Product Spec

Chipper is a classic chip synth VST3 focused on iconic console, computer, and arcade sound chips. It should feel authentic enough for chiptune users while staying immediate for modern producers who want a usable lead, bass, arp, drum, blip, laser, coin, jump, or explosion sound in seconds.

The plugin must not use generic retro oscillators with famous chip labels. Each mode is backed by either a compatible-license emulation core or a clean-room register-level model. Modes without register/timing verification are labeled chip-inspired.

- **NES/RP2A03:** two pulse channels, triangle, noise, DMC behavior, nonlinear mixer.
- **Game Boy/DMG:** two pulse channels, wave RAM channel, noise, envelopes, length, sweep.
- **SID/C64:** 6581/8580 variants, three oscillator voices, pulse/saw/triangle/noise, PWM, sync/ring mod, ADSR quirks, filter, resonance, drive.
- **YM2149/AY:** three square tone channels backed by `emu2149`, shared noise, mixer bits, hardware envelope shapes, buzzy arcade/computer character.
- **SN76489:** three tone channels, noise modes, attenuation, Sega PSG behavior. The current partial core is backed by vendored MIT emu76489.
- **YM2612/OPN2:** six-channel FM, operators, algorithms, feedback, LFO, DAC behavior, stereo.
- **OPL2/OPL3:** two-operator FM, rhythm mode, waveform variants, envelopes, stereo for OPL3.
- **Expansion:** SPC700-style sample playback, Paula, HuC6280 wavetable, Namco WSG, YM2151, YM2413/OPLL, Konami SCC. SPC700-style, POKEY, Paula, HuC6280, Namco WSG, YM2413/OPLL, and SCC have started as audible partial expansion cores; YM2413/OPLL is backed by vendored MIT emu2413.

Behavior strictness levels exposed in the instrument:

- **Inspired:** musical approximation with smoothed edges.
- **Hybrid:** default chip flavor with modern playability.
- **Authentic:** register/channel/timing behavior closely follows the original chip where implemented.

Verification labels are separate from that control. The footer, renderer debug JSON, and accuracy docs describe what is actually proven: planned, partial, verified partial, or eventually cycle-accurate when accepted test suites or real hardware captures justify it. A chip can run in Authentic strictness and still truthfully report **Verified partial** until the implementation, tests, emulator comparisons, and/or hardware captures justify a stronger claim.

## Parameter List

- Chip Mode: NES/RP2A03, Game Boy/DMG, SID/C64, YM2149/AY, SN76489, YM2612, OPL2/OPL3, SPC700, POKEY, Paula, HuC6280, Namco WSG, YM2151, YM2413/OPLL, SCC
- Strictness: Inspired, Hybrid, Authentic. This is a requested behavior mode, not the verification claim; footer badges and renderer debug JSON report what is actually proven. The stable host/MIDI parameter remains named `Behavior Strictness`.
- Play Mode: Big Mono, Chip Poly where the selected chip has tested finite-channel allocation; Manual and Clone are reserved until tracker routing and hybrid engine cloning are implemented.
- Output Level
- Preset Recipe: internal chip-native defaults used by factory/user presets for Coin, Bass, Lead, Arp, Drum, Hit, Laser, Jump, and Power-Up style sounds. The user-facing workflow should still say **Preset**; "recipe" is internal compatibility wording for the stable APVTS/CC parameter.
- Clock Rate override and model variant where applicable
- Register-backed channel controls for each implemented chip
- Duty, noise mode, sweep, length/envelope controls for pulse/noise chips
- Operator, algorithm, feedback, per-operator level trim/multiplier/attack-rate/decay-rate/sustain-rate/release-rate, LFO, DAC, and stereo controls for FM chips
- Sample slot, rate, loop, and bit-depth controls for sampler chips
- Wavetable index, wave RAM/SCC waveform, and channel mix controls for wavetable chips
- Arp, retrigger, pitch motion, vibrato, glide, output trim
- Debug/validation export controls in the renderer, not the VST audio thread

All current VST parameters have fixed default MIDI CC mappings. Chip-aware controls use the same stable APVTS parameters for UI, host automation, preset recall, and MIDI CC control; register-style choices quantize to legal chip values instead of exposing fake continuous states. Current examples include NES Pulse 2 Duty override bits, DMG Pulse 2 Duty / `NR21` override bits, NES `$4011` DMC Direct Level, shared Sample Slot control for manual DMC/SPC700/Paula bank browsing, per-voice Sample Slot pins for SPC700 and Paula channel banks, NES `$4010` DMC Rate for matching DPCM sample clocking, NES `$4010` DMC Loop for one-shot versus looped playback, shared Sample Playback Mode for Manual Slot, mapped sample-bank playback, and NES Sample Map Only playback, shared Sample Bank Root for choosing the first MIDI note of the sample-bank map, SPC700 Loop Start/End controls for explicit normalized sample-loop ranges, DMG `NR32` Wave Level, the shared CC94 `Chip Choice / Route` slot for mode-specific choices such as DMG `NR51` Stereo Route, SID Model, SPC700/Paula sample lifetime, HuC6280 LFO mode, POKEY AUDCTL pairing, FM pan/routing, or final wavetable lane choices, independent SID 12-bit pulse-width controls for voices 1-3, SID `$D418` Filter Mode choices including combined LP/BP/HP bit outputs, per-voice SID AD/SR attack/decay/sustain/release nibble overrides, SID voice control-register sync/ring Osc Interaction, SID `$D417` Resonance, SID `$D417` voice filter routing, YM2149 register 7 per-channel A/B/C tone/noise mixer overrides, YM2612/YM2151 per-operator Level trims that offset native FM total-level registers around the preset-resolved value, YM2612/YM2151 per-operator Multiplier choices that write legal native multiplier nibbles while `Follow` preserves Operator Tone, YM2612/YM2151 per-operator Attack Rate choices that write legal native AR values while `Follow` preserves the selected Envelope Shape, YM2612/YM2151 per-operator Decay Rate choices that write legal native D1R/decay values while `Follow` preserves the selected Envelope Shape, YM2612/YM2151 per-operator Sustain Rate choices that write legal native D2R values while `Follow` preserves the selected Envelope Shape, and YM2612/YM2151 per-operator Release Rate choices that write legal native RR values while preserving the preset-resolved sustain-level nibble. Inherited register choices should be shown as chip-specific follow states such as `Preset` or `Follow`, not `Macro`, so users understand that the patch is following preset-resolved chip state rather than selecting zero/off/none. Envelope UI uses honest chip taxonomy: native ADSR/EG for SID, S-DSP/SPC700, and FM/operator chips; native envelope controls for NES, DMG, and AY/YM2149; and clearly labeled Chipper amp-envelope helpers for attenuation, sample, or wavetable chips that do not own ADSR hardware.

Controls should only become visible when they have an engine mapping, stable parameter identity, preset/save-state behavior, and at least targeted renderer or descriptor coverage. Placeholder prose can describe planned chip work, but fake knobs or empty helper modules are worse than honest absence. Planning docs follow the same rule: active tasks describe the next user-visible capability, while fixed regressions point to their release gate in `docs/release-builds.md`.

Retired or unknown chip IDs are migration fallbacks only. They should never be advertised as selectable instruments, preset categories, or generic "custom" engines; users should always land on a named chip family with a truthful implementation status.

## Preset Categories

- NES Leads
- NES Bass
- NES Drums
- DMG Pulse
- DMG Wave
- SID Bass
- SID PWM Leads
- SID FX
- YM Beeps
- YM Arps
- SN76489 / Sega PSG
- SNES SPC700-style
- Atari POKEY
- Amiga Paula
- PC Engine HuC6280
- Namco WSG
- Konami SCC
- Genesis FM
- DOS FM
- Arcade FM
- Tracker Samples
- Arcade UI
- Classic Game SFX
- Modern Chip Hybrid

## Factory Preset Catalog

Factory presets should prioritize implemented chips and avoid implying that planned chips already have audio cores. The current executable factory presets include:

Unimplemented chip modes should not expose non-audible presets. If a chip has only roadmap metadata, keep that metadata in docs/descriptors and avoid presenting it as a playable factory preset until the chip has an audited or clean-room audio core.

The authoritative current preset list is the renderer-exported catalog, not this prose list:

```powershell
chipper_render --list-presets --debug presets.json
chipper_render --list-presets --chip sid --debug sid-presets.json
```

Every shipped factory preset must appear in that catalog, include chip/category/note metadata, map to an implemented chip, and pass the automated factory-preset audibility render. New presets should be original Chipper parameter snapshots; do not import third-party preset banks, song patches, module data, DMC samples, or tracker instrument data without a separate license and rights review.

- NES Hero Pulse
- NES Triangle Bass
- NES Noise Snare
- NES Coin Blip
- NES Boss Damage
- NES Power-Up Rise
- NES Thin Duty Pluck
- NES Duty Arp Runner
- NES Triangle Kick
- NES Menu Confirm
- DMG Wave Bass
- DMG Pocket Arp
- DMG Noise Hat
- DMG Power-Up Rise
- DMG Pulse Lead
- DMG Wave Glass
- DMG Noise Kick
- DMG Split Chord
- SID Dirty Bass
- SID PWM Lead
- SID Robot Arp
- SID Noise Drum
- SID Filter Pluck
- SID 8580 Round Bass
- SID Sync Saw Lead
- SID Ring Bell
- SID Noise Riser
- SID Notch PWM Keys
- SID LP+BP Sweep Stack
- SID BP+HP Metal Bell
- SID All-Mode Growl
- YM Three-Voice Arp
- YM Fake Chord Stack
- YM Menu Beep
- YM Noise Hat
- YM Bright Beep
- YM Envelope Bell
- YM Arcade Clang
- YM Fast Minor Arp
- Sega PSG Lead
- Sega PSG Coin
- Sega PSG Arcade Laser
- PSG Noise Hit
- PSG Tone Stack
- PSG Arcade Bass
- PSG Periodic Zap
- PSG Warning Alarm
- SPC700 Soft Bass
- SPC700 Bell Lead
- SPC700 Voice Arp
- SPC700 Drum Map
- HuC6280 Glass Lead
- HuC6280 Wave Bass
- HuC6280 Six-Wave Arp
- HuC6280 Noise Tap
- Paula Tracker Bass
- Paula Chip Lead
- Paula Tracker Arp
- Paula Noise Tick
- Namco Arcade Lead
- Namco Wave Bass
- Namco Tracker Arp
- Namco Wave Tick
- SCC Arcade Lead
- SCC Wave Bass
- SCC Five-Voice Arp
- SCC Wave Tick
- OPN2 Feedback Bass
- OPN2 Metallic Lead
- OPN2 Fake Chord Arp
- OPN2 UI Chime
- OPN2 Damage Hit
- OPN2 Pitch Laser
- OPN2 Power Rise
- OPN2 Rubber Bass
- OPN2 Brass Stack
- OPN2 DAC Kick
- OPL2 Bass
- OPL2 Bright Lead
- OPL2 Organ Arp
- OPL2 UI Bell
- OPL2 Laser
- OPL2 Power Rise
- OPL2 Velvet Keys
- OPL2 Crunch Hit
- OPL2 Rhythm Kit
- OPL2 Rhythm Impact
- OPM Arcade Bass
- OPM Metallic Lead
- OPM Arcade Arp
- OPM UI Chime
- OPM Damage Hit
- OPM Laser Sweep
- OPM Power Sweep
- OPM Glass Keys
- OPM Warning Stab
- OPM Hollow Pad
- OPM Noise Zap
- OPLL UI Chime
- OPLL Preset Bass
- OPLL Brass Lead
- OPLL Organ Arp
- OPLL Impact Bell
- OPLL Power Organ
- OPLL Soft Marimba
- OPLL Robot Alert
- OPLL Slow Choir
- OPLL Rhythm Starter
- OPLL Soft Keys

Roadmap preset targets, kept as product direction until their chip cores exist:

- None at the moment. YM2612/OPN2 and OPL2/OPL3 now have audible verified-partial cores and renderer-exported factory preset catalogs. Future preset targets should be added only after confirming the chip has an audible engine path, stable state recall, and clean provenance.

## DSP Architecture Outline

- `AudioProcessorValueTreeState` owns all DAW-facing parameters with stable IDs.
- A chip-core interface owns clock rate, register writes, note helper mapping, sample rendering, debug JSON, and verification status.
- The VST and command-line renderer use the same chip-core interface.
- Clean-room cores use register-level state where practical.
- Third-party cores must pass a license audit before vendoring.
- The VST maps musical controls to register writes and mode-specific helpers rather than drawing audio from generic oscillators.
- One plugin instance owns one patch. That patch may use all native chip channels. Big Mono stacks channels for one playable sound; Chip Poly allocates notes onto finite chip channels where implemented; Manual and Clone are reserved for tracker-style routing and hybrid cloned-poly behavior.
- Host tempo is read from the playhead. The arp emits note/register events at sample offsets inside the block.
- Post rendering applies only honest modern conveniences: output trim, optional safety limiting/DC cleanup, and labeled hybrid effects.
- State save/restore uses APVTS XML plus an internal register replay snapshot for the active core where practical.
- User preset files remain flat `.chipperpreset` XML files. The browser scans the default Chipper Presets folder recursively, treats subfolders as user banks, and combines those banks with metadata search, tag filters, and local favorites. Presets can reference external NES DMC, SPC700, and Paula sample files by path; missing sample references restore non-fatally and surface in the relevant sample status line until the user loads a valid replacement.
- SPC700-style sample banks accept renderer-loaded BRR data and plugin-loaded BRR/WAV/AIFF-style bank material through the current partial sample path. The VST treats all eight voices as independent sample lanes: per-voice sample-slot pins live in each voice card, while the shared Sample Bank section handles manual slot selection, note mapping, loop controls, waveform preview, and missing-file restore status. Shared Bank uses the global manual slot or note map; slot pins let voices 1-8 hold different loaded samples at the same time. The renderer exposes the same voice-local behavior through `--spc700-brr-bank-hex`, the shared `--spc700-sample-slot`, and `--spc700-sample-slot1..8` for BRR bank regression coverage.
- Paula sample banks accept WAV, AIFF, uncompressed IFF/8SVX, and first-pass ProTracker MOD sample extraction. The Paula UI treats all four DAC channels as independent sample voices: generated sample-shape controls and optional loaded sample-slot pins live in each channel card instead of a global sample-shape selector, while the shared Sample Bank section handles external file/folder/bank loading, note-map/manual playback, waveform preview, missing-file restore status, and imported uncompressed 8SVX, WAV `smpl`, AIFF `MARK`/`INST`, and MOD repeat loop markers. Shared Bank uses the global manual slot or note map; slot pins let channels 1-4 keep different loaded samples at the same time. The renderer exposes the same channel-local behavior through `--paula-sample`, `--paula-shape1..4`, and `--paula-sample-slot1..4` for WAV/8SVX regression coverage. Compressed 8SVX, full MOD playback/effects, and tracker effect metadata remain planned.

## UI Layout Plan

Current single-screen shell:

- **Header:** ASCII logo, preset browser with search/filter/favorites, Load/Save/Save As, Chip Mode, Strictness, and Play Mode.
- **Chip summary:** one concise implementation/status sentence for the active chip.
- **Sources:** chip-native channels, voices, lanes, operators, or sample slots with their owned controls.
- **Chip edit workspace:** tone, shape, filter, operator, envelope, sample-bank, loop, routing, and chip-specific utility controls.
- **Performance macros:** modern musical helpers and register-aware macro gestures, clearly labeled when they are not native hardware behavior.
- **Output and telemetry:** output trim, scope, footer status, build marker, MIDI CC range, and verification disclosure.
- **Register/debug view:** renderer/developer output only; hidden from the normal musical workflow except as compact readouts and tooltips.

Controls use plain names and tooltips. Mode-specific controls remain musically labeled and avoid register names, hex values, and chip-documentation jargon in the main workflow.

Mode-specific layouts should adapt structurally, not just change labels. Section headers stay unnumbered, and each chip may use a fixed or aspect-aware layout when that better serves readability. The current default editor contract is 1240 x 820 for most chips, with documented fixed-height exceptions such as SID at 1240 x 880 when a native control surface needs the room. The goal is enough room for standard controls and waveform previews without opening off-screen in typical DAW windows. NES and SN76489 can stay generator/register-strip oriented; SID should use three deep voice panels plus a global filter; SNES should become an eight-voice sample-bank workstation with BRR/sample selection, ADSR/GAIN, memory budget, and echo/FIR controls; FM chips should use operator and algorithm views. Reuse Chipper components where possible, but let each chip's native instrument shape determine the layout.

Fixed regressions remain part of the release gate rather than active feature planning. Current smoke gates include FM held-note tail checks for the ymfm/emu2413-backed chips and NES DMC one-shot loop-off behavior. If either issue returns in a current build, treat it as a P0 regression; otherwise prioritize deeper operator, sample, wave, preset, and verification work. Documentation-only planning updates should use `git diff --check`; rebuild, install, and DAW footer verification are required for code, DSP, parameter, UI, preset, or installer changes. When a doc says a chip or preset is "current," prefer renderer-exported metadata over hand-maintained prose.
