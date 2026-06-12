# Chipper Product Gap Roadmap

This roadmap captures the broad product gaps that remain after the first playable-chip and UI-polish passes. The guiding question is:

> Can a musician make a complete chiptune bass, lead, arp, drum kit, and SFX pack in Chipper without fighting the UI?

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
   - Scope: chip, category, musical role, active lanes, macro destinations, native feature use, accuracy mode, sample usage, tracker motion usage, mono/poly/drum/SFX flags, favorites, and init patches.
   - Preset files should remain portable flat files and should store recipe metadata as well as parameter state.

7. **Enforceable Authentic / Hybrid / Inspired Behavior**
   - User value: high. Behavior Strictness choices should change what controls are allowed and how honestly the UI describes them.
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

## Execution Principle

Avoid polishing Chipper into fifteen pretty but separate chip panels. Prioritize features that turn the existing chip surfaces into a complete musical workflow: browse a role, play a patch, edit native chip controls, add tracker motion, shape samples/waves/operators, save/share the result, and automate it from DAW or MIDI hardware.

## Additional Gaps To Watch

These are not as visible as chip-specific controls, but they strongly affect whether musicians can trust Chipper in real projects.

1. **Project Recall and State Migration**
   - Saved DAW projects must reopen with the same chip, preset, loaded sample paths, per-chip settings, MIDI mappings, and accuracy mode.
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

6. **Legal Asset Boundaries**
   - Factory presets must not embed unlicensed samples, ROM data, copyrighted wave dumps, or derived game content.
   - User import workflows should make it clear that external samples stay user-owned and outside the repository.

7. **Beginner-to-Expert Path**
   - A new user should be able to select a chip, choose a musical role, tweak a few obvious controls, and save a patch.
   - Expert users should still have a clear route to register-like behavior, operator/wave/sample editing, and strict accuracy constraints.
