# Chipper Build And Release Notes

Chipper uses two layers of automation. `.github/workflows/ci.yml` runs for pull requests, pushes to `main`, and manual dispatches so a single change does not create duplicate branch-push and pull-request runs; `.github/workflows/release.yml` remains the deliberate packaging and publishing path.

## Local Development Build

From the repository root:

```powershell
cmake -S . -B build-codex -G "Visual Studio 17 2022" -A x64
cmake --build build-codex --config Release --target Chipper_VST3 chipper_render
ctest --test-dir build-codex -C Release --output-on-failure
```

For a focused smoke pass while iterating:

```powershell
ctest --test-dir build-codex -C Release -R "chipper_descriptor_smoke|processor_midi_cc_smoke|chipper_editor_size_smoke|chipper_render_opl3_four_op_editor|chipper_ui_motion_snapshot_smoke|chipper_ui_opl3_four_op_snapshot_smoke|held_tail|preset_.*held" --output-on-failure
```

This focused pass is the current high-signal regression gate for:

- chip descriptor/UI metadata
- MIDI CC and state-recall smoke behavior
- OPL3 four-operator algorithm/role mapping, exact AM/VIB/EGT/KSR/MULT and KSL/TL composite bytes, grouped CC12-19 held-note updates, schema-8 compatibility, OPM/OPL chip-switch isolation, and the shared OP1-OP4 editor at both supported widths
- YM2151 per-operator DT1/DT2 state, CC12-19, exact `$40/$C0` composite registers, held-note base-pitch neutrality, and two-width operator-card geometry
- OPNA fixed ADPCM-A region geometry/rates/reset and OPNB sparse 256-byte-page packing, decoded previews, schema-8 region validation, deleted-source fallback, legacy packed-bank compatibility, and both supported editor widths
- DAW-friendly editor default and restored-window height
- Motion Lab schema-8 recall, sample-accurate/no-allocation playback, two-width layout/accessibility, and focused snapshot behavior
- NES DMC one-shot versus loop behavior
- FM held-tail behavior and held factory presets

Latest local development checkpoint: the full RelWithDebInfo build, 863/863 CTest cases, 401/401 factory-preset audibility renders, the binary VST3 save/reopen gate, the dense-MIDI and Motion Lab no-allocation/deadline gates, the independent Ayumi YM2149 reference gate, and editor/browser/Motion Lab/OPL3 four-operator capture gates passed on Windows on 2026-07-27. The separately verified full Release build and pluginval 1.0.4 strictness-level-5 checkpoint remain dated 2026-07-26.

Generated sample prerequisites such as NES `.dmc` and Paula `.8svx` fixtures should use CTest fixtures, not only `DEPENDS`, so filtered sample-focused runs still prepare their binary inputs.

If those fixed-regression gates pass, keep the release checklist unchanged and continue with forward product work. Reopen a fixed regression only after it reproduces in the current build and the relevant gate fails.

For preset work, run the catalog and audibility checks before calling the bank ready:

```powershell
.\scripts\verify-presets.ps1 -BuildRoot build-codex
```

These commands verify that factory presets are chip-local, metadata-complete, category-safe, and actually render audible output. They do not prove that a preset is musically excellent, so new presets still need a quick hand-audition pass and clean provenance.

The preset QA helper auto-detects `chipper_render` in common Windows and Unix CMake layouts, including `Release/chipper_render.exe`, `Release/chipper_render`, and single-config `chipper_render` builds. Pass `-Renderer` only when using a custom output path.

Install the exact local build you just made:

```powershell
.\install-vst3.ps1 -Scope User -BuildRoot build-codex
.\install-vst3.ps1 -VerifyOnly -BuildRoot build-codex
```

The build hash in Chipper's footer should match the installed marker reported by the installer. If a host still opens an older build, close the host and check both user and global VST3 folders:

```powershell
.\install-vst3.ps1 -VerifyOnly -BuildRoot build-codex
```

If both locations contain Chipper, sync them from an elevated PowerShell:

```powershell
.\install-vst3.ps1 -Scope Both -BuildRoot build-codex
```

Docs-only planning, README, or release-note updates do not need a VST3 build/install cycle. For those slices, run:

```powershell
git diff --check
```

If the docs uncovered a mismatch that also required source, preset, installer, or workflow changes, use the normal build, targeted test, install, and verify flow above.

For docs-only planning cleanup, prefer removing duplicate command snippets from roadmap files and pointing back here. That keeps release gates from drifting as the test suite evolves.

## GitHub Release Build

The CI workflow builds and tests Windows, Linux, and macOS, runs preset QA on all three, validates the Windows VST3 with pluginval 1.0.4 at strictness level 5, and runs the engine/renderer suite under Linux AddressSanitizer and UndefinedBehaviorSanitizer. The pluginval archive is pinned by SHA-256 before extraction. Release jobs repeat the full cross-platform build/test/preset gates and checksum-verified pluginval validation before packaging.

The workflow is `.github/workflows/release.yml`.

It runs only for:

- Manual `workflow_dispatch` from the GitHub Actions tab. Manual runs can set an optional `release_tag` for artifact names and can set `attach_to_release` to upload assets to a draft/prerelease GitHub Release for that tag.
- Version tag pushes that match `v*`, such as `v0.2.0`.
- Published GitHub Releases.

The release workflow does not run on normal pushes or pull requests; the separate CI workflow does.

The workflow currently builds and tests:

- Windows VST3
- Linux VST3
- macOS VST3

Each platform artifact is packaged as a zip containing:

- `Chipper.vst3`
- `README.md`
- `THIRD_PARTY_NOTICES.md`
- `LICENSE`
- `CHANGELOG.md`

Each platform zip is accompanied by a `.sha256` file in the standard `hash  filename` format for `sha256sum -c` style verification. README media assets under `screenshots/`, `examples/`, and generated upload helpers under `examples/video/` are intentionally excluded from release zips so platform downloads stay focused on the plugin.

For a candidate build:

1. Run `Release VST3` manually from the Actions tab.
2. Leave `attach_to_release` off for private workflow artifacts, or provide `release_tag` and enable `attach_to_release` to stage assets on a draft/prerelease GitHub Release.
3. Download each platform zip plus `.sha256` file, verify the checksum, and smoke-test at least the Windows VST3 in a host.

For a public release draft:

1. Make sure `main` is green locally.
2. Update `CHANGELOG.md` and bump the CMake project version if this is a new release version.
3. Create and push a version tag, for example `v0.2.0`.
4. Let the `Release VST3` workflow build, test, package, create or update a draft/prerelease GitHub Release, and attach the zips plus checksums.
5. Download each artifact, verify checksums, and smoke-test it in a host before publishing the release.

Publishing a GitHub Release manually for an existing tag also runs the workflow and uploads the platform zips plus checksums to that release. This is useful if you want to write release notes first. A plain `v*` tag push creates a draft/prerelease build by default so maintainers can inspect artifacts before making the release public.

## Release Gate

Before a release is considered usable, keep these checks green:

- OPL3 four-operator exact-register, algorithm-role, editor-layout, and focused snapshot tests, because the linked `$C0` bits and OP1-OP4 physical routing must remain synchronized.
- Descriptor and MIDI CC smoke tests.
- FM held-tail tests, because FM notes fading to silence is a fixed regression.
- NES DMC loop-off tests, because one-shot DMC playback should stop stepping and hold the DAC value instead of looping.
- Editor-size smoke tests, because Chipper should open and restore inside the documented DAW-friendly per-chip fixed height. Most chips are capped at 860 px; SID is currently capped at 880 px for readable ADSR.
- Motion Lab editor/snapshot and processor-performance gates, because its per-chip non-parameter state, active-step UI, Big Mono timing, and intentional Chip Poly bypass must remain deterministic.
- Factory preset catalog and audibility checks whenever preset content changed.
- The parameter-ID, state-schema, portable-asset, and mono/stereo bus invariants in [compatibility-contract.md](compatibility-contract.md).
- Independent reference comparisons for selected high-risk cores, with provenance and per-capture thresholds recorded as described in `tests/references/README.md`.

  ```powershell
  ctest --test-dir build-codex -C Release --output-on-failure -R ym2149_ayumi_reference
  ```

  The first checked-in gate renders a direct YM2149 tone-A register trace and
  compares it against an independently generated Ayumi fixture. It verifies the
  fixture checksum/format/provenance metadata, the renderer's final register
  state, per-channel waveform correlation and normalized error, raw RMS ratio,
  bounded startup lag, channel layout, and exact duration. Its thresholds catch
  gross tone, duty, polarity, routing, gain, and duration regressions; they do not
  claim cycle accuracy, exact analog output, complete variant behavior, or
  hardware validation.
- The binary-level VST3 scan/load/project-save/reopen gate:

  ```powershell
  ctest --test-dir build-codex -C Release --output-on-failure -R chipper_vst3_host_state_smoke
  ```

  It discovers the built VST3 class through JUCE's VST3 host, instantiates the
  bundle twice, automates parameters, saves an external DMC asset with only the
  DMC source audible, deletes the source file, restores from the bounded embedded
  project copy, verifies that the fallback is re-saved, and compares the reopened
  render with the saved instance. This is the
  reproducible host-boundary gate; at least one manual Windows DAW scan/load/
  project-save/reopen check remains required for release UX. Linux and macOS
  manual host checks are required before those platforms are advertised as host-tested.
- The repeatable dense-MIDI processor capture:

  ```powershell
  ctest --test-dir build-codex -C Release --output-on-failure -R chipper_processor_performance_smoke
  ```

  It warms every chip mode, then measures steady dense-MIDI blocks and a 1,024-block
  mode-switch stress pass at 48 kHz / 256 samples. The gate fails on any
  `processBlock` allocation, callback deadline miss, non-finite output, or
  out-of-range output, and prints mean, p99, maximum callback time, deadline,
  real-time load, and peak for the release record.

Passing fixed-regression gates should not create new roadmap work. If one fails, reopen the owning bug as a P0, fix it with a tighter test, then return it to this release gate instead of leaving stale todo language in the planning docs.

For planning cleanup, treat this release gate as the source of truth for fixed high-risk regressions. The roadmap should point here or name the same focused CTest command; it should not keep separate open-ended todos for issues that already pass the gate.

## Legal Packaging

Do not package copyrighted samples, ROM data, ripped instruments, tracker modules, or game-derived wave tables. Factory presets should be original Chipper sound design unless every imported asset has compatible licensing and documented provenance.

Every release zip should include `THIRD_PARTY_NOTICES.md` so emulator-core credits and license notes travel with the plugin.
