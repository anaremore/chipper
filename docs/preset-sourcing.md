# Preset Sourcing And QA

Chipper needs many delightful factory presets, but those presets must be safe to ship, easy to edit, and honest about how the sound was made. This document owns the preset sourcing rules so factory preset growth does not drift into copied game data, unlicensed samples, or vague provenance.

## Allowed Factory Preset Sources

Prefer these paths, in order:

1. **Original Chipper sound design**
   - Hand-authored patches built from public chip documentation, known synthesis techniques, and Chipper's own controls.
   - This is the default path for leads, basses, arps, UI sounds, drums, and SFX.

2. **Clean-room recreations of broad chip techniques**
   - Examples: NES triangle bass, SID PWM lead, AY fake chord, OPL bell, POKEY poly bass.
   - Do not copy a song, game instrument, tracker module, commercial patch, ROM table, or sample asset. Recreate the general technique using Chipper parameters.

3. **Explicitly permissive third-party material**
   - Only use when the license is compatible with the project and the exact source, revision, license, and attribution are documented.
   - Public-domain, CC0, MIT, BSD, Apache-2.0, or similarly permissive material is easiest to handle.
   - GPL/LGPL preset banks, samples, ROM data, or tracker instruments are not bundled unless the project makes a deliberate license/distribution decision first.

4. **User-owned external samples**
   - NES DMC, SPC700, and Paula presets may reference user sample paths, but factory presets must not embed or commit unlicensed audio.
   - Preset files should keep external sample references portable where possible and clearly show missing-asset states.

## Disallowed Factory Preset Sources

Do not bundle or derive factory presets from:

- Commercial game patches, songs, sound effects, samples, soundfonts, ROMs, or extracted instrument banks.
- Tracker modules or instrument files without explicit compatible licensing.
- Commercial VST presets, sample packs, or plugin factory sounds.
- Hardware captures that include copyrighted music or game assets.
- Emulator lookup tables, waveform dumps, or patch data unless the license and provenance are documented in the source-map and notices.

It is fine to study the musical behavior of classic sounds. It is not fine to copy the data that produced them.

## Preset Quality Bar

A factory preset is not ready just because it has a good name. It should pass this checklist:

- **Audible:** the preset renders above the minimum audibility threshold and below clipping.
- **Playable:** it behaves sensibly for held notes, short notes, and the selected play mode.
- **Chip-local:** the preset belongs to the selected chip and uses that chip's native vocabulary.
- **Visible:** selecting it updates the relevant controls instead of hiding decisions in an opaque recipe.
- **Editable:** the user can tweak the obvious musical parameters after loading it.
- **Shareable:** it can be saved as a `.chipperpreset` without embedding unlicensed assets.
- **Truthful:** helper envelopes, modern stereo, imported samples, and partial emulation are labeled honestly.
- **Stable:** it survives chip switching, project recall, MIDI CC changes, and the renderer/debug JSON path.

## Furnace-Informed Coverage Matrix

Furnace is the benchmark for breadth, chip vocabulary, and tracker-style workflow coverage. Chipper may use it to define coverage targets, but not as a source of copied patch data, wave tables, modules, or samples.

`presetQualityTargets()` owns the shippable matrix. Each implemented chip must cover at least ten chip-local factory presets, the core musical roles `Bass`, `Lead`, `Arp`, `Keys / Pad`, `Drums`, and `SFX`, plus chip-family reference tags such as `fm`, `sampler`, `wavetable`, `noise`, `tracker`, or `dmc`. `chipper_render --list-presets` exports this matrix under `summary.qualityTargets`, and `assert_preset_catalog_json.py` fails when the minimum preset count, any required role, or any reference tag is missing.

## Required QA Loop

Before adding or releasing factory presets:

```powershell
cmake --build build --config Release --target Chipper_VST3 chipper_render
.\scripts\verify-presets.ps1
```

`verify-presets.ps1` defaults to the local `build` folder, falls back to `build-codex` when that is the available build tree, validates the catalog metadata, prints the preset count for every implemented chip, and renders every factory preset into `preset-qa` artifacts. Use `-BuildRoot`, `-Config`, or `-Renderer` only when testing a non-standard build.

For each new preset bank, also audition representative presets in the plugin:

- Load the preset from the browser.
- Confirm the visible controls explain the sound.
- Play low, middle, and high notes.
- Test the intended play mode.
- For FM presets, hold a note and confirm the envelope does not fade to silence unless designed.
- For DMC/SPC700/Paula presets, test one-shot, loop, note-map/manual-slot behavior, and missing-file states.

## Metadata To Track

When a preset is based on anything other than original Chipper sound design, record:

- Source URL or local reference path.
- Upstream author/project.
- License name and license file location.
- Commit, version, or retrieval date.
- What was used: concept, parameter values, sample, table, or only a listening reference.
- Whether attribution is required in `THIRD_PARTY_NOTICES.md`.

If that metadata is unclear, do not bundle the preset.

## Naming Rules

Preset names should describe musical use without implying copied game content:

- Good: `NES Triangle Bass`, `SID PWM Lead`, `OPL Bell Stack`, `POKEY Poly Engine`, `SPC700 Soft Bass`.
- Avoid: direct game/song/character names, soundtrack titles, commercial plugin preset names, or anything that suggests an unlicensed recreation.

Use categories for discovery, not legal camouflage. A preset named broadly can still be a problem if it copies protected data.
