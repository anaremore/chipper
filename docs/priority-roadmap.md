# Chipper Priority Roadmap

This list ranks near-term work by user value, implementation effort, and confidence. Value and confidence are scored 1-10, where 10 is highest. Effort is scored 1-10, where 10 is largest. "Confidence improves with" names the evidence or preparation that would make the work safer to execute.

| Rank | Todo | User Value | Effort | Confidence | Confidence Improves With |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | Make implemented chip UIs read as real instruments while preserving chip-native controls. Start with clearer SID voice cards, per-voice ADSR readability, filter-first SID tone controls, and legible output monitoring. | 10 | 5 | 8 | Visual checks in the standalone/VST host, reusable source-card layout helpers, and descriptor screenshots for each chip. |
| 2 | Expand the verification harness so every implemented chip has pitch, envelope, noise, mixer, source-enable, MIDI CC, preset, and debug JSON assertions tied to descriptor metadata. | 10 | 6 | 8 | More compact assertion helpers and reference metadata exported from `chipper_render --list-descriptors`. |
| 3 | Improve SID authenticity where it most affects musicians: per-voice ADSR edge cases, sync/ring interactions, filter routing, 6581/8580 response differences, and output drive. | 10 | 8 | 6 | Trusted SID references, hardware captures or accepted emulator comparisons, and narrow renderer traces for each behavior. |
| 4 | Add a reusable register-control component system so discrete chip fields never appear as generic smooth sliders. | 9 | 6 | 8 | A stronger `ChipParameterSpec` layout schema and UI smoke checks for legal choices per chip. |
| 5 | Make factory presets more delightful and source-informed for implemented chips, with visible controls matching every preset snapshot. | 9 | 4 | 9 | Preset audition passes, renderer loudness checks, and descriptor JSON checks for pinned controls. |
| 6 | Improve NES/RP2A03 authenticity around DMC sample playback, frame sequencer timing, sweep edge cases, nonlinear mixer tolerances, and test trace coverage. DMC sample support should use user-selected/imported sample paths; do not commit local test samples or unlicensed sample content. | 9 | 7 | 7 | Local development samples, test ROM references, emulator comparison renders, and golden metadata for existing clean-room behavior. |
| 7 | Improve DMG authenticity around wave RAM writes, length/envelope timing, sweep overflow behavior, NR52 power behavior, and stereo routing. | 8 | 7 | 7 | Pan Docs-style public references, focused register traces, and stronger edge-case renderer tests. |
| 8 | Improve YM2149/AY and SN76489 usability with chip-specific mixer/noise panels, preset categories, and clearer channel source cards. | 8 | 5 | 8 | Shared source-card component improvements and current renderer JSON for mixer/noise state. |
| 9 | Add an explicit UI/renderer accuracy badge per chip mode that distinguishes Inspired, Hybrid, Authentic, and unverified roadmap modes without overclaiming. | 8 | 4 | 9 | A single descriptor field for verified behaviors and docs reviewed against current test coverage. |
| 10 | Audit compatible emulator cores and license paths for planned FM/sample/wavetable chips before any vendoring. | 8 | 6 | 6 | License matrix, provenance notes, and source-map updates for each candidate core. |
| 11 | Implement the next chip core only after a scoped accuracy plan exists; likely OPL2/OPL3 or YM2612 using a compatible high-quality FM core. | 8 | 9 | 5 | Completed license audit, a thin adapter spike, and renderer golden tests before VST UI exposure. |
| 12 | Add hardware/reference comparison documentation and tolerance thresholds for each implemented chip. | 7 | 8 | 5 | Real hardware recordings, reproducible capture settings, and agreed spectral/timing tolerances. |

## Chip UI And License Pass Order

SID is the first proof-of-product chip because it exercises the hardest instrument UI ideas early: three independent oscillators, per-voice ADSR, PWM, sync/ring interaction, multimode filter routing, model selection, source scopes, and output monitoring. The pass is only done when cutoff, resonance, filter mode, and filter routing stay together in the Filter module; waveform choices stay with the voice cards; and ADSR controls remain per voice.

After SID, prioritize the other implemented chips before adding planned chips:

| Order | Chip | User Value | License / Source Posture | Next UI Value |
| --- | --- | ---: | --- | --- |
| 1 | SID / C64 | 10 | Current code is internal clean-room. GPL SID projects remain reference/validation only unless Chipper adopts a compatible open-source license. | Finish filter-first panel, per-voice ADSR readability, waveform/source scopes, and stronger preset delight. |
| 2 | NES / RP2A03 | 10 | Current code is internal clean-room. FigBug/RP2A03 is license-sensitive and not vendored. Local DMC samples may be used for development only and must not be committed. | Make pulse/triangle/noise channels feel playable, add user-path/import based DMC sample support, expose DMC/direct DAC honestly, improve duty/sweep/noise percussion controls. |
| 3 | Game Boy / DMG | 9 | Current code is internal clean-room. SameBoy needs file-level audit before extraction; FigBug/PAPU remains GPL reference-only. | Expose pulse duty/sweep, Wave RAM shape/level, NR51 stereo route, and noise width/clock as chip-native controls. |
| 4 | YM2149 / AY | 8 | Current code is internal clean-room; permissive candidates exist in the source map and must be audited before reuse. | Improve A/B/C mixer rows, hardware envelope shape visualization, fake-chord/arpeggio controls, and shared noise clarity. |
| 5 | SN76489 / Sega PSG | 8 | Current code is internal clean-room; permissive and LGPL-sensitive references are tracked separately. | Improve tone/noise channel cards, attenuation/noise mode controls, and arcade/Sega preset vocabulary. |
| 6 | Atari POKEY | 7 | Current code is internal clean-room partial; web-pokey remains a permissive candidate/reference until audited. | Expand beyond the first AUDF/AUDC/AUDV pass with clearer distortion-code UI, AUDCTL pairing/high-pass controls, and Atari preset vocabulary. |
| 7 | YM2612 / OPN2 and OPL2/OPL3 | 8 | Do not start VST UI exposure until an FM core license plan is chosen and documented. | Use a real operator/algorithm UI with envelope visuals instead of generic oscillator controls. |

## Wide Chip Implementation Queue

Going wide is useful now, but only if planned modes stay honest. Planned chip descriptors should expose roadmap templates, source strategy, UI shape, and blockers while continuing to render silence or unsupported-core output until a verified core exists.

| Order | Chip Family | First Value To Deliver | Likely Source Path | UI Shape | Confidence |
| --- | --- | --- | --- | --- | ---: |
| 1 | YM2612 / OPN2 | Real Genesis-style FM bass/lead core with operator automation. | Audit and spike `ymfm` first; keep Nuked-OPN2 as LGPL-sensitive comparison. | Six FM voice strips with algorithm, feedback, operator envelope/level, DAC, and stereo. | 6 |
| 2 | OPL2/OPL3 | DOS FM instruments and rhythm-mode kits. | Audit and spike `ymfm` first; keep Nuked-OPL3 as LGPL-sensitive comparison. | Operator-pair editor with waveform selector, rhythm mode, feedback, tremolo/vibrato, and per-operator envelopes. | 6 |
| 3 | YM2149 / AY polish | Stronger early-computer beeps, arps, and noise percussion from an already implemented core. | Continue clean-room; evaluate `emu2149` or `ayumi` if needed. | Three channel strips plus shared noise/envelope shape visualization. | 8 |
| 4 | SN76489 polish | Better Sega PSG/arcade immediacy from an already implemented core. | Continue clean-room; evaluate `emu76489`; FigBug/SN76489 remains LGPL-sensitive reference. | Three tone strips plus noise strip with attenuation and noise-control choices. | 8 |
| 5 | Game Boy / DMG polish | Game Boy pulse/wave/noise patches with clearer Wave RAM and stereo routing. | Continue clean-room; SameBoy only after file-level audit; FigBug/PAPU remains GPL reference. | Four APU lanes with pulse sweep, Wave RAM view, NR32 level, NR51 routing, and noise width. | 7 |
| 6 | Amiga Paula | Tracker sample playback as a musician-friendly sampler mode. | Clean-room sampler model with audited ProTracker/Paula references. | Four sample channels with period, loop, volume, panning, retrigger, and sample browser. | 7 |
| 7 | SPC700-style | Lo-fi sample playback and SNES-flavored ambience without overclaiming DSP accuracy. | Separate sampler/DSP design; Furnace and MAME as reference-only until audited. | Sample voice bank with rate, ADSR/gain, echo color, and clear "style" labeling. | 5 |
| 8 | YM2151 / OPM | Arcade/X68000 four-op FM leads and metallic basses. | Audit and spike `ymfm`. | Eight four-operator voices with algorithm, feedback, LFO PM/AM, and envelopes. | 6 |
| 9 | YM2413 / OPLL | Preset-FM quick sounds and UI/game chimes. | Audit `ymfm` and `emu2413`. | Preset instrument selector plus limited editable patch/rhythm controls. | 7 |
| 10 | Konami SCC | Five-channel wavetable stack and crunchy arps. | Audit `emu2212`; clean-room fallback feasible. | Five wavetable strips with wave edit/slot, level, tuning, and stack/spread macros. | 7 |
| 11 | HuC6280 / Namco WSG | Wavetable arcade/console colors. | Clean-room first; audit any MAME/Furnace-derived references before reuse. | Wavetable channel banks with waveform memory, noise/LFO where native, and source scopes. | 5 |
| 12 | POKEY deeper pass | Improve the new partial Atari core. | Continue clean-room; web-pokey/MAME/Furnace reference-only unless audited. | Add AUDCTL channel pairing, high-pass filters, stronger polynomial validation, and source-card visuals. | 6 |
| 13 | Arcade Hybrid | Delightful cross-chip SFX layer. | Compose only from verified Chipper cores and original templates. | Performance layer over verified sources with no false single-chip accuracy claim. | 8 |

Confidence improves most by doing small adapter spikes, adding renderer traces before UI exposure, and recording exact upstream commit/license details before any third-party source enters the tree.

## Open-Source Decision Gate

Chipper can be open sourced, but the project license should be chosen before importing GPL/LGPL code. Until that decision is made:

- Keep current clean-room cores and permissive candidates as the preferred implementation path.
- Treat GPL-family projects, including large tracker references, as behavior/UX/reference material only.
- Treat LGPL candidates as possible future integrations only after deciding dynamic/static linking, relinkability, source distribution, attribution, and commercial distribution obligations.
- Add exact upstream URL, revision, license file, and file-level audit notes to `THIRD_PARTY_NOTICES.md` before any code, lookup table, preset dump, sample, or derived asset enters the repo.

## Current Execution Rule

Each completed development slice should:

- Improve either emulation truth, musical usability, verification, presets, documentation, or installability.
- Keep DAW parameter IDs and MIDI CC mappings stable unless a migration plan exists.
- Build `Chipper_VST3`, run the relevant tests, install the VST3, commit, and push.
- Avoid product claims stronger than the current renderer tests and documented verification support.
