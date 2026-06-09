# Chipper Priority Roadmap

This list ranks near-term work by user value, implementation effort, and confidence. Value and confidence are scored 1-10, where 10 is highest. Effort is scored 1-10, where 10 is largest. "Confidence improves with" names the evidence or preparation that would make the work safer to execute.

| Rank | Todo | User Value | Effort | Confidence | Confidence Improves With |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | Make implemented chip UIs read as real instruments while preserving chip-native controls. Start with clearer SID voice cards, per-voice ADSR readability, filter-first SID tone controls, and legible output monitoring. | 10 | 5 | 8 | Visual checks in the standalone/VST host, reusable source-card layout helpers, and descriptor screenshots for each chip. |
| 2 | Expand the verification harness so every implemented chip has pitch, envelope, noise, mixer, source-enable, MIDI CC, preset, and debug JSON assertions tied to descriptor metadata. | 10 | 6 | 8 | More compact assertion helpers and reference metadata exported from `chipper_render --list-descriptors`. |
| 3 | Improve SID authenticity where it most affects musicians: per-voice ADSR edge cases, sync/ring interactions, filter routing, 6581/8580 response differences, and output drive. | 10 | 8 | 6 | Trusted SID references, hardware captures or accepted emulator comparisons, and narrow renderer traces for each behavior. |
| 4 | Add a reusable register-control component system so discrete chip fields never appear as generic smooth sliders. | 9 | 6 | 8 | A stronger `ChipParameterSpec` layout schema and UI smoke checks for legal choices per chip. |
| 5 | Make factory presets more delightful and source-informed for implemented chips, with visible controls matching every preset snapshot. | 9 | 4 | 9 | `chipper_render --list-presets`, factory preset audibility tests, renderer loudness checks, and descriptor JSON checks for pinned controls. |
| 6 | Improve NES/RP2A03 authenticity around DMC sample playback, frame sequencer timing, sweep edge cases, nonlinear mixer tolerances, and test trace coverage. DMC sample support should use user-selected/imported sample paths; do not commit local test samples or unlicensed sample content. | 9 | 7 | 7 | Local development samples, test ROM references, emulator comparison renders, and golden metadata for existing clean-room behavior. |
| 7 | Improve DMG authenticity around wave RAM writes, length/envelope timing, sweep overflow behavior, NR52 power behavior, and stereo routing. | 8 | 7 | 7 | Pan Docs-style public references, focused register traces, and stronger edge-case renderer tests. |
| 8 | Improve YM2149/AY and SN76489 usability with chip-specific mixer/noise panels, preset categories, and clearer channel source cards. | 8 | 5 | 8 | Shared source-card component improvements and current renderer JSON for mixer/noise state. |
| 9 | Add an explicit UI/renderer accuracy badge per chip mode that distinguishes Inspired, Hybrid, Authentic, and unverified roadmap modes without overclaiming. | 8 | 4 | 9 | A single descriptor field for verified behaviors and docs reviewed against current test coverage. |
| 10 | Audit compatible emulator cores and license paths for planned FM/sample/wavetable chips before any vendoring. | 8 | 6 | 7 | License matrix, provenance notes, and source-map updates for each candidate core; ymfm is now vendored as the first permissive FM path for YM2612, OPL2, and YM2151. |
| 11 | Expand the ymfm-backed YM2612 pass from useful six-lane melodic adapter to deeper Genesis FM instrument. | 8 | 8 | 7 | Dedicated operator grid, per-operator ADSR, DAC/sample behavior, LFO/AMS/PMS, register traces, and golden tests. |
| 12 | Add hardware/reference comparison documentation and tolerance thresholds for each implemented chip. | 7 | 8 | 5 | Real hardware recordings, reproducible capture settings, and agreed spectral/timing tolerances. |

## Chip UI And License Pass Order

SID is the first proof-of-product chip because it exercises the hardest instrument UI ideas early: three independent oscillators, per-voice ADSR, PWM, sync/ring interaction, multimode filter routing, model selection, source scopes, and output monitoring. The pass is only done when cutoff, resonance, filter mode, and filter routing stay together in the Filter module; waveform choices stay with the voice cards; and ADSR controls remain per voice.

After SID, prioritize the other implemented chips before adding planned chips:

| Order | Chip | User Value | License / Source Posture | Next UI Value |
| --- | --- | ---: | --- | --- |
| 1 | SID / C64 | 10 | Current code is internal clean-room. GPL SID projects remain reference/validation only unless Chipper adopts a compatible open-source license. | Finish filter-first panel, per-voice ADSR readability, waveform/source scopes, and stronger preset delight. |
| 2 | NES / RP2A03 | 10 | Current code is internal clean-room. FigBug/RP2A03 is license-sensitive and not vendored. Local DMC samples may be used for development only and must not be committed. | Make pulse/triangle/noise channels feel playable, add user-path/import based DMC sample support, expose DMC/direct DAC honestly, improve duty/sweep/noise percussion controls. |
| 3 | Game Boy / DMG | 9 | Current code is internal clean-room. SameBoy needs file-level audit before extraction; FigBug/PAPU remains GPL reference-only. | Expose pulse duty/sweep, Wave RAM shape/level, NR51 stereo route, and noise width/clock as chip-native controls. |
| 4 | YM2149 / AY | 8 | Current audio path is backed by vendored MIT emu2149, with provenance notes and Chipper-side source/macro adaptation. | Improve A/B/C mixer rows, hardware envelope shape visualization, fake-chord/arpeggio controls, shared noise clarity, and golden comparisons. |
| 5 | SN76489 / Sega PSG | 8 | Current audio path is backed by vendored MIT emu76489; LGPL-sensitive references remain comparison-only. | Improve tone/noise channel cards, attenuation/noise mode controls, and arcade/Sega preset vocabulary. |
| 6 | SNES SPC700-style | 8 | Current code is internal clean-room partial. emu-rs/snes-apu and snes-echo are permissive-looking audit candidates; C700, blargg snes_spc, GME, Furnace, and MAME remain reference or license-sensitive audit targets only. | Replace the generic four-source view with eight sample-voice lanes, BRR/sample slots, ADSR/gain controls, Gaussian/output filtering, and honest echo/FIR status. |
| 7 | Atari POKEY | 7 | Current code is internal clean-room partial; web-pokey remains a permissive candidate/reference until audited. | Expand beyond the first AUDF/AUDC/AUDV pass with clearer distortion-code UI, AUDCTL pairing/high-pass controls, and Atari preset vocabulary. |
| 8 | Amiga Paula | 8 | Current code is internal clean-room partial; pt2-clone/libxmp/Furnace remain reference or audit targets only. | Replace the generic four-source view with tracker sample-channel strips, period/loop/volume controls, sample browser/import, and MOD-style retrigger vocabulary. |
| 9 | PC Engine HuC6280 | 7 | Current code is internal clean-room partial; MAME/GME/Furnace/HuSIC remain reference or audit targets only. | Replace the generic four-source view with six wavetable lanes, wave RAM visuals, noise/LFO controls, and PC Engine preset vocabulary. |
| 10 | Namco arcade WSG | 7 | Current code is internal clean-room partial; MAME/Furnace remain reference or audit targets only. | Replace the generic four-source view with WSG lane banks, 4-bit wave RAM visuals, enable/volume controls, and arcade preset vocabulary. |
| 11 | Konami SCC | 7 | Current audio path is backed by vendored MIT emu2212; exact SCC/SCC+ quirks still need golden/reference validation. | Replace the generic four-source view with five wavetable lanes, wave RAM visuals/editing, key/volume controls, and Konami preset vocabulary. |
| 12 | YM2612 / OPN2, OPL2/OPL3, and YM2151 / OPM | 8 | ymfm is now vendored for first YM2612, OPL2, and YM2151 melodic adapters; use the same audited path for future OPL3-specific work. | Use a real operator/algorithm UI with envelope visuals instead of generic oscillator controls. |

## Wide Chip Implementation Queue

Going wide is useful now, but only if planned modes stay honest. Planned chip descriptors should expose roadmap templates, source strategy, UI shape, and blockers while continuing to render silence or unsupported-core output until a verified core exists.

| Order | Chip Family | First Value To Deliver | Likely Source Path | UI Shape | Confidence |
| --- | --- | --- | --- | --- | ---: |
| 1 | YM2612 / OPN2 | Real Genesis-style FM bass/lead core with operator automation. | First BSD-3-Clause `ymfm` adapter is in place; keep Nuked-OPN2 as LGPL-sensitive comparison. | Six FM voice strips with algorithm, feedback, operator envelope/level, DAC, and stereo. | 8 |
| 2 | OPL2/OPL3 | DOS FM instruments and rhythm-mode kits. | First BSD-3-Clause `ymfm` OPL2/YM3812 adapter is in place; keep Nuked-OPL3 as LGPL-sensitive comparison. | Operator-pair editor with waveform selector, rhythm mode, feedback, tremolo/vibrato, and per-operator envelopes. | 8 |
| 3 | YM2149 / AY polish | Stronger early-computer beeps, arps, and noise percussion from a vendored PSG core. | Continue the `emu2149` adapter and compare against references before stronger claims. | Three channel strips plus shared noise/envelope shape visualization. | 8 |
| 4 | SN76489 polish | Better Sega PSG/arcade immediacy from an emu-backed core. | `emu76489` is now vendored under MIT; FigBug/SN76489 remains LGPL-sensitive reference. | Three tone strips plus noise strip with attenuation and noise-control choices. | 8 |
| 5 | Game Boy / DMG polish | Game Boy pulse/wave/noise patches with clearer Wave RAM and stereo routing. | Continue clean-room; SameBoy only after file-level audit; FigBug/PAPU remains GPL reference. | Four APU lanes with pulse sweep, Wave RAM view, NR32 level, NR51 routing, and noise width. | 7 |
| 6 | Amiga Paula deeper pass | Make the new partial tracker-sampler core feel like a first-class Amiga instrument. | Continue clean-room; audit pt2-clone/libxmp/Furnace references before any reuse. | Four sample channels with period, loop, volume, panning, retrigger, and sample browser. | 7 |
| 7 | SPC700-style deeper pass | Make the new partial lo-fi sample-voice core feel like a first-class SNES-style instrument. | Continue clean-room; audit emu-rs/snes-apu and snes-echo first for permissive paths, and keep C700/blargg snes_spc/GME/Furnace/MAME reference-only unless licensing is settled. | Eight sample voices with BRR/sample slots, ADSR/gain, pitch modulation status, Gaussian/output filtering, echo/FIR controls, and clear "style" labeling. | 6 |
| 8 | YM2151 / OPM | Arcade/X68000 four-op FM leads and metallic basses. | First BSD-3-Clause `ymfm` OPM melodic adapter is in place. | Eight four-operator voices with algorithm, feedback, LFO PM/AM, noise, and per-operator envelopes. | 8 |
| 9 | YM2413 / OPLL | Preset-FM quick sounds and UI/game chimes. | Audit `ymfm` and `emu2413`. | Preset instrument selector plus limited editable patch/rhythm controls. | 7 |
| 10 | SCC deeper pass | Turn the new emu2212-backed partial Konami SCC/SCC+ core into a first-class five-channel wavetable instrument. | Continue the emu2212 adapter; add golden/reference comparisons before stronger accuracy claims. | Five wavetable strips with wave edit/slot, level, tuning, key mask, and stack/spread macros. | 8 |
| 11 | HuC6280 deeper pass | Turn the new partial PC Engine core into a first-class six-channel wavetable instrument. | Continue clean-room; audit MAME/GME/Furnace/HuSIC before any reuse. | Six wavetable channel strips with wave RAM preview/editing, native noise, LFO, channel balance, and source scopes. | 6 |
| 12 | Namco WSG deeper pass | Turn the new partial Namco WSG core into a first-class arcade wavetable instrument. | Continue clean-room; audit any MAME/Furnace-derived references before reuse. | Wavetable lane banks with 4-bit waveform memory, voice-count variants, enable masks, levels, tuning, and source scopes. | 6 |
| 13 | POKEY deeper pass | Improve the new partial Atari core. | Continue clean-room; web-pokey/MAME/Furnace reference-only unless audited. | Add AUDCTL channel pairing, high-pass filters, stronger polynomial validation, and source-card visuals. | 6 |
Confidence improves most by doing small adapter spikes, adding renderer traces before UI exposure, and recording exact upstream commit/license details before any third-party source enters the tree.

Arcade-style sounds should come from named chip modes such as YM2149, SN76489, Namco WSG, SCC, and YM2151 rather than from an undefined generic arcade mode.

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
