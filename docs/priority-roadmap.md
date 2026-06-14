# Chipper Priority Roadmap

This list ranks near-term work by user value, implementation effort, and confidence. Value and confidence are scored 1-10, where 10 is highest. Effort is scored 1-10, where 10 is largest. "Confidence improves with" names the evidence or preparation that would make the work safer to execute.

For broader product gaps beyond this immediate chip-core and UI execution list, see [product-gap-roadmap.md](product-gap-roadmap.md). That document tracks the larger instrument-workflow work: tracker motion, wave/sample editing, FM operator editing, drum/SFX workflows, multi-output routing, preset tags, Strictness behavior, and MIDI/automation polish.

| Rank | Todo | User Value | Effort | Confidence | Confidence Improves With |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | Finish the all-chip readability pass: no overlapping text, standard-height dropdowns/inputs, visible level controls, and no dead placeholder modules. | 10 | 4 | 9 | Screenshot checks for every chip after each layout change and a small UI non-regression checklist. |
| 2 | Keep chip-native controls owned by their source cards. Pulse duty belongs under pulse voices, wave/sample selectors under wavetable or sampler voices, and noise modes under noise voices. | 10 | 5 | 9 | Reusable source-card layout helpers and descriptor-driven visibility tests. |
| 3 | Make sample and wavetable chips feel first-class: Paula, SPC700, HuC6280, Namco WSG, and SCC need per-channel sample/wave selection, visible level, waveform preview, root/loop behavior, and missing-asset states. | 10 | 7 | 8 | More renderer coverage for sample-slot selection, note-map behavior, loop flags, and state recall. |
| 4 | Expand preset value: more high-quality factory presets, role/category filtering, user preset save/load, portable flat files, and audible preset regression checks. | 9 | 5 | 8 | Preset metadata validation, loudness checks, and a small hand-audition checklist for each chip. |
| 5 | Build real FM editor surfaces for YM2612/OPN2, YM2151/OPM, OPL2/OPL3, and YM2413/OPLL. Macros can stay, but serious users need operator/algorithm/envelope access. | 9 | 8 | 7 | Operator-grid UI spike, sustained-note regression checks, and register/debug JSON for algorithm, level, feedback, and envelope state. |
| 6 | Improve NES/RP2A03 authenticity and usability around DMC playback, sweep edge cases, frame sequencing, nonlinear mixer tolerances, and test trace coverage. | 9 | 7 | 8 | Local development samples, test ROM references, emulator comparison renders, and golden metadata for clean-room behavior. |
| 7 | Add chip-aware tracker motion and SFX gestures that write native-looking register changes instead of generic modulation. | 9 | 7 | 7 | A per-chip destination map, renderer traces for common gestures, and UI copy that shows the resolved register path. |
| 8 | Enforce Strictness behavior consistently. The header control should choose Inspired/Hybrid/Authentic behavior; the footer verification label should state what is actually tested. | 8 | 5 | 8 | Descriptor metadata for allowed controls, host-state recall tests, and docs reviewed against current test coverage. |
| 9 | Expand verification coverage across implemented chips: pitch, envelope/EG, noise, mixer, sample/wave selection, source-enable, MIDI CC, preset, sustained-note, loop-off, and debug JSON assertions. | 8 | 7 | 8 | Compact assertion helpers and reference metadata exported from `chipper_render --list-descriptors`. |
| 10 | Keep the license/provenance audit current before importing or deriving from any outside emulator, waveform, table, preset dump, or sample asset. | 8 | 5 | 9 | Exact upstream URL, revision, license file, file-level notes, and `THIRD_PARTY_NOTICES.md` entries before code enters the tree. |
| 11 | Add hardware/reference comparison documentation and tolerance thresholds for each implemented chip. | 7 | 8 | 5 | Real hardware recordings, reproducible capture settings, accepted emulator comparisons, and agreed spectral/timing tolerances. |
| 12 | Release readiness: installer clarity, GitHub tag builds, DAW scan guidance, platform build notes, and compatibility QA for Windows, Linux, and macOS. | 7 | 6 | 7 | GitHub Actions release workflow, artifact naming, smoke tests, and host verification notes. |

## Current State Snapshot

- `Arcade Hybrid` and `Custom` are intentionally removed from the chip selector. Arcade sounds should come from named hardware families such as YM2149, SN76489, Namco WSG, SCC, YM2151, and OPL.
- The top control is **Strictness**, not an accuracy claim. It changes how hard Chipper leans into native limits. The small footer status is where verification language belongs.
- Channel-local register controls must stay inside the channel/voice cards wherever practical. Detached summary panels are only for genuinely shared hardware such as filters, echo, sample banks, output routing, and cross-channel pairing.
- Factory presets and user presets must not embed copyrighted samples, ROM data, copyrighted wave dumps, or derived game assets. External sample paths stay user-owned and outside the repository.
- FM sustained-note output is considered fixed by current engine work. Treat "FM fades to silence while a note is held" as a P0 regression if it reappears, not as open product design work. Keep sustained-note smoke/regression coverage in the FM test plan for YM2612/OPN2, OPL2/OPL3, YM2151/OPM, and YM2413/OPLL.
- NES DMC loop-off playback is now part of the regression contract: one-shot sample stepping stops at the final bit and the DMC DAC holds the terminal level until retrigger, while the loop bit explicitly repeats the sample. Treat "DMC loops while Loop Sample is off" as a regression.
- Current planning assumes recent layout passes are the baseline, not the destination. Remaining UI work should prioritize controls that are still hard to see, channel-local controls that still live in shared panels, and chip families that still need deeper editor surfaces.

## Current Cleanup Targets

These are the highest-signal cleanup items after the latest all-chip screenshot pass:

- Keep sampler layouts collision-free. Paula and SPC700 need protected vertical room for sample banks, waveform previews, root/playback controls, and Performance Macros.
- Keep wavetable cards readable. HuC6280, Namco WSG, and SCC should keep per-lane wave selectors and visible level controls in each source card; if a card cannot fit them, grow the layout rather than hiding controls.
- Keep helper envelopes honest. POKEY, Paula, HuC6280, Namco WSG, and SCC helper envelopes are Chipper musical volume helpers, not native ADSR.
- Keep FM surfaces playable while the deeper operator editor is planned. Macro/editor passes must not disturb key-on, source-level, or operator-envelope sustain behavior.
- Keep docs and UI labels aligned: the header says **Strictness**; verification strength belongs in the footer, renderer debug JSON, and accuracy docs.

## Immediate Execution Checklist

Use this checklist before calling any slice done. It is intentionally small enough to run often.

- **Readability:** inspect the affected chip plus one neighboring chip for overlapping labels, clipped dropdowns/inputs, invisible level lanes, and stale hidden-module text.
- **Audibility:** load at least one preset for the affected chip and confirm the visible controls represent the sound that is playing.
- **Sustain:** when an FM chip is touched, run or manually verify a held note does not decay to silence unless the selected envelope clearly asks for that.
- **Samples:** when NES DMC, SPC700, or Paula sample handling is touched, verify one-shot vs loop behavior, note-map/manual-slot behavior, and missing-file UI states.
- **State:** switch away from the chip and back, then confirm the chip-local settings, preset choice, and sample/wave selections are preserved.
- **Honesty:** update docs and footer/descriptor wording whenever implementation evidence changes; do not promote a mode beyond what tests or references support.
- **Release hygiene:** for code changes, build `Chipper_VST3`, run targeted tests, install from the same build root, and verify the footer hash against the installed marker before committing.

## Chip UI And License Pass Order

SID proved the first deep instrument UI pattern: three independent oscillator voices, per-voice ADSR, PWM, sync/ring interaction, multimode filter routing, model selection, source scopes, and output monitoring. That pattern should now generalize carefully, not blindly. Each chip gets the structure implied by its hardware: channel strips for PSG/APU chips, deep voice cards for SID, operator grids for FM, and sampler/wavetable workspaces for SPC700, Paula, HuC6280, Namco WSG, and SCC.

After SID, prioritize the other implemented chips before adding planned chips:

| Order | Chip | User Value | License / Source Posture | Next UI Value |
| --- | --- | ---: | --- | --- |
| 1 | NES / RP2A03 | 10 | Current code is internal clean-room. FigBug/RP2A03 is license-sensitive and not vendored. Local DMC samples may be used for development only and must not be committed. | Keep pulse duty and noise mode inside channel cards, keep DMC sample browsing/looping obvious, preserve loop-off tests, and improve DMC waveform preview/import workflow. |
| 2 | Game Boy / DMG | 9 | Current code is internal clean-room. SameBoy needs file-level audit before extraction; FigBug/PAPU remains GPL reference-only. | Preserve independent Pulse 1/Pulse 2 duty controls inside their cards, keep Wave RAM shape/level in the wave card, and keep NR51 stereo routing readable. |
| 3 | SID / C64 | 10 | Current code is internal clean-room. GPL SID projects remain reference/validation only unless Chipper adopts a compatible open-source license. | Keep filter controls together, preserve per-voice ADSR, expose voice 3 behavior honestly, and deepen presets around PWM, filter, sync, and ring-mod use. |
| 4 | YM2149 / AY | 8 | Current audio path is backed by vendored MIT emu2149, with provenance notes and Chipper-side source/macro adaptation. | Improve A/B/C mixer rows, hardware envelope shape visualization, fake-chord/arpeggio controls, shared noise clarity, and golden comparisons. |
| 5 | SN76489 / Sega PSG | 8 | Current audio path is backed by vendored MIT emu76489; LGPL-sensitive references remain comparison-only. | Improve tone/noise channel cards, attenuation/noise mode controls, and arcade/Sega preset vocabulary. |
| 6 | SNES SPC700-style | 8 | Current code is internal clean-room partial. emu-rs/snes-apu and snes-echo are permissive-looking audit candidates; C700, blargg snes_spc, GME, Furnace, and MAME remain reference or license-sensitive audit targets only. | Deepen the current eight-voice sample surface with clearer per-voice loop/envelope/noise/echo state, exact BRR loop semantics, Gaussian/output validation, and honest echo/FIR controls. |
| 7 | Atari POKEY | 7 | Current code is internal clean-room partial; web-pokey remains a permissive candidate/reference until audited. | Expand beyond the first AUDF/AUDC/AUDV pass with clearer distortion-code UI, AUDCTL pairing/high-pass controls, and Atari preset vocabulary. |
| 8 | Amiga Paula | 8 | Current code is internal clean-room partial; pt2-clone/libxmp/Furnace remain reference or audit targets only. | Deepen the current four-channel tracker sample surface with visible period/loop/volume state, MOD-style retrigger vocabulary, imported loop metadata, and per-channel sample workflow checks. |
| 9 | PC Engine HuC6280 | 7 | Current code is internal clean-room partial; MAME/GME/Furnace/HuSIC remain reference or audit targets only. | Deepen the current six-lane wavetable surface with editable 32-sample Wave RAM, native noise, Ch 1/2 LFO clarity, per-lane pitch/pan where appropriate, and PC Engine preset vocabulary. |
| 10 | Namco arcade WSG | 7 | Current code is internal clean-room partial; MAME/Furnace remain reference or audit targets only. | Deepen the current eight-lane WSG surface with 4-bit Wave RAM editing, enable/volume masks, voice-count variants, tuning controls, and arcade preset vocabulary. |
| 11 | Konami SCC | 7 | Current audio path is backed by vendored MIT emu2212; exact SCC/SCC+ quirks still need golden/reference validation. | Deepen the current five-lane SCC wavetable surface with Wave RAM editing, key/volume controls, SCC/SCC+ distinctions, and deeper Konami preset vocabulary. |
| 12 | YM2612 / OPN2, OPL2/OPL3, YM2151 / OPM, and YM2413 / OPLL | 8 | ymfm is now vendored for first YM2612, OPL2, and YM2151 melodic adapters; emu2413 is vendored for OPLL. Keep OPL3-specific, custom OPLL patch, and rhythm behavior inside the audited source/licensing plan. | Move from macro/operator summaries to real operator/algorithm UI with envelope visuals, carrier/modulator clarity, sustained-note regression checks, and chip-specific FM artifacts. |

## Wide Chip Implementation Queue

Going wide is useful now, but only if planned modes stay honest. Planned chip descriptors should expose roadmap templates, source strategy, UI shape, and blockers while continuing to render silence or unsupported-core output until a verified core exists.

| Order | Chip Family | First Value To Deliver | Likely Source Path | UI Shape | Confidence |
| --- | --- | --- | --- | --- | ---: |
| 1 | YM2612 / OPN2 | Real Genesis-style FM bass/lead core with operator automation. | First BSD-3-Clause `ymfm` adapter is in place; keep Nuked-OPN2 as LGPL-sensitive comparison. | Six FM voice strips with algorithm, feedback, operator envelope/level, DAC, and stereo. | 8 |
| 2 | OPL2/OPL3 | DOS FM instruments and rhythm-mode kits. | First BSD-3-Clause `ymfm` OPL2/YM3812 adapter is in place; keep Nuked-OPL3 as LGPL-sensitive comparison. | Operator-pair editor with waveform selector, rhythm mode, feedback, tremolo/vibrato, and per-operator envelopes. | 8 |
| 3 | YM2149 / AY polish | Stronger early-computer beeps, arps, and noise percussion from a vendored PSG core. | Continue the `emu2149` adapter and compare against references before stronger claims. | Three channel strips plus shared noise/envelope shape visualization. | 8 |
| 4 | SN76489 polish | Better Sega PSG/arcade immediacy from an emu-backed core. | `emu76489` is now vendored under MIT; FigBug/SN76489 remains LGPL-sensitive reference. | Three tone strips plus noise strip with attenuation and noise-control choices. | 8 |
| 5 | Game Boy / DMG polish | Game Boy pulse/wave/noise patches with clearer Wave RAM and stereo routing. | Continue clean-room; SameBoy only after file-level audit; FigBug/PAPU remains GPL reference. | Four APU lanes with pulse sweep, Wave RAM view, NR32 level, NR51 routing, and noise width. | 7 |
| 6 | Amiga Paula deeper pass | Make the partial tracker-sampler core feel like a first-class Amiga instrument. | Continue clean-room; audit pt2-clone/libxmp/Furnace references before any reuse. | Four sample channels with per-channel sample/shape pins, period, loop, volume, hard-pan identity, retrigger, sample browser, and imported loop metadata. | 7 |
| 7 | SPC700-style deeper pass | Make the partial lo-fi sample-voice core feel like a first-class SNES-style instrument. | Continue clean-room; audit emu-rs/snes-apu and snes-echo first for permissive paths, and keep C700/blargg snes_spc/GME/Furnace/MAME reference-only unless licensing is settled. | Eight sample voices with per-voice sample slots, ADSR/GAIN, pitch modulation/noise status, Gaussian/output filtering, echo/FIR controls, 64 KB RAM budget, and clear "style" labeling. | 6 |
| 8 | YM2151 / OPM | Arcade/X68000 four-op FM leads and metallic basses. | First BSD-3-Clause `ymfm` OPM melodic adapter is in place. | Eight four-operator voices with algorithm, feedback, LFO PM/AM, noise, and per-operator envelopes. | 8 |
| 9 | YM2413 / OPLL | Preset-FM quick sounds and UI/game chimes. | Audit `ymfm` and `emu2413`. | Preset instrument selector plus limited editable patch/rhythm controls. | 7 |
| 10 | SCC deeper pass | Turn the emu2212-backed partial Konami SCC/SCC+ core into a first-class five-channel wavetable instrument. | Continue the emu2212 adapter; add golden/reference comparisons before stronger accuracy claims. | Five wavetable strips with wave edit/slot, visible level, tuning, key mask, SCC/SCC+ behavior, and stack/spread macros. | 8 |
| 11 | HuC6280 deeper pass | Turn the partial PC Engine core into a first-class six-channel wavetable instrument. | Continue clean-room; audit MAME/GME/Furnace/HuSIC before any reuse. | Six wavetable channel strips with wave RAM preview/editing, visible level, native noise, Ch 1/2 LFO behavior, channel balance, and source scopes. | 6 |
| 12 | Namco WSG deeper pass | Turn the partial Namco WSG core into a first-class arcade wavetable instrument. | Continue clean-room; audit any MAME/Furnace-derived references before reuse. | Wavetable lane banks with 4-bit waveform memory, visible levels, voice-count variants, enable masks, tuning, and source scopes. | 6 |
| 13 | POKEY deeper pass | Improve the partial Atari core. | Continue clean-room; web-pokey/MAME/Furnace reference-only unless audited. | Add AUDCTL channel pairing, high-pass filters, stronger polynomial validation, and source-card visuals. | 6 |
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
