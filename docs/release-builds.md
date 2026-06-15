# Chipper Build And Release Notes

Chipper should not spend GitHub Actions minutes on every push. Normal pushes are quiet. Release builds happen only when a maintainer deliberately starts them.

## Local Development Build

From the repository root:

```powershell
cmake -S . -B build-codex -G "Visual Studio 17 2022" -A x64
cmake --build build-codex --config Release --target Chipper_VST3 chipper_render
ctest --test-dir build-codex -C Release --output-on-failure
```

For a focused smoke pass while iterating:

```powershell
ctest --test-dir build-codex -C Release -R "chipper_descriptor_smoke|processor_midi_cc_smoke|held_tail|preset_.*held" --output-on-failure
```

This focused pass is the current high-signal regression gate for:

- chip descriptor/UI metadata
- MIDI CC and state-recall smoke behavior
- NES DMC one-shot versus loop behavior
- FM held-tail behavior and held factory presets

Latest focused gate check: 26/26 passing on 2026-06-14 for `chipper_descriptor_smoke|processor_midi_cc_smoke|held_tail|preset_.*held`.

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

The workflow is `.github/workflows/release.yml`.

It runs only for:

- Manual `workflow_dispatch` from the GitHub Actions tab.
- Published GitHub Releases.

It does not run on normal pushes or pull requests.

The workflow currently builds and tests:

- Windows VST3
- Linux VST3
- macOS VST3

Each platform artifact is packaged as a zip containing:

- `Chipper.vst3`
- `README.md`
- `THIRD_PARTY_NOTICES.md`
- `LICENSE`

For a public release:

1. Make sure `main` is green locally.
2. Create and push a version tag, for example `v0.2.0`.
3. Publish a GitHub Release for that tag.
4. Let the `Release VST3` workflow build, test, package, and attach the zips.
5. Download each artifact and smoke-test it in a host before calling the release production-ready.

For a private candidate build, run `Release VST3` manually from the Actions tab instead of publishing a release.

## Release Gate

Before a release is considered usable, keep these checks green:

- Descriptor and MIDI CC smoke tests.
- FM held-tail tests, because FM notes fading to silence is a fixed regression.
- NES DMC loop-off tests, because one-shot DMC playback should stop stepping and hold the DAC value instead of looping.
- Factory preset catalog and audibility checks whenever preset content changed.
- At least one manual DAW scan/load check for Windows. Linux and macOS host checks should be added before public cross-platform releases are advertised as tested.

Passing fixed-regression gates should not create new roadmap work. If one fails, reopen the owning bug as a P0, fix it with a tighter test, then return it to this release gate instead of leaving stale todo language in the planning docs.

For planning cleanup, treat this release gate as the source of truth for fixed high-risk regressions. The roadmap should point here or name the same focused CTest command; it should not keep separate open-ended todos for issues that already pass the gate.

## Legal Packaging

Do not package copyrighted samples, ROM data, ripped instruments, tracker modules, or game-derived wave tables. Factory presets should be original Chipper sound design unless every imported asset has compatible licensing and documented provenance.

Every release zip should include `THIRD_PARTY_NOTICES.md` so emulator-core credits and license notes travel with the plugin.
