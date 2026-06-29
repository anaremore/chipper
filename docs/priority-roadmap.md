# Chipper Priority Roadmap

This list ranks near-term work by user value, implementation effort, and confidence. Value and confidence are scored 1-10, where 10 is highest. Effort is scored 1-10, where 10 is largest. "Confidence improves with" names the evidence or preparation that would make the work safer to execute.

Review status: planning docs synced on 2026-06-29 after the ZX Spectrum beeper slice joined the implemented chip set. The editor height contract remains chip-aware: most modes stay at 1240 x 820, SID uses 1240 x 880 so its per-voice ADSR surface does not collide with Performance Macros, and the UI smoke gate checks every chip at the 1180 px compact-width contract. The focused UI gate includes default editor size, host-restored oversize clamping, readable visible controls, standard dropdown sizing, NES DMC sample controls, NES Performance Macro visibility, all-chip compact layout geometry, SID ADSR separation from Performance Macros at default and compact widths, a readable SID ADSR panel minimum, and sampler source-card Level lanes that must remain visible, owned, and close to their sample/wave selectors. Fixed regressions are release gates, not active roadmap items; broad workflow ideas, visual audit rules, fixed-bug details, and license evidence live in their owning docs. The current forward bias is concrete chip-aware improvements: finish the remaining layout/readability debt, then spend larger slices on sample/wave editors, presets, and FM operator editing. When we deliberately go wide again, the remembered next new-chip candidates are NES + Namco 163 after wave-editor groundwork, then SegaPCM, RF5C68, and OKI MSM6258/MSM6295 after sample-bank hardening.

Planning rule: keep this file pointed at the next shippable slice. If a behavior is already fixed and protected by a named CTest gate, reference the gate once here and keep the active queue focused on playable user value. For the current build, that means FM sustain is a guardrail and the next FM work is operator/algorithm/editing depth.

For broader product gaps beyond this immediate chip-core and UI execution list, see [product-gap-roadmap.md](product-gap-roadmap.md). That document tracks the larger instrument-workflow work: tracker motion, wave/sample editing, FM operator editing, drum/SFX workflows, multi-output routing, preset tags, Strictness behavior, and MIDI/automation polish.

## Planning Sync Check

When doing a planning-only cleanup, keep this checklist small and mechanical:

- Fixed regressions stay in the gate table, not the active queue. FM held-tail and NES DMC loop-off are closed unless a current build fails their named tests.
- Descriptor smoke is also a planning guardrail now: source-card ownership, visible source levels, MIDI CC coverage, named chip modes, preset presence, and truthful helper-envelope labels should stay green before UI cleanup is considered done.
- The active queue should describe forward user value: playable controls, better presets, sample/wave/operator editing, state recall, verification evidence, or release readiness.
- Keep owner docs separate:
  - [ui-priority-audit.md](ui-priority-audit.md): screenshot rules, chip-owned control placement, and visual non-regression checks.
  - [product-gap-roadmap.md](product-gap-roadmap.md): larger instrument workflows and backlog shape.
  - [emulation-accuracy.md](emulation-accuracy.md) and [emulator-source-map.md](emulator-source-map.md): verification, source, and license strength.
  - [release-builds.md](release-builds.md): build, install, release, and gate command lines.
  - [product-spec.md](product-spec.md) and README: user-facing behavior when it affects hosts, presets, sharing, or public positioning.

## Living Plan For The Next Slices

- Keep fixed regressions as tests, not roadmap churn. FM held-tail and NES DMC loop-off are closed gates, not design tasks. Reopen them only with a current repro and a failing targeted test.
- Use [release-builds.md](release-builds.md) as the source of truth for local build/install commands and release-gate command lines. This roadmap should name the relevant gate, not fork its own build procedure.
- Default to a vertical slice when there is no fresh regression: choose one chip, improve the audible/editable workflow, add or tighten the smallest useful guardrail, update the owning doc, then build/test/install for code changes. Do not pause to rediscover the backlog when the user has already reported a concrete issue.
- Keep moving chip-owned controls into their source cards. The most valuable UI work is still making the panel match the chip signal path: pulse controls under pulse voices, sample controls under sampler voices, wave controls under wavetable lanes, operator controls under FM operators, and shared hardware in shared modules.
- Use the project-local [Chipper JUCE UI skill](../skills/chipper-juce-ui/SKILL.md) before editor/layout work. General design skills can inspire aesthetics, but implementation decisions should stay grounded in JUCE Components, APVTS bindings, LookAndFeel tokens, host automation, and chip-native control behavior.
- The next deeper feature work should favor first-class editors: sample/wavetable editing for Paula, SPC700, HuC6280, Namco WSG, SCC, and Game Boy wave RAM; then FM operator/algorithm editing for the Yamaha family.
- Treat fixed FM sustain as a guardrail, not the FM roadmap. The next FM value is editability, algorithm clarity, and envelope terminology while keeping held-tail tests green.
- Treat screenshot feedback as a verification habit, not a permanent backlog. Once a layout bug becomes a durable rule in `ui-priority-audit.md`, the next code slice should apply that rule to the affected chip and then move on to the next playable workflow.
- Treat planning updates as maintenance, not product progress, unless they remove confusion or make the next implementation slice easier to execute. The default next step should still be a playable instrument improvement.
- Presets should grow from original sound design, not imported game data. Every new preset should be audible, visibly reflected in controls, provenance-safe, and easy to share as a flat file.
- Reliable preset expansion means hand-authored patches plus the sourcing/QA loop in [preset-sourcing.md](preset-sourcing.md): run `.\scripts\verify-presets.ps1 -BuildRoot build-codex`, audition the new preset, confirm loudness and sustain/one-shot behavior, confirm visible controls match the sound, and record any external sample dependency as a user-owned path instead of repository content. The catalog validator now keeps every implemented chip covered across the six core roles: lead, bass, keys/pad, arp, drums, and SFX.
- Update only the owning doc when possible. UI screenshot rules belong in [ui-priority-audit.md](ui-priority-audit.md), broad product workflow gaps belong in [product-gap-roadmap.md](product-gap-roadmap.md), verification/license evidence belongs in the accuracy and source-map docs, and release/build mechanics belong in [release-builds.md](release-builds.md).
- Keep this plan lean. Spend the next slice on playable instrument value: clearer controls, better sound design, stronger sample/wave/operator workflows, or better validation evidence.
- Do not re-audit the whole product when this queue already contains a concrete user-reported issue. Execute the highest-ranked known issue first, verify the affected chip plus one neighboring layout family, then return here only if the next slice is unclear.
- Preserve the DAW-fit fixed editor contract: most modes use 1240 x 820, SID currently uses 1240 x 880 for readable per-voice ADSR, and every mode keeps the 1180 px compact-width floor. If a page feels too cramped, rebalance sections, compact secondary macros, or add a deliberate secondary view instead of making the default VST taller or collapsing controls into unreadable strips.

## Furnace-Informed VST Priority Queue

Furnace is most useful to Chipper as a product/reference map, not as code or patch data to import. Its strongest lesson for this VST is workflow depth: fast sound selection, broad chip-local preset coverage, sample/wave/operator editing, and tracker-style motion should be first-class without breaking host automation, flat preset files, clean preset provenance, or the fixed DAW-fit editor contract.

Use this implementation order when no current regression is blocking release:

1. **Preset browser metadata and filtering.** Role, engine, favorite, and tag metadata are exported and validated, the VST header now has compact Role/Engine/Tag/Favorites plus search filtering, and original coverage/signature packs fill chip-local roles without importing Furnace data. The Furnace-inspired quality matrix of chip x count x role x engine/tag targets is exported and enforced, including chip-signature reference tags for techniques such as triangle/DMC, duty/wave/noise, PWM/sync/filter, rhythm/operator feedback, sampler loop/echo, and wavetable LFO/wave/noise coverage. The first diagnostic-driven packs deepened OPN2 DAC/envelope/chord coverage, DMG stereo/sweep/duty/noise coverage, HuC6280 LFO/chord/noise coverage, Paula raw-DAC/noise coverage, OPLL alarm/envelope coverage, SN76489 periodic/alarm coverage, YM2149 tracker/lead coverage, POKEY distortion/lead coverage, SCC noise/drum coverage, Namco WSG tracker/lead coverage, and SPC700 tracker/lead coverage. Saved user presets remain portable `.chipperpreset` files, but now carry role/engine/tag metadata so they participate in browser filtering and search; the VST also keeps local factory/user preset favorites for quick filtering and scans subfolders as user preset banks. Remaining browser work is larger bank operations such as rename/move/export flows.
2. **One sample/wave vertical slice.** Pick SPC700, Paula, or a wavetable family and complete the workflow: loaded asset state, lane/voice assignment, loop/root behavior, waveform preview, missing-file state, state recall, and renderer coverage. The Paula slice now imports uncompressed 8SVX VHDR loops, WAV `smpl` forward loops, AIFF `MARK`/`INST` sustain loops, and first-pass ProTracker MOD sample repeat points into status, preview, state recall, and playback loop state. NES DMC, SPC700, and Paula preset restores now surface missing external sample references in their sample status lines and waveform preview labels without failing the rest of the preset, and successful manual loads clear stale warnings. Paula renderer coverage now exercises repeatable `--paula-sample` banks plus per-channel sample-slot pins; full MOD playback/effects remain open.
3. **OPN2/OPM/OPN FM operator editor slice.** Start with YM2612 because ymfm is already vendored and six lanes are visible. The first passes made Operator EG role-aware, moved the stable Operator Tone and FM Level controls into Operator EG, added four APVTS/MIDI/preset/renderer-backed FM Operator Level trims that offset native total-level registers for OPN2 and OPM, added four neutral-default per-operator Multiplier menu controls that write native YM2612/YM2151 multiplier nibbles while `Follow` preserves Operator Tone, and added neutral-default per-operator Attack Rate, Decay Rate, Sustain Rate, and Release Rate menus that write native AR/D1R/D2R/RR values while `Follow` preserves Envelope Shape. The operator rows now carry compact level, multiplier, and envelope controls beside the carrier/modulator register readouts, and original FM preset passes now use level trims plus per-operator EG snapshots for OPN2/OPM basses, leads, keys, pads, bells, drums, and SFX. The first diagnostic-driven OPN2 expansion added original DAC snare, operator-envelope bell, and chord-pad presets, with descriptor/catalog/render JSON gates. The current FM pass promotes OPN2/OPM feedback from a continuous macro into a native 0-7 register menu and extends YM2203/OPN, YM2608/OPNA, and YM2610/OPNB from FM plus embedded SSG tone adapters into embedded SSG tone/noise/envelope adapters with renderer JSON, source-card, chip-poly, SSG-only render, preset, and docs coverage. Next, continue into OPNA/OPNB ADPCM/rhythm, chip-specific modulation, or the next candidate chips while keeping renderer JSON and held-tail checks green.
4. **Tracker motion and SFX gestures.** Add chip-aware arps, slides, vibrato, retrigger, and one-shot gestures as native-looking register changes. Keep the UI musical, but make tooltips/debug JSON show the resolved register path.
5. **Drum/percussion workflows.** Tighten NES DMC, SPC700 drum maps, Paula tracker maps, OPL/OPLL rhythm, YM/PSG noise kits, and POKEY percussion around playable banks instead of generic oscillator macros.
6. **Multi-output and source mixing.** Expose per-source routing/metering only after source-card ownership and level lanes are stable for the affected chip family.
7. **Strictness and reference QA.** Make Inspired/Hybrid/Authentic behavior choices explicit in engine/UI code, while verification claims stay in footer text, renderer metadata, and accuracy docs.
8. **Release finish.** Keep build/install scripts, VST3 packaging, screenshots, host scan notes, and preset QA in sync as the product surface stabilizes.

## Next Candidate Chip Priority List

Confidence is scored 1-10 and means "confidence this is a good next Chipper implementation investment," not a cycle-accuracy claim. Furnace may guide feature coverage, role vocabulary, and audition targets, but Chipper should not import Furnace code, patches, instruments, wavetables, or samples without a separate source/license audit. Furnace itself is GPLv2-or-later/GPLv3 depending on build options, so the safe default is reference-only behavior study plus original Chipper code and presets.

Current philosophy filter:

- Prefer one playable vertical slice over a large label dump: source-card lanes, presets, renderer JSON, tests, docs, and install verification.
- Extend existing Chipper families before adding near-duplicate modes. If the chip is already represented, deepen the surface instead of duplicating the mode.
- Keep expansions attached to their host identity, such as NES + VRC6 or NES + FDS, unless the chip naturally stands alone in a VST workflow.
- Add sample-heavy chips only after sample-bank, loop, note-map, missing-file, and preset provenance workflows are strong enough to make them useful.
- Treat software drivers and tracker engines as motion/preset inspiration, not as chip modes.

| Priority | Candidate | Confidence | Decision | Rationale |
| ---: | --- | ---: | --- | --- |
| 1 | NES + VRC6 | 9 | Implemented first expansion slice | Best first expansion slice: two pulses plus saw, strong musical identity, small UI/test surface, and no sample-bank dependency. Landed as a clean-room NES-attached expansion mode with source lanes, presets, renderer JSON, and Chip Poly coverage. |
| 2 | YM2608 / OPNA | 8 | Implemented current FM-family slice | Natural YM2203 superset with PC-98 value. The first vertical slice covers six FM lanes plus embedded SSG tone/noise/envelope lanes through ymfm; native rhythm, ADPCM-A, and ADPCM-B remain follow-up work. |
| 3 | YM2610 / OPNB and YM2610B / OPNB2 | 8 | Implemented first OPNB slice | Strong Neo Geo identity and close to OPNA concepts. The first vertical slice covers YM2610 four-FM plus embedded SSG tone/noise/envelope lanes through ymfm; external ADPCM-A/B and YM2610B/OPNB2 six-FM behavior remain major follow-up milestones. |
| 4 | NES + Famicom Disk System | 8 | Implemented first FDS slice | Single expressive wavetable/modulation expansion that fits the existing NES page and future wave editor direction. Landed as a clean-room NES-attached expansion mode with one FDS Wave source lane, wave/mod debug state, presets, renderer JSON, and Chip Poly coverage. |
| 5 | NES + Sunsoft 5B | 8 | Implemented current expansion slice | AY-style expansion fit Chipper because it reused NES source-card patterns and AY/PSG vocabulary. Landed as a clean-room NES-attached mode with three 5B tone lanes, pseudo-register metadata, presets, renderer JSON, and Chip Poly coverage. |
| 6 | NES + MMC5 | 8 | Implemented current expansion slice | Extra pulse channels and PCM are a clear NES expansion win with modest new UI complexity. Landed as a clean-room NES-attached mode with two MMC5 pulse lanes, one conservative PCM/DAC thump lane, pseudo-register metadata, presets, renderer JSON, and Chip Poly coverage across the pitched lanes. |
| 7 | NES + VRC7 / Family Noraebang OPLL | 8 | Implemented current expansion slice | OPLL was already present, so the first slice reuses the MIT emu2413 VRC7 patch table inside a NES-attached mode with pulse 1, pulse 2, triangle, six VRC7 melodic FM lanes, source cards, presets, renderer JSON, and Chip Poly coverage. NES Noise/DMC exposure, custom patch editing, rhythm behavior, mixer calibration, and golden/hardware validation remain follow-up work. |
| 8 | SAA1099 | 8 | Implemented current compact PSG slice | Distinct stereo PSG with envelopes and dual noise. Landed as a clean-room six-channel stereo PSG mode with source lanes, dual noise metadata, simplified envelope groups, presets, renderer JSON, and Chip Poly coverage. |
| 9 | PC Speaker | 8 | Implemented current beeper slice | Landed as a clean-room one-lane PIT channel 2 beeper mode with port 0x61 gate/data state, direct-click SFX, speaker-mode controls, original presets, renderer JSON, source-off coverage, and no Chip Poly claim. |
| 10 | ZX Spectrum beeper | 8 | Implemented current beeper slice | Landed as a clean-room one-lane ULA port FE beeper mode with EAR/MIC/border state, MIC-click SFX, beeper-mode controls, original presets, renderer JSON, source-off coverage, and no Chip Poly claim. |
| 11 | YM2203 / OPN, YM2608 / OPNA, and YM2610 / OPNB SSG noise and envelope polish | 8 | Implemented current deepening slice | FM plus SSG tone/noise/envelope lanes are in place for OPN, OPNA, and OPNB, now with honest SSG noise-period, per-channel tone/noise/off mixer, and envelope-shape controls without pretending the SSG side is a full AY editor. |
| 12 | YM3526 / OPL and YM3812 / OPL2 | 8 | Deepen existing OPL surface | Already covered through the OPL2/OPL3 adapter; improve operator-pair editing, rhythm behavior, and presets before adding more OPL labels. |
| 13 | YM2413 / OPLL | 8 | Deepen existing mode | Add custom patch, rhythm, and pitchable drum clarity before using OPLL as an NES expansion layer. |
| 14 | YM2151 / OPM | 8 | Deepen existing mode | High-value arcade/X68000 mode already exists; next value is better operator UI, LFO, noise, and presets. |
| 15 | YM2612 / OPN2 | 8 | Deepen existing mode | Keep improving DAC, operator envelopes, algorithm feedback, detune, and Genesis presets rather than adding a duplicate chip. |
| 16 | SID 6581/8580 | 8 | Deepen existing mode | Strong Chipper fit; keep expanding PWM/filter/sync/ring presets and model honesty. |
| 17 | Game Boy / DMG, including zombie envelopes | 8 | Deepen existing mode | Existing APU is valuable; software-envelope/zombie behavior belongs as truthful advanced DMG controls and presets. |
| 18 | POKEY | 8 | Deepen existing mode | Add AUDCTL pairing, high-pass, distortion validation, and Atari-specific preset coverage. |
| 19 | HuC6280 | 8 | Deepen existing mode | Existing six-lane wavetable core needs wave editing, noise/LFO clarity, and PC Engine presets. |
| 20 | Konami SCC/SCC+ | 8 | Deepen existing mode | Existing SCC adapter should gain wave editing, SCC+ distinction, key/volume masks, and Konami presets. |
| 21 | SNES / SPC700-style | 8 | Deepen existing mode | Avoid a second SNES label; deepen BRR, loop, ADSR/GAIN, echo/FIR, and sample-bank behavior. |
| 22 | Amiga / Paula | 8 | Deepen existing mode | Avoid a second Amiga label; deepen sample loops, tracker maps, MOD behavior, and drum kits. |
| 23 | YMF262 / OPL3 full 4-op | 7 | Implement after OPL editor depth | High value, but it should arrive with real operator-pair/four-op editing instead of a superficial mode switch. |
| 24 | AY8930 | 7 | Implement after PSG wins | Reasonable AY-family extension with lower pull than Sunsoft 5B, SAA1099, and existing AY polish. |
| 25 | NES + Namco 163 | 7 | Implement after wave editor groundwork | Great expansion identity, but multi-wavetable memory and voice-count tradeoffs want better wave editing first. |
| 26 | SegaPCM | 7 | Implement after sampler-bank hardening | Clear 16-channel arcade identity and good preset payoff once sample slots, loops, and missing-file states are fully comfortable. |
| 27 | Ricoh RF5C68 | 7 | Implement after sampler-bank hardening | Good Sega CD/FM Towns value; needs banked sample memory and loop behavior to feel first-class. |
| 28 | OKI MSM6258 and MSM6295 | 7 | Implement after one-shot sample workflow | Excellent drums/SFX chips, especially once ADPCM lanes, kits, and one-shot audition are strong. |
| 29 | Namco WSG/C15/C30 variants | 7 | Fold into existing Namco WSG depth | Keep as variant behavior and presets under the current Namco WSG roadmap instead of separate early headline modes. |
| 30 | WonderSwan | 6 | Implement later | Nice handheld wavetable identity, but lower near-term payoff than NES expansion and existing wavetable depth. |
| 31 | Seta/Allumer X1-010 | 6 | Implement later | Interesting wavetable/sample hybrid; best after the wave/sample editor pattern settles. |
| 32 | Yamaha Y8950 | 6 | Implement after OPL and ADPCM depth | OPL plus ADPCM is useful, but depends on both OPL operator depth and sample/ADPCM workflow. |
| 33 | Yamaha YMZ280B / PCMD8 | 6 | Implement after stronger sample-chip pattern | Useful PCM target, but less immediate identity than SegaPCM, RF5C68, and OKI. |
| 34 | Konami K007232 | 6 | Implement after arcade sample pattern | Good arcade color; wait until generic sample-chip lane conventions are proven. |
| 35 | Konami K053260 | 6 | Implement after arcade sample pattern | Good follow-up to K007232 or SegaPCM once banks/loops/mixing are established. |
| 36 | Irem GA20 | 6 | Implement after arcade sample pattern | Viable sample chip with moderate recognition and lower preset pull. |
| 37 | Namco C140 | 6 | Implement after sample-chip foundation | Strong arcade follow-up once Namco/sample-chip architecture is deeper. |
| 38 | Namco C219 | 6 | Implement after C140/sample foundation | Similar family value, best treated as a later variant/follow-up. |
| 39 | Capcom QSound | 6 | Implement later | High recognition and spatial identity, but needs a broader sample/banking/mixing surface. |
| 40 | OKI MSM5232 | 6 | Implement later | Distinct arcade tone chip, but lower near-term demand than SAA1099 or NES/OPN work. |
| 41 | Mikey / Atari Lynx | 6 | Implement later | Good handheld identity; useful once wavetable/sample lanes and presets are deeper. |
| 42 | TIA / Atari 2600 | 6 | Implement later as SFX-focused | Strong character and TIunA-style pitch ideas, but should be framed as constrained SFX/tuning behavior. |
| 43 | Game Boy Advance DMA two-channel mode | 6 | Implement later | Useful sample-channel identity, but starts to feel like a console subsystem unless tightly scoped. |
| 44 | YM2414 / OPZ | 6 | Implement later | Attractive TX81Z-style four-op FM target; wait until OPN/OPM operator UI and detune are stronger. |
| 45 | Commodore VIC | 5 | Defer | Characterful but niche; follow stronger PSG/beeper and C64/SID work. |
| 46 | TED / Commodore Plus/4 | 5 | Defer | Useful Commodore flavor, but lower pull than SID depth or compact PSG wins. |
| 47 | Virtual Boy | 5 | Defer | Fits wavetable philosophy, but lower demand and preset payoff than other handhelds. |
| 48 | Konami Bubble System WSG | 5 | Defer | Historically interesting; better as later Konami wavetable flavor after SCC. |
| 49 | YMF278 / OPL4 | 5 | Defer | Requires both full OPL3 depth and sample-channel architecture; not a near-term slice. |
| 50 | ESS ESFM | 5 | Defer | Good after solid OPL3 support, but not before first-class OPL3 four-op. |
| 51 | Ensoniq ES5506 | 5 | Defer | Powerful but larger/less chip-instrument-shaped; needs mature sample voice architecture. |
| 52 | Commodore PET | 4 | Defer | Small historical flavor, low immediate VST payoff. |
| 53 | Casio PV-1000 | 4 | Defer | Niche enough to wait for a specific user story. |
| 54 | Pokemon Mini | 4 | Defer | Fun but narrow; not a better investment than current chip families. |
| 55 | Sharp SM8521 / Tiger Game.com | 4 | Defer | Low recognition and low near-term preset value. |
| 56 | Watara Supervision | 4 | Defer | Low demand compared with other handheld and PSG targets. |
| 57 | Nintendo DS | 4 | Defer | Too broad for the current single-instrument chip-mode scope until sampler engines and bank state are much stronger. |
| 58 | SFX-like engine and QuadTone engine | 5 | Translate into motion tools | Good behavior inspiration for beeper presets, arps, and one-shot gestures; not a clean hardware chip mode. |
| 59 | TIunA software tuning engine | 4 | Translate into motion tools | Useful for TIA pitch/tuning ideas, but should not become a standalone chip. |
| 60 | MinMod software driver | 4 | Translate into sampler/tracker tools | Useful GBA tracker-driver reference, not a chip implementation target. |

The current implementation wave has landed **NES + VRC6**, **YM2608/OPNA**, the first **YM2610/OPNB** playable slice, **NES + FDS**, **NES + Sunsoft 5B**, **NES + MMC5**, **NES + VRC7**, **SAA1099**, **PC Speaker**, **ZX Spectrum beeper**, and **OPN/OPNA/OPNB SSG noise/envelope polish**. If we prefer depth over more labels, the best slices are **OPNA/OPNB ADPCM and rhythm**, **OPL/OPLL/OPM/OPN2 operator/rhythm/custom-patch polish**, **VRC7 custom patch/rhythm/mixer polish**, **PC Speaker/ZX reference and golden validation**, and **SAA1099 golden/reference validation plus envelope/noise polish**. The remembered next new-chip targets are **NES + Namco 163** after wave-editor groundwork and **SegaPCM**, **RF5C68**, and **OKI MSM6258/MSM6295** after the sample-bank workflow feels boringly reliable.

## Current Next-Best Priority List

| Rank | Candidate | Confidence | Why this is next-best for Chipper |
| ---: | --- | ---: | --- |
| 1 | OPNA rhythm plus ADPCM-A/B | 7 | YM2608 is already in the ymfm family and PC-98 rhythm/ADPCM would unlock a very recognizable preset layer, but it needs careful sample/one-shot workflow and honest rhythm controls. |
| 2 | OPNB ADPCM-A/B plus OPNB2 behavior | 7 | YM2610 already has FM plus SSG lanes; Neo Geo identity wants ADPCM, but external sample provenance, banking, and YM2610B six-FM behavior need a tidy vertical slice. |
| 3 | OPL3 full 4-op/operator-pair polish | 7 | The YMF262 core is already present; the missing value is a real OPL3 editing surface with 2-op/4-op pairing, stereo lanes, and rhythm clarity. |
| 4 | YM2413/OPLL custom patch and rhythm polish | 8 | emu2413 is already vendored and OPLL/VRC7 share product vocabulary; a single shared custom patch editor and clearer rhythm surface fit the VST philosophy. |
| 5 | YM2151/OPM LFO, detune, noise, and operator depth | 8 | OPM is implemented and musically important; LFO/AMS/PMS, DT1/DT2, and channel-8 noise polish are high-value without adding a new chip label. |
| 6 | YM2612/OPN2 DAC, LFO, detune, and deeper operator presets | 8 | Genesis FM is a flagship mode, and further DAC/operator polish improves existing presets more than another near-duplicate FM chip would. |
| 7 | SID PWM/filter/sync/ring preset depth and model validation | 8 | The SID surface already matches Chipper’s chip-native philosophy; better original presets and validation raise quality without major new architecture. |
| 8 | Game Boy zombie envelopes and sweep/noise polish | 8 | DMG is already implemented, and advanced envelope/sweep behavior can be exposed as truthful, compact controls instead of a full tracker engine. |
| 9 | POKEY AUDCTL/high-pass/distortion validation | 8 | Existing POKEY support can become much more Atari-specific with focused register controls and validation. |
| 10 | HuC6280, SCC/SCC+, and Namco WSG wave-editor depth | 8 | Existing wavetable chips need a shared wave editing pattern before more wavetable expansions are added. |
| 11 | SPC700 BRR loop, ADSR/GAIN, echo/FIR, and sample-bank polish | 8 | Current sample-chip value rises sharply once imported/sample-bank behavior is boringly reliable and testable. |
| 12 | Amiga/Paula sample-loop and tracker-map polish | 8 | Paula is already a good sampler workflow proving ground; deeper loop and kit behavior helps every later sample chip. |
| 13 | NES + Namco 163 | 7 | Best next new chip once wave editing exists; the voice-count/wave-RAM tradeoff should be first-class rather than bolted onto the NES page. |
| 14 | SegaPCM and RF5C68 | 7 | Strong arcade/Sega sample identities; implement after sample-bank slots, loops, and missing-file states are sturdy. |
| 15 | OKI MSM6258/MSM6295 | 7 | Excellent one-shot drum/SFX payoff, but ADPCM import, audition, and kit workflows should be established first. |

## Next Code Slices To Prefer

When there is no fresh P0 regression, use this order for the next development slice:

1. Finish the current DAW-fit layout debt without changing the default editor height. Known high-value targets are sampler Performance Macro/sample-bank overlap, standard-height dropdowns/inputs, and any remaining chip pages where visible controls are still clipped or hidden.
2. Polish one sampler or wavetable chip end-to-end instead of spreading tiny changes across all chips. Best current candidates are Paula sample-bank workflow, SPC700 voice-to-sample mapping, or HuC6280/Namco/SCC per-lane wave editing.
3. Extend regression coverage only where the current gates do not already protect the behavior. `descriptor_smoke` already covers source-card ownership, source levels, MIDI CC mappings, per-channel wave/sample selectors, named chip modes, and factory preset presence; the next useful checks should target remaining engine-visible behavior such as sample-bank loop state, helper-envelope output, preset audibility, and state recall.
4. Expand original factory presets only after the target chip's visible controls and renderer/debug metadata can prove what the preset changed. Furnace can guide coverage targets, role names, audition vocabulary, and chip-feature checklists, but Chipper presets must remain original parameter snapshots rather than copied tracker instruments, modules, wave tables, or sample data.
5. Continue the FM operator editor as contained vertical slices: OPN2/OPM/OPN/OPNA/OPNB now have editable Operator/SSG Tone where relevant, FM/SSG Level, four neutral-default per-operator Level trims, four neutral-default per-operator Multiplier menus, neutral-default per-operator Attack Rate, Decay Rate, Sustain Rate, and Release Rate menus, feedback menus, and OPN/OPNA/OPNB SSG tone/noise/envelope exposure with automation, preset, renderer, MIDI, compact Operator EG UI coverage, and curated original factory balances. The next step should be OPNA rhythm/ADPCM, OPNB ADPCM/OPNB2 behavior, chip-specific modulation, or clearer operator-grid visualization.
6. If the next slice should be a new chip instead of depth, choose **NES + Namco 163** after wave-editor groundwork or **SegaPCM/RF5C68/OKI MSM6258/MSM6295** after sample-bank hardening. Keep new chips scoped as honest source-lane vertical slices with original presets and renderer trace coverage, not broad machine emulation.

If a protected behavior is suspected again, first run the named gate. A passing gate leaves the plan unchanged; a failing gate becomes a P0 regression, gets one owner-doc update, and must leave behind a tighter test before returning to the fixed list.

| Rank | Todo | User Value | Effort | Confidence | Confidence Improves With |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | Finish the all-chip readability pass inside the chip-specific DAW-fit contract (820 px default, documented SID 880 px ADSR exception) and 1180 px compact-width floor: no overlapping text, standard-height dropdowns/inputs, visible level controls, no dead placeholder modules, and no off-screen default windows. | 10 | 4 | 9 | Screenshot checks for every chip after each layout change and the editor-size smoke gate. |
| 2 | Keep chip-native controls owned by their source cards. Pulse duty belongs under pulse voices, wave/sample selectors under wavetable or sampler voices, level controls stay visible in every source card, and noise modes belong under noise voices. | 10 | 5 | 9 | Reusable source-card layout helpers and descriptor-smoke guards for source-card ownership. |
| 3 | Make sample and wavetable chips feel first-class: Paula, SPC700, HuC6280, Namco WSG, and SCC need per-channel sample/wave selection, visible level, waveform preview, root/loop behavior, and missing-asset states. | 10 | 7 | 8 | More renderer coverage for sample-slot selection, note-map behavior, loop flags, and state recall. |
| 4 | Expand preset value: more high-quality factory presets, role/category filtering, user preset save/load, portable flat files, and audible preset regression checks. | 9 | 5 | 8 | Preset metadata validation, loudness checks, and a small hand-audition checklist for each chip. |
| 5 | Build real FM editor surfaces for YM2612/OPN2, YM2151/OPM, OPL2/OPL3, and YM2413/OPLL. Macros can stay, but serious users need operator/algorithm/envelope access. | 9 | 8 | 7 | Operator-grid UI spike, kept-green held-tail regression checks, and register/debug JSON for algorithm, level, feedback, and envelope state. |
| 6 | Improve NES/RP2A03 authenticity and usability around DMC playback, sweep edge cases, frame sequencing, nonlinear mixer tolerances, and test trace coverage. | 9 | 7 | 8 | Local development samples, test ROM references, emulator comparison renders, and golden metadata for clean-room behavior. |
| 7 | Add chip-aware tracker motion and SFX gestures that write native-looking register changes instead of generic modulation. | 9 | 7 | 7 | A per-chip destination map, renderer traces for common gestures, and UI copy that shows the resolved register path. |
| 8 | Enforce Strictness behavior consistently. The header control should choose Inspired/Hybrid/Authentic behavior; the footer verification label should state what is actually tested. | 8 | 5 | 8 | Descriptor metadata for allowed controls, host-state recall tests, and docs reviewed against current test coverage. |
| 9 | Expand verification coverage across implemented chips: pitch, envelope/EG, noise, mixer, sample/wave selection, source-enable, MIDI CC, preset, sustained-note, loop-off, and debug JSON assertions. | 8 | 7 | 8 | Compact assertion helpers, the existing descriptor/processor smoke tests, held-tail assertions, and reference metadata exported from `chipper_render --list-descriptors`. |
| 10 | Keep the license/provenance audit current before importing or deriving from any outside emulator, waveform, table, preset dump, or sample asset. | 8 | 5 | 9 | Exact upstream URL, revision, license file, file-level notes, and `THIRD_PARTY_NOTICES.md` entries before code enters the tree. |
| 11 | Add hardware/reference comparison documentation and tolerance thresholds for each implemented chip. | 7 | 8 | 5 | Real hardware recordings, reproducible capture settings, accepted emulator comparisons, and agreed spectral/timing tolerances. |
| 12 | Release readiness: installer clarity, GitHub tag builds, DAW scan guidance, platform build notes, and compatibility QA for Windows, Linux, and macOS. | 7 | 6 | 7 | Keep [release-builds.md](release-builds.md), the GitHub Actions release workflow, artifact naming, smoke tests, and host verification notes aligned. |

## Current State Snapshot

- `Arcade Hybrid` and `Custom` are intentionally removed from the chip selector. Arcade sounds should come from named hardware families such as YM2149, SN76489, SAA1099, Namco WSG, SCC, YM2151, and OPL.
- Any unknown or retired chip descriptor is migration-only. It should help old projects resolve to a named hardware-family mode, not appear as a roadmap target, preset bank, or semi-playable generic engine.
- The top control is **Strictness**, not an accuracy claim. It changes how hard Chipper leans into native limits. The small footer status is where verification language belongs.
- Channel-local register controls must stay inside the channel/voice cards wherever practical. Detached summary panels are only for genuinely shared hardware such as filters, echo, sample banks, output routing, and cross-channel pairing.
- Presets are the product's main fast path. A preset should be audible, chip-local, editable, shareable, and visibly represented by the controls after it loads. Do not add preset names for non-playable cores.
- Roadmap-only ideas belong in docs until they have an audible engine path, stable parameters, preset recall, and renderer or descriptor coverage. The executable chip selector and preset browser should stay focused on named, playable hardware-family surfaces.
- Factory presets and user presets must not embed copyrighted samples, ROM data, copyrighted wave dumps, or derived game assets. External sample paths stay user-owned and outside the repository.
- High-quality factory presets should be original Chipper patch design based on chip behavior, public hardware documentation, common synthesis techniques, and hand-authored sound design. Do not copy commercial game patches, ripped instrument data, tracker modules, ROM wave tables, or sample assets into presets unless the license and provenance are explicitly compatible and documented.
- Current planning assumes recent layout passes are the baseline, not the destination. Remaining UI work should prioritize controls that are still hard to see, any chip-owned controls that still live in shared panels, and chip families that still need deeper editor surfaces.
- If a previously fixed bug is suspected again, verify it first with the targeted regression before adding new roadmap work.

## Current Cleanup Targets

These are the highest-signal cleanup items after the latest all-chip screenshot pass:

- Keep sampler layouts collision-free. Paula and SPC700 now reserve extra module height for sample-bank editing and use a shorter Performance Macros strip so waveform previews, root/playback controls, loop state, per-voice sample controls, and macros do not collide. If those compete, the sample editor wins and the macro strip moves or grows.
- Keep the plugin DAW-fit first. Most chip pages open, clamp, and advertise the 820 px default height as the resize maximum so high-DPI hosts do not reopen Chipper off-screen. SID is the documented 880 px exception for readable per-voice ADSR; future exceptions need a named layout reason and smoke-test coverage.
- Keep wavetable cards readable. HuC6280, Namco WSG, and SCC now keep per-lane wave selectors and visible level controls in each source card; if future per-lane pitch, pan, gate, or wave-edit controls do not fit, grow the layout rather than hiding controls.
- Keep helper envelopes honest and visible. POKEY, Paula, HuC6280, Namco WSG, and SCC helper envelopes are Chipper musical volume helpers, not native ADSR. They should render as standard controls with chip-specific readouts, never as empty title-only panels.
- Keep shared output controls complete. If a chip uses one output module for both a tone/spread slider and a route/LFO segment, reserve enough height for the label, the actual control, and the readout; showing only the label is a UI regression.
- Keep channel ownership intact. NES/DMG pulse duty, noise mode, wave level, wavetable shape, sampler slot, and per-source level belong in the source card that owns the sound path. Descriptor smoke tests already pin the current ownership rules; add new assertions when a new chip-local control becomes visible.
- Keep source-card controls visually grouped. Wavetable and sampler source cards should place wave/sample selectors and their level lane together; level sliders must not float at the bottom of an oversized card with a dead middle.
- Keep wavetable source decks compact and bounded. HuC6280, Namco WSG, and SCC cards should reserve enough height for waveform preview, selector, and level, but should not collapse into unreadable slices or stretch into tall blank panels; the editor smoke gate now guards a tighter readable card-height band.
- Keep Performance Macro readouts concise on-screen. Full register/debug text belongs in tooltips, status summaries, renderer JSON, or docs; compact value labels should show the musical result without stealing layout space from the actual control.
- Keep FM surfaces playable while the deeper operator editor is planned. Macro/editor passes must not disturb key-on, source-level, or operator-envelope sustain behavior. The next FM value is editable operator structure, not re-solving held-note sustain unless a regression is reproduced.
- Keep docs and UI labels aligned: the header says **Strictness**; verification strength belongs in the footer, renderer debug JSON, and accuracy docs.
- Keep shared host/CC names neutral when one stable parameter serves different chip meanings. For example, CC94 is `Chip Choice / Route` at the host layer, while each chip panel relabels it as DMG Stereo Route, SID Model, SPC700 loop behavior, POKEY AUDCTL pairing, HuC6280 LFO mode, FM pan/routing, or a wavetable lane selector as appropriate.
- Keep the standard control-size baseline: source-card dropdowns, sample-bank dropdowns, numeric boxes, and level lanes must remain readable at the default editor size. If a future chip-specific control needs more room, grow the card or layout instead of shrinking controls back into clipped mini rows.
- Keep NES compact without hiding important controls. The channel cards own pulse/noise controls, the DMC module owns sample browsing and waveform preview, and Performance Macros use a two-row layout where APU Decay sits beside Output so their sliders and readouts cannot overlap.
- Keep preset sourcing clean. More factory presets are valuable, but the safe path is original patch design plus renderer/debug metadata, not importing copyrighted game sounds, tracker instruments, ROM data, or sample banks.
- 2026-06-19 verification pass: the current DAW-fit editor baseline builds, passes the editor geometry smoke test, passes the processor MIDI/CC smoke test, and passes the focused NES DMC renderer smoke/assert set. NES DMC loop-off behavior is treated as one-shot byte stepping plus final DAC hold, not sample wrap.

## Immediate Execution Checklist

Use this checklist before calling any slice done. It is intentionally small enough to run often.

- **Readability:** inspect the affected chip plus one neighboring chip for overlapping labels, clipped dropdowns/inputs, invisible level lanes, and stale hidden-module text.
- **Audibility:** load at least one preset for the affected chip and confirm the visible controls represent the sound that is playing.
- **Sustain:** when an FM chip is touched, run the held-tail subset (`ctest --test-dir build-codex -C Release -R "held_tail|preset_.*held" --output-on-failure`) or manually verify a held note does not decay to silence unless the selected envelope clearly asks for that.
- **Samples:** when NES DMC, SPC700, or Paula sample handling is touched, verify one-shot vs loop behavior, note-map/manual-slot behavior, and missing-file UI states. For NES DMC, keep `ctest --test-dir build-codex -C Release -R "processor_midi_cc_smoke" --output-on-failure` green because it now acts as the loop-off regression contract. The NES UI should label the checkbox as `Loop Sample`; unchecked means one-shot playback stops byte stepping at the sample end while the register/DAC state holds its final value, and checked means the `$4010` loop bit wraps the loaded sample. A completed loop-off one-shot must not keep the plugin voice audibly open after key release.
- **State:** switch away from the chip and back, then confirm the chip-local settings, preset choice, and sample/wave selections are preserved.
- **Honesty:** update docs and footer/descriptor wording whenever implementation evidence changes; do not promote a mode beyond what tests or references support.
- **Release hygiene:** for code changes, build `Chipper_VST3`, run targeted tests, install from the same build root, and verify the footer hash against the installed marker before committing.
- **Docs-only hygiene:** for planning, README, or release-note-only changes, run `git diff --check` and skip VST rebuild/install unless the docs reveal a code mismatch that was also changed.

## Regression Gate Quick Reference

Fixed regressions are release gates, not active roadmap items. Keep them named in tests and docs so they can be checked quickly, but do not let them crowd out forward product work unless a current build reproduces the problem.

| Gate | Current Status | Protective Check |
| --- | --- | --- |
| FM held notes fading to silence | Fixed | `ctest --test-dir build-codex -C Release -R "held_tail|preset_.*held" --output-on-failure` |
| NES DMC one-shot samples looping with Loop off | Fixed | `ctest --test-dir build-codex -C Release -R "processor_midi_cc_smoke" --output-on-failure` |
| Factory presets becoming silent or metadata-invalid | Preset regression | `.\scripts\verify-presets.ps1 -BuildRoot build-codex` |
| Reappearing clipped, hidden, or overlapping controls | UI regression | All-chip screenshot pass plus the checklist in [ui-priority-audit.md](ui-priority-audit.md) |

The complete local command set lives in [release-builds.md](release-builds.md). When a fixed regression is suspected, update this section only after reproducing it in the current build. Otherwise keep active priority on forward user value: deeper source editors, better presets, clearer sample/wave/operator workflows, and stronger verification evidence.

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
| 6 | Philips SAA1099 | 8 | Current code is internal clean-room partial from public register documentation; Furnace, SAASound, and MAME remain reference or audit targets only. | Validate against references, tighten envelope/noise timing, keep six source lanes readable, and deepen stereo PSG presets. |
| 7 | PC Speaker | 8 | Current code is internal clean-room partial from public PIT/port 0x61 documentation; Furnace, DOSBox-style projects, MAME, and PCem/86Box remain reference or audit targets only. | Validate PIT edge behavior against references, tune click/gate gestures, keep the single speaker source lane honest, and deepen beeper/SFX presets without adding fake polyphony. |
| 8 | ZX Spectrum Beeper | 8 | Current code is internal clean-room partial from public ULA port FE documentation; Furnace, Fuse, MAME, and SpecEmu-style projects remain reference or audit targets only. | Validate EAR/MIC timing against references, tune MIC/pulse-train gestures, keep the single beeper source lane honest, and deepen beeper/SFX presets without adding fake polyphony. |
| 9 | SNES SPC700-style | 8 | Current code is internal clean-room partial. emu-rs/snes-apu and snes-echo are permissive-looking audit candidates; C700, blargg snes_spc, GME, Furnace, and MAME remain reference or license-sensitive audit targets only. | Deepen the current eight-voice sample surface with clearer per-voice loop/envelope/noise/echo state, exact BRR loop semantics, Gaussian/output validation, and honest echo/FIR controls. |
| 10 | Atari POKEY | 7 | Current code is internal clean-room partial; web-pokey remains a permissive candidate/reference until audited. | Expand beyond the first AUDF/AUDC/AUDV pass with clearer distortion-code UI, AUDCTL pairing/high-pass controls, and Atari preset vocabulary. |
| 11 | Amiga Paula | 8 | Current code is internal clean-room partial; pt2-clone/libxmp/Furnace remain reference or audit targets only. | Deepen the current four-channel tracker sample surface with visible period/loop/volume state, MOD-style retrigger vocabulary, imported uncompressed 8SVX, WAV `smpl`, AIFF `MARK`/`INST`, and first-pass ProTracker MOD sample loop metadata, and remaining full-MOD playback/effect workflow checks. |
| 12 | PC Engine HuC6280 | 7 | Current code is internal clean-room partial; MAME/GME/Furnace/HuSIC remain reference or audit targets only. | Deepen the current six-lane wavetable surface with editable 32-sample Wave RAM, native noise, Ch 1/2 LFO clarity, per-lane pitch/pan where appropriate, and PC Engine preset vocabulary. |
| 13 | Namco arcade WSG | 7 | Current code is internal clean-room partial; MAME/Furnace remain reference or audit targets only. | Deepen the current eight-lane WSG surface with 4-bit Wave RAM editing, enable/volume masks, voice-count variants, tuning controls, and arcade preset vocabulary. |
| 14 | Konami SCC | 7 | Current audio path is backed by vendored MIT emu2212; exact SCC/SCC+ quirks still need golden/reference validation. | Deepen the current five-lane SCC wavetable surface with Wave RAM editing, key/volume controls, SCC/SCC+ distinctions, and deeper Konami preset vocabulary. |
| 15 | YM2612 / OPN2, YM2203 / OPN, YM2608 / OPNA, YM2610 / OPNB, OPL2/OPL3, YM2151 / OPM, and YM2413 / OPLL | 8 | ymfm is now vendored for first YM2612, YM2203, YM2608, YM2610, OPL2, and YM2151 melodic adapters; emu2413 is vendored for OPLL. Keep OPNA rhythm/ADPCM, OPNB ADPCM/OPNB2 behavior, OPL3-specific, custom OPLL patch, and rhythm behavior inside the audited source/licensing plan. | Move from macro/operator summaries to real operator/algorithm UI with envelope visuals, carrier/modulator clarity, preserved held-tail regression coverage, and chip-specific FM/SSG artifacts. |

## Wide Chip Implementation Queue

Going wide is useful now, but only if planned modes stay honest. Planned chip descriptors should expose roadmap templates, source strategy, UI shape, and blockers while continuing to render silence or unsupported-core output until a verified core exists.

| Order | Chip Family | First Value To Deliver | Likely Source Path | UI Shape | Confidence |
| --- | --- | --- | --- | --- | ---: |
| 1 | YM2612 / OPN2 | Real Genesis-style FM bass/lead core with operator automation. | First BSD-3-Clause `ymfm` adapter is in place; keep Nuked-OPN2 as LGPL-sensitive comparison. | Six FM voice strips with algorithm, feedback, operator envelope/level, DAC, and stereo. | 8 |
| 2 | OPL2/OPL3 | DOS FM instruments and rhythm-mode kits. | First BSD-3-Clause `ymfm` OPL2/YM3812 adapter is in place; keep Nuked-OPL3 as LGPL-sensitive comparison. | Operator-pair editor with waveform selector, rhythm mode, feedback, tremolo/vibrato, and per-operator envelopes. | 8 |
| 3 | YM2149 / AY polish | Stronger early-computer beeps, arps, and noise percussion from a vendored PSG core. | Continue the `emu2149` adapter and compare against references before stronger claims. | Three channel strips plus shared noise/envelope shape visualization. | 8 |
| 4 | SN76489 polish | Better Sega PSG/arcade immediacy from an emu-backed core. | `emu76489` is now vendored under MIT; FigBug/SN76489 remains LGPL-sensitive reference. | Three tone strips plus noise strip with attenuation and noise-control choices. | 8 |
| 5 | Game Boy / DMG polish | Game Boy pulse/wave/noise patches with clearer Wave RAM and stereo routing. | Continue clean-room; SameBoy only after file-level audit; FigBug/PAPU remains GPL reference. | Four APU lanes with pulse sweep, Wave RAM view, NR32 level, NR51 routing, and noise width. | 7 |
| 6 | Amiga Paula deeper pass | Make the partial tracker-sampler core feel like a first-class Amiga instrument. | Continue clean-room; audit pt2-clone/libxmp/Furnace references before any reuse. | Four sample channels with per-channel sample/shape pins, period, loop, volume, hard-pan identity, retrigger, sample browser, imported uncompressed 8SVX, WAV `smpl`, AIFF `MARK`/`INST`, and first-pass ProTracker MOD sample loop metadata, and remaining full-MOD playback/effect checks. | 7 |
| 7 | SPC700-style deeper pass | Make the partial lo-fi sample-voice core feel like a first-class SNES-style instrument. | Continue clean-room; audit emu-rs/snes-apu and snes-echo first for permissive paths, and keep C700/blargg snes_spc/GME/Furnace/MAME reference-only unless licensing is settled. | Eight sample voices with per-voice sample slots, ADSR/GAIN, pitch modulation/noise status, Gaussian/output filtering, echo/FIR controls, 64 KB RAM budget, and clear "style" labeling. | 6 |
| 8 | YM2151 / OPM | Arcade/X68000 four-op FM leads and metallic basses. | First BSD-3-Clause `ymfm` OPM melodic adapter is in place. | Eight four-operator voices with algorithm, feedback, LFO PM/AM, noise, and per-operator envelopes. | 8 |
| 9 | YM2203 / OPN SSG polish | Complete the OPN SSG side by adding honest noise/envelope controls without muddying the FM operator surface. | Continue the BSD-3-Clause `ymfm` adapter already used for the FM+SSG slice. | Three FM voices plus three SSG tone/noise/envelope voices are in place; next add timer/prescaler validation, CSM, and golden comparisons before stronger accuracy claims. | 8 |
| 10 | YM2608 / OPNA | PC-98 six-FM plus embedded SSG vocabulary with a clear route toward rhythm and ADPCM. | First BSD-3-Clause `ymfm` OPNA adapter is in place. | Six FM strips plus three SSG tone/noise/envelope lanes now; next add native rhythm, ADPCM-A/B, and golden comparisons. | 8 |
| 11 | YM2610 / OPNB | Neo Geo four-FM plus embedded SSG vocabulary with a clear route toward ADPCM and OPNB2. | First BSD-3-Clause `ymfm` OPNB adapter is in place. | Four FM strips plus three SSG tone/noise/envelope lanes now; next add external ADPCM-A/B, YM2610B/OPNB2 six-FM behavior, and golden comparisons. | 8 |
| 12 | YM2413 / OPLL | Preset-FM quick sounds and UI/game chimes. | Audit `ymfm` and `emu2413`. | Preset instrument selector plus limited editable patch/rhythm controls. | 7 |
| 13 | SCC deeper pass | Turn the emu2212-backed partial Konami SCC/SCC+ core into a first-class five-channel wavetable instrument. | Continue the emu2212 adapter; add golden/reference comparisons before stronger accuracy claims. | Five wavetable strips with wave edit/slot, visible level, tuning, key mask, SCC/SCC+ behavior, and stack/spread macros. | 8 |
| 14 | HuC6280 deeper pass | Turn the partial PC Engine core into a first-class six-channel wavetable instrument. | Continue clean-room; audit MAME/GME/Furnace/HuSIC before any reuse. | Six wavetable channel strips with wave RAM preview/editing, visible level, native noise, Ch 1/2 LFO behavior, channel balance, and source scopes. | 6 |
| 15 | Namco WSG deeper pass | Turn the partial Namco WSG core into a first-class arcade wavetable instrument. | Continue clean-room; audit any MAME/Furnace-derived references before reuse. | Wavetable lane banks with 4-bit waveform memory, visible levels, voice-count variants, enable masks, tuning, and source scopes. | 6 |
| 16 | POKEY deeper pass | Improve the partial Atari core. | Continue clean-room; web-pokey/MAME/Furnace reference-only unless audited. | Add AUDCTL channel pairing, high-pass filters, stronger polynomial validation, and source-card visuals. | 6 |
Confidence improves most by doing small adapter spikes, adding renderer traces before UI exposure, and recording exact upstream commit/license details before any third-party source enters the tree.

Arcade-style sounds should come from named chip modes such as YM2149, SN76489, SAA1099, Namco WSG, SCC, and YM2151 rather than from an undefined generic arcade mode.

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
- Build `Chipper_VST3` and `chipper_render` for code changes, run the relevant tests, install the VST3 from the same build root, verify the installed marker when practical, commit, and push. For docs-only changes, run `git diff --check`; do not spend a build/install cycle unless code changed too.
- Avoid product claims stronger than the current renderer tests and documented verification support.
- Update only the docs that own the changed behavior: `ui-priority-audit.md` for layout/control placement, `product-gap-roadmap.md` for broad workflow gaps, `product-spec.md` for user-facing product contract, `emulation-accuracy.md` and `emulator-source-map.md` for accuracy/source posture, and `release-builds.md` for build/install/release workflow.
- Keep fixed bugs listed as regression gates with named tests, not as repeated open roadmap items.
- Keep planning docs lean: remove stale "planned-only" wording when a chip becomes playable, and avoid copying the same fixed-regression note into multiple active queues.
