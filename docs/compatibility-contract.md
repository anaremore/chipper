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
- Current saved state declares `stateSchemaVersion="10"`. Schema 3 added
  optional embedded custom Wave RAM; schema 4 added optional per-chip Motion
  Lab patterns; schema 5 added native YM2151 LFO controls; schema 6 added
  native YM2151 per-operator DT1/DT2 choices; and schema 7 gives those stable
  slots native OPL3 AM/VIB/KSR and KSL semantics. Schema 8 adds optional
  indexed `CHIPPER_ADPCM_A_REGION` children beneath the existing OPNA/OPNB
  sample-state parents while preserving legacy packed parent records. Schema 9
  adds OPN2 DAC `sourceRateHz`, `rootNote`, `trimStart`, `trimEnd`, and
  `tailBehavior` metadata. Schema-8 OPN2 state migrates to source rate 0
  (native legacy cadence), root note 60, full trim, and centered tail so old
  projects keep their resolved sound.
  Unversioned, schema-1, and schema-2 states migrate with generated wave
  templates and no custom lanes; schema-3 and older states migrate with one
  disabled neutral motion pattern per chip; schema-4/5 states deterministically
  backfill the newer OPM controls to Preset; and schema-6 migration preserves
  genuine YM2151 DT1/DT2 values while resetting latent values in non-YM2151
  state before OPL3 can reinterpret them.
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
- A schema-8 Yamaha region parent declares `bankMode="regions"` and exactly six
  logical positions. Restore rejects duplicate/out-of-range indices, OPNA
  payloads that do not exactly fill their fixed hardware window, OPNB payloads
  that do not use complete 256-byte pages, and OPNB banks exceeding 1 MiB.
- Schema fixtures live in `tests/state/` and are exercised by
  `chipper_processor_midi_cc_smoke`, including legacy migration, current
  schema-v10 round trips, schema-8 OPN2 migration, malformed ADPCM-A region rejection, deleted-source
  fallback, missing OPM choice backfill, malformed motion, and future versions.

## External assets

Schema 10 adds optional `yn0` through `yn7` to YM2149 Motion patterns: zero follows the preset; 1-32 encode noise periods 0-31. Older states default to zero. Native values on other chip modes and out-of-range values fail before mutation. Transient Motion periods are excluded from base register snapshots.

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
- Shareable `.chipperpreset` files remain reference-only, and preset restore
  rejects injected embedded payloads before mutating processor state.
- DAW host project state may include a bounded fallback copy of user-owned NES
  DMC, SPC700 BRR/imported PCM, Paula PCM/MOD-instrument, OPN2 DAC, OPNA
  ADPCM-A/B, and OPNB ADPCM-A/B bytes. OPNA/OPNB editable ADPCM-A banks store
  fallback payloads per indexed region rather than flattening their ownership.
  The original path remains authoritative:
  Chipper uses the embedded copy only when the source cannot be read and shows
  `Using embedded project copy ...; relink source` in the asset status.
- Bank serialization includes at most the first 32 playable slots and 256 KiB
  per slot. Single-asset caps are 256 KiB for OPN2 DAC, 64 KiB for OPNA rhythm,
  1 MiB for OPNA ADPCM-B, 2 MiB for OPNB ADPCM-A, and 16 MiB for OPNB ADPCM-B.
  Within schema-8 region mode, each OPNA payload must equal its fixed
  128-5952-byte window; each OPNB payload is at most 1 MiB and the six-region
  aggregate must also fit the 1 MiB YM2610 window.
  A project may contain at most 101 embedded payloads and 16 MiB of decoded
  sample bytes inside a 32 MiB processor-state envelope. Assets beyond a cap
  stay reference-only.
- Embedded payloads carry format version, encoding, name, loop points, source
  instrument index/byte count, Base64 data, and an FNV-1a checksum. OPN2 DAC
  parents additionally retain rate, root, trim, and center/hold tail metadata
  whether their bytes come from the live path, embedded fallback, or a missing
  reference tombstone. Structurally
  oversized or misplaced payloads fail before state mutation. A missing or
  corrupt individual bank slot becomes a named silent tombstone so later note
  mappings do not shift; an ADPCM-A region likewise retains its logical index
  while later OPNB regions repack around the hole. The UI keeps the relink
  warning visible.
- Embedded project fallback does not grant redistribution rights. Factory and
  shared preset content still requires provenance and license review.

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
ctest --test-dir build-codex -C Release -R "chipper_vst3_host_state_smoke" --output-on-failure
```

For a release candidate, also run the full CTest suite, preset QA, pluginval,
host scan/load/project-recall checks, and the independent-reference workflow in
`tests/references/README.md`.
