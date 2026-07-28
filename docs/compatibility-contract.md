# Chipper compatibility contract

This document is the pre-1.0 compatibility boundary for host automation, saved
state, external assets, and audio buses. A release candidate must keep the
checks named below green. New functionality may extend this contract, but it
must not silently reinterpret an existing ID or accepted state.

## Host parameters

- The 107 IDs in `tests/parameter-id-contract.txt` are stable host automation
  identifiers. Display names, grouping, and chip-specific visibility may
  improve without changing those IDs.
- Do not remove, rename, or reuse a listed ID for a different meaning. If a
  control becomes obsolete, keep a compatible hidden/deprecated parameter until
  an explicit migration and host-automation policy is implemented.
- New parameters require a new unique ID, a deterministic default, state-recall
  coverage, and an update to the checked-in contract fixture.
- `accuracy` remains a recalled compatibility field. Its visible choices are
  reserved/current and do not claim distinct synthesis paths.
- `chipper_processor_midi_cc_smoke` compares the live APVTS parameter set with
  the checked-in fixture and fails on an accidental addition, removal, or
  rename.
- The shared OPL3 OP1-OP4 matrix reuses the existing stable
  `fmOperator*Level`, `fmOperator*Multiplier`, and `fmOperator*Attack/Decay/
  Sustain/ReleaseRate` IDs. Neutral Level plus Follow choices preserve the
  previous resolved OPL patch. No parameter ID or state-schema revision was
  added. In 4-op topology only, the existing `macroControl1` OPL connection
  value now selects algorithms 0-3; two-operator serial/parallel behavior is
  unchanged.

## Saved state

- The APVTS root tag is `ChipperState`.
- Unversioned state is schema 1. It is accepted and migrated to the current
  schema before APVTS and non-parameter state restore.
- Current saved state declares `stateSchemaVersion="6"`. Schema 3 added
  optional embedded custom Wave RAM; schema 4 added optional per-chip Motion
  Lab patterns; schema 5 added native YM2151 LFO controls; and schema 6 added
  native YM2151 per-operator DT1/DT2 choices. Unversioned, schema-1, and
  schema-2 states migrate with generated wave templates and no custom lanes;
  schema-3 and older states migrate with one disabled neutral motion pattern
  per chip; and schema-4/5 states deterministically backfill the newer OPM
  controls to Preset so old projects preserve their resolved sound.
- Invalid versions and versions newer than the plugin supports fail explicitly;
  they are not partially interpreted.
- A state payload may restore at most 4,096 low-level register writes and at
  most 256 sample references per bank, 19 native custom Wave RAM lanes,
  and one Motion Lab pattern for each of the 27 canonical chip modes.
  Oversized register, Wave RAM, or motion payloads fail; oversized sample
  banks are bounded. Wave RAM lanes contain exactly 32 native-range samples.
  Each motion pattern contains exactly eight bounded steps, one legal synced
  rate, a length from 1-8, pitch from -24 to +24 semitones, level from 0-15,
  and Hold/Trig/Cut gate values; duplicate or unknown chip IDs fail explicitly.
- Schema fixtures live in `tests/state/` and are exercised by
  `chipper_processor_midi_cc_smoke`, including legacy migration, current schema-v6
  round trips, missing OPM LFO/detune choice backfill, malformed motion rejection, and future-version rejection.

## External assets

- Saved asset tags retain the original `path` and are annotated with `fileName`
  plus a portable `relativePath` when the file is inside the preset directory
  or its `Samples`/`samples` folder.
- Restore tries a safe in-preset relative path first, then the original path,
  then same-name files beside the preset and in `Samples`/`samples`.
- Absolute values in `relativePath` and traversal outside the preset directory
  are rejected. Missing files produce a visible restore warning rather than
  silently substituting unrelated data.
- Presets do not embed copyrighted samples, ROMs, tracker modules, or ripped
  wave tables. Redistribution still requires provenance and license review.

## Audio buses

- Chipper exposes one main output bus named `Output`.
- Hosts may negotiate that bus as mono or stereo. Surround layouts and
  additional output buses are not part of the current contract.
- A future multi-output design must add buses without renaming the main bus,
  define downmix and project-recall behavior, and land host tests before it is
  advertised.

## Release evidence

Before changing this contract, run at minimum:

```powershell
cmake --build build-codex --config Release --target Chipper_VST3 chipper_processor_midi_cc_smoke
ctest --test-dir build-codex -C Release -R "chipper_processor_midi_cc_smoke|chipper_parameter_midi_cc_smoke|chipper_editor_size_smoke" --output-on-failure
```

For a release candidate, also run the full CTest suite, preset QA, pluginval,
host scan/load/project-recall checks, and the independent-reference workflow in
`tests/references/README.md`.
