# Chipper Product Gap Roadmap

This roadmap captures the broad product gaps that remain after the first playable-chip and UI-polish passes. The guiding question is:

> Can a musician make a complete chiptune bass, lead, arp, drum kit, and SFX pack in Chipper without fighting the UI?

## Current Baseline To Preserve

Recent work has converted many early placeholder panels into playable, chip-aware surfaces. Treat these as non-regression requirements while moving forward:

- FM modes should sustain normally; the previous "one note fades to silence" issue is treated as fixed and covered by held-tail regression tests that assert renderer `tailRms` for YM2612/OPN2, OPL2/OPL3, YM2151/OPM, YM2413/OPLL, and key FM factory presets. Any return of that behavior is a P0 regression, not an accepted limitation or normal FM envelope behavior.
- NES DMC one-shot mode should not loop when the Loop control is off. The authentic behavior to communicate is "bit stream stops, DAC holds final level." The `processor_midi_cc_smoke` test covers this behavior for manual-slot and note-map playback so it can be checked quickly when DMC UI or sample playback changes.
- Sample and wavetable chips should keep per-voice wave/sample selectors and visible level controls in the voice cards wherever the chip has independent channels.
- Strictness is a behavior request, not a proof label. Verification strength remains in the footer and docs.
- Factory and user presets are real user value only when they load audible, playable states and visibly update the chip controls.
- Source cards are now the baseline home for chip-owned controls. Do not move per-channel duty, wave, sample, or level controls back into detached summary panels unless the chip hardware really shares that control.
- Roadmap-only chip features should remain in docs until they have an audible engine path, stable parameters, state recall, and renderer or descriptor coverage. The plugin UI should favor implemented, truthful surfaces over planned-looking controls.

Do not spend new planning cycles on fixed regressions unless they are reproduced in the current build. Instead, keep them as smoke-test checks while pushing the remaining work: deeper chip editors, preset quality, sample/wave workflows, and verification evidence. If a user reports a fixed issue again, first run the targeted regression and then decide whether the roadmap needs to change.

Fast smoke command for the fixed-regression gates:

```powershell
ctest --test-dir build-codex -C Release -R "chipper_descriptor_smoke|processor_midi_cc_smoke|held_tail|preset_.*held" --output-on-failure
```

## Highest-Value Product Gaps

1. **Chip-Aware Tracker Motion / SFX Gestures**
   - User value: very high. Chiptune identity often comes from fast note and register gestures, not only static tone.
   - Scope: 0xx-style arps, octave jumps, pitch drops/rises, vibrato, duty cycling, wave cycling, rapid noise pitch changes, fake chords, retrigger, stutter, note cut, and stepped volume motion.
   - UI direction: a compact **Motion / Tracker FX** surface with chip-specific destinations and synced rates like 1/8, 1/16, 1/32, and 1/64.

2. **Wave / Sample Editor**
   - User value: very high. This can become one of Chipper's signature workflows for wavetable and sampler chips.
   - Chips: Game Boy wave channel, SNES SPC700-style, Amiga Paula, HuC6280, Namco WSG, Konami SCC.
   - Scope: draw, randomize, smooth, invert, mirror, normalize, bit-depth quantize, copy/paste between lanes, import WAV/BRR/DMC where appropriate, auto-loop, root note, one-shot/loop, and save wave mini-presets.

3. **FM Operator Editor**
   - User value: very high for FM users. Macros are useful starts, but serious FM patches require operator access.
   - Chips: YM2612/OPN2, YM2151/OPM, OPL2/OPL3, YM2413/OPLL custom patch.
   - Scope: algorithm, feedback, operator enable/mute, level, ratio/multiple, detune, attack, decay, sustain/level, release, key scale, velocity response, and clear output-operator highlighting.
   - Non-goal: do not treat the old held-note fade-out as open design work unless it reproduces. Keep held-tail tests green while adding editor depth.

4. **Drum / SFX Workflow**
   - User value: high. Classic chips are as much about kicks, hats, zaps, lasers, coins, jumps, and explosions as leads and basses.
   - Chips: NES DMC/noise, Game Boy noise, SN76489 noise, OPL rhythm, YM2413 rhythm, YM2612 Ch6 DAC, SNES samples, Paula tracker samples, POKEY distortion/noise.
   - UI direction: chip-aware **Drum/SFX Designer** presets or page that emphasizes transient shaping, retrigger, pitch sweeps, noise clocks, sample slots, and mapped drum lanes.

5. **Multi-Output and Per-Channel Routing**
   - User value: high for DAW production. Users will want to process individual channels separately once Chipper exposes lanes clearly.
   - Scope: main stereo out, optional individual chip channel outs, noise/DMC/DAC/drum outs, dry chip out plus processed out, per-channel pan, mute, and solo.
   - Architecture note: design early so source lanes, presets, and host buses do not need disruptive renaming later.

6. **Preset Browser With Musical Tags**
   - User value: high. Fifteen chips need browsing by musical intent, not only chip name.
   - Scope: chip, category, musical role, active lanes, macro destinations, native feature use, Strictness behavior, sample usage, tracker motion usage, mono/poly/drum/SFX flags, favorites, and init patches.
   - Preset files should remain portable flat files and should store recipe metadata as well as parameter state.

7. **Enforceable Authentic / Hybrid / Inspired Behavior**
   - User value: high. Strictness choices should change what controls are allowed and how honestly the UI describes them.
   - Authentic: hardware ranges, stepped values, native channel limits, real-ish register behavior, no impossible polyphony.
   - Hybrid: native core plus musical helpers, voice stacking, modern tuning, and helper envelopes where labeled.
   - Inspired: freer synth behavior, extra modulation, wider stereo, smoothing, and less strict hardware limitation.

8. **MIDI Learn and Automation Polish**
   - User value: high. Hardware control is central to making Chipper feel like an instrument.
   - Scope: MIDI learn, readable host automation names, stable parameter IDs, per-chip visibility metadata, mod wheel defaults, pitch bend range, aftertouch, velocity mappings, sustain pedal behavior, panic/all notes off, mono legato/retrigger controls, and possibly program change.

9. **Chip-Native Feature Checklist**
   - User value: medium-high. Each chip should have an obvious path to its identity features, even if some live in Expert view.
   - Examples: NES sweep/linear counter/DMC/frame behavior; DMG Pulse 1 sweep/wave RAM/NR50/NR51; SID voice 3 tricks and filter routing; OPL rhythm/4-op/waveforms; POKEY AUDCTL pairing; SPC700 ADSR/GAIN/echo/FIR; Paula period tables and hard pan; SCC/Namco/HuC wave RAM management.

10. **Contextual Help**
    - User value: medium-high. Small chip-aware hints make the instrument approachable without turning the UI into a manual.
    - Direction: terse tooltips and optional help text for concepts like DMC, OPL rhythm mode, SID filter routing, POKEY AUDCTL, and SNES BRR/echo.

## Envelope And EG Policy

Envelope UI is a correctness issue, not just a layout issue. Chipper should only show ADSR-style controls when the chip or selected emulation path actually has ADSR-like behavior.

1. **Native ADSR / GAIN**
   - Chips: SID and SNES SPC700/S-DSP.
   - UI: voice-centric ADSR or ADSR/GAIN controls with envelope visuals. These are first-class chip behaviors, not optional macro helpers.

2. **Native Operator EG**
   - Chips: YM2612/OPN2, YM2151/OPM, OPL2/OPL3, and YM2413/OPLL where the editable/custom path exposes operator envelopes.
   - UI: operator envelope controls named as FM EG stages, not generic synth ADSR unless the chip's terms map cleanly. Carrier/modulator roles should be visible.

3. **Native Non-ADSR Envelope / Volume Hardware**
   - Chips: NES/RP2A03, Game Boy/DMG, YM2149/AY, SN76489, and similar PSG/APU chips.
   - UI: stepped envelope, length, attenuation, volume nibble, or hardware-envelope controls. Do not present these as full ADSR.

4. **Chipper Helper Amp Envelopes**
   - Chips: POKEY, Paula, HuC6280, Namco WSG, SCC, and any chip path where Chipper adds musical volume shaping over hardware that lacks native ADSR.
   - UI: label as **Amp Env**, **Volume Gate**, **Tracker Amp Env**, or another chip-specific helper. The copy must say it is a modern musical helper when it is not native hardware.

## Execution Principle

Avoid polishing Chipper into fifteen pretty but separate chip panels. Prioritize features that turn the existing chip surfaces into a complete musical workflow: browse a role, play a patch, edit native chip controls, add tracker motion, shape samples/waves/operators, save/share the result, and automate it from DAW or MIDI hardware.

## Planning Ownership

Keep planning split into three buckets so fixed work does not keep looking unfinished:

- **Fixed regressions with named tests:** FM sustained-note fade-out and NES DMC one-shot looping are closed unless a current build reproduces them. Keep the held-tail renderer subset and `processor_midi_cc_smoke` in the smoke-test habit, then reopen roadmap work only after a failing repro.
- **Implemented-but-shallow surfaces:** prioritize deeper operator editors, clearer sample/wavetable workflows, stronger presets, state recall, and verification evidence.
- **Future research:** keep source/licensing decisions, hardware captures, and unverified emulator comparisons out of the active UI unless they have an audible engine path and truthful verification label.

When docs drift, update the owning document instead of repeating the same note everywhere:

| Document | Owns |
| --- | --- |
| [priority-roadmap.md](priority-roadmap.md) | Ranked execution order and the immediate done checklist |
| [product-gap-roadmap.md](product-gap-roadmap.md) | Broad product gaps and workflow direction |
| [ui-priority-audit.md](ui-priority-audit.md) | Layout non-regression rules and chip-aware control placement |
| [product-spec.md](product-spec.md) | Public instrument contract and visible behavior |
| [emulation-accuracy.md](emulation-accuracy.md) | Verification, accuracy labels, source posture, and license constraints |

## Additional Gaps To Watch

These are not as visible as chip-specific controls, but they strongly affect whether musicians can trust Chipper in real projects.

1. **Project Recall and State Migration**
   - Saved DAW projects must reopen with the same chip, preset, loaded sample paths, per-chip settings, MIDI mappings, and Strictness setting.
   - Parameter IDs, preset schema versions, and sample-bank references need migration rules before public releases.

2. **Missing-Asset and Crash Recovery**
   - Sample-based chips should show clear missing-file states instead of failing silently.
   - External sample paths, deleted folders, moved preset files, invalid BRR/DMC/WAV files, and unsupported formats should be handled without audio-thread allocation, hangs, or crashes.

3. **Performance Budgets**
   - Each chip should have a CPU and memory budget for common use: mono lead, chip poly, sample bank browsing, heavy echo/filter use, and large preset banks.
   - Background sample decoding/import should never block the audio thread.

4. **Accessibility and Readability**
   - Every chip theme should preserve readable contrast, keyboard focus, large enough hit targets, and non-color-only state cues.
   - Dense expert controls should scale by layout size rather than shrinking text into unreadable labels.

5. **Release and Compatibility QA**
   - Test in at least one major DAW per platform before public release.
   - Verify VST3 scanning, plugin ID stability, preset loading, automation recall, MIDI CC handling, and installer behavior on Windows, Linux, and macOS builds.
   - Keep [release-builds.md](release-builds.md) aligned with the actual GitHub workflow so releases stay deliberate and do not run on every push.

6. **Legal Asset Boundaries**
   - Factory presets must not embed unlicensed samples, ROM data, copyrighted wave dumps, or derived game content.
   - User import workflows should make it clear that external samples stay user-owned and outside the repository.

7. **Beginner-to-Expert Path**
   - A new user should be able to select a chip, choose a musical role, tweak a few obvious controls, and save a patch.
   - Expert users should still have a clear route to register-like behavior, operator/wave/sample editing, and strict accuracy constraints.

8. **Planning Hygiene**
   - Docs should separate fixed regressions, active product gaps, and future research. Fixed regressions stay as named tests and verification checklist items, not as repeated feature priorities.
   - Screenshots and user feedback should update this roadmap only when they identify a reproducible problem, a missing chip-native control, or a workflow gap that blocks making music.
   - Planning docs have distinct jobs: the priority roadmap decides next execution order, this file tracks broad product gaps, the UI audit records layout non-regression rules, the product spec states the public instrument contract, and the accuracy/source docs constrain claims and licensing.
   - When a chip surface graduates from placeholder to playable, remove stale "planned-only" language from the relevant docs in the same slice so screenshots, tests, and docs tell the same story.
