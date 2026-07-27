# Chipper compatibility contract

This document is the pre-1.0 compatibility boundary for host automation, saved
state, external assets, and audio buses. A release candidate must keep the
checks named below green. New functionality may extend this contract, but it
must not silently reinterpret an existing ID or accepted state.

## Host parameters

- The 96 IDs in `tests/parameter-id-contract.txt` are stable host automation
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

## Saved state

- The APVTS root tag is `ChipperState`.
- Unversioned state is schema 1. It is accepted and migrated to the current
  schema before APVTS restore.
- Current saved state declares `stateSchemaVersion="3"`. Schema 3 adds
  optional embedded custom Wave RAM; unversioned, schema-1, and schema-2
  states migrate with generated wave templates and no custom lanes.
- Invalid versions and versions newer than the plugin supports fail explicitly;
  they are not partially interpreted.
- A state payload may restore at most 4,096 low-level register writes and at
  most 256 sample references per bank and 19 native custom Wave RAM lanes.
  Oversized register or Wave RAM payloads fail; oversized sample banks are
  bounded. Wave RAM lanes contain exactly 32 native-range samples.
- Schema fixtures live in `tests/state/` and are exercised by
  `chipper_processor_midi_cc_smoke`.

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
