```text
   ▄████▄   ██  ██  ██  ██▓███   ██▓███  ▓█████  ██▀███
   ██▀ ▀█  ▓██  ██ ▓██ ▓██   ██ ▓██   ██ ▓█   ▀ ▓██   ██
   ▓█    ▄  ██▀▀██  ██  ██  ██   ██  ██   ███    ██  ▄█
   ▓▓▄ ▄██  ▓█  ██  ██  ██▄█     ██▄█     ▓█  ▄  ██▀▀█▄
    ▓███▀   ▓█  ██  ██  ██       ██        ████  ██   ██

     .oO  c h i p t u n e   s y n t h   c o r e  Oo.
```

# Chipper

Chipper is a multi-chip retro synth VST3 focused on classic game and computer sound chips. The goal is to make chip limitations feel musical: fast presets and readable controls on the surface, with register-aware or emulator-backed behavior underneath.

The project is intentionally honest about accuracy. A mode is only labeled accurate when the implementation and tests justify it. Most modes are currently **verified partial** or **clean-room partial** while renderer tests, emulator comparisons, and hardware validation are built out.

## Highlights

- Chip-aware VST3 instrument built with JUCE.
- User-facing controls map to chip-native ideas such as pulse duty, PSG noise modes, SID filter routing, FM algorithms, BRR/DMC sample playback, and wavetable source lanes.
- Factory presets are chip-local and update the visible controls instead of hiding decisions inside opaque macros.
- Every plugin parameter has a fixed MIDI CC mapping for hardware control and Ableton Configure workflows.
- A separate `chipper_render` command-line tool renders WAV plus debug JSON for regression tests and emulator validation.
- Third-party emulator cores are vendored only after a license/provenance audit.

## Current Development Focus

Chipper is being developed as an accurate, truthful chip instrument rather than a decorative retro synth. The current product rules are:

- Presets are the primary musical entry point. They should load audible chip-local sounds, update the visible controls, and remain editable/shareable as normal Chipper state.
- The header's **Strictness** control requests Inspired, Hybrid, or Authentic behavior. It is not an accuracy certificate; the footer and docs carry the actual verification claim. Hosts and MIDI maps may show the same stable parameter as `Behavior Strictness`.
- Chip-owned controls belong where the hardware owns them: duty under pulse channels, noise mode under noise channels, wave/sample choices and level under each wavetable or sampler lane, filters/echo/sample banks in shared modules.
- Chips without native ADSR use clearly labeled Chipper amp/gate helpers instead of pretending the hardware has full ADSR.
- Fixed regressions stay in tests and release gates. FM held-note fade-out and NES DMC loop-off behavior are currently treated as fixed; if they return, they are regressions, not open design questions.
- UI readability wins over compactness. It is acceptable for chip-specific layouts to grow or use fixed aspect ratios when that preserves standard-height controls, visible level lanes, and waveform previews.

## Planning Map

The planning docs intentionally separate current execution from broader research so fixed bugs do not keep looking like active product gaps:

- [docs/priority-roadmap.md](docs/priority-roadmap.md) is the ranked execution list for the next high-value slices.
- [docs/product-gap-roadmap.md](docs/product-gap-roadmap.md) tracks larger instrument workflows such as tracker motion, wave/sample editing, FM operator editing, preset sharing, and release readiness.
- [docs/ui-priority-audit.md](docs/ui-priority-audit.md) is the UI non-regression checklist for chip-aware layouts, standard control sizes, and channel-local controls.
- [docs/emulation-accuracy.md](docs/emulation-accuracy.md) is the truthful source/verification/license status for each chip.
- [docs/release-builds.md](docs/release-builds.md) owns local install, GitHub release, and release-gate procedure.

Fixed issues such as FM sustained-note fade-out and NES DMC one-shot looping should remain in regression tests and release gates, not in the active feature queue unless they reproduce in the current build. When a fixed issue is suspected, run the named gate first; if it passes, keep planning focused on new user value instead of adding another stale todo. As of the current planning cleanup, those gates are treated as green guardrails and the next work should favor chip-aware editors, preset quality, state recall, and verification evidence.

## Current Chip Modes

Implemented depth varies by chip. See [docs/emulation-accuracy.md](docs/emulation-accuracy.md) for the source, verification status, known gaps, and license posture of each mode.

| Mode | Current status |
| --- | --- |
| NES / RP2A03 | Partial clean-room APU with pulse, triangle, noise, DMC file/folder banks, nonlinear mixer, and frame-counter tests |
| Game Boy / DMG | Partial clean-room APU with pulse, wave RAM helpers, noise, envelope/sweep/stereo routing tests |
| SID / C64 | Partial clean-room SID-style voice model with 3 voices, per-voice waves/PW/ADSR, sync/ring bits, filter modes, routing, and model color |
| YM2149 / AY | Verified-partial MIT `emu2149` PSG adapter |
| SN76489 / Sega PSG | Verified-partial MIT `emu76489` PSG adapter |
| YM2612 / Genesis FM | Verified-partial BSD-3-Clause `ymfm` OPN2 adapter with partial channel-6 DAC drum playback |
| OPL2 / OPL3 | Verified-partial BSD-3-Clause `ymfm` YMF262/OPL3 adapter with a nine-lane OPL2-compatible surface; full 18-channel/four-op editing planned |
| SNES SPC700-style | Partial clean-room eight-voice BRR/WAV/AIFF sample-player model with note maps, loop handling, Gaussian-style interpolation, PMON-style pitch modulation, noise, and musical echo helper |
| Atari POKEY | Partial clean-room polynomial/timer model |
| Amiga Paula | Partial clean-room tracker-sampler model with file/folder sample banks |
| PC Engine HuC6280 | Partial clean-room wavetable/noise model with six lanes and partial Ch 1/2 FM-LFO pairing |
| Namco arcade WSG | Partial clean-room wavetable model |
| YM2151 / OPM | Verified-partial BSD-3-Clause `ymfm` OPM adapter with eight lanes, pan, and first channel-8 noise control |
| YM2413 / OPLL | Verified-partial MIT `emu2413` preset-FM adapter |
| Konami SCC | Verified-partial MIT `emu2212` wavetable adapter |

## Build

Requirements:

- CMake 3.22 or newer
- Visual Studio 2019/2022 with the Desktop C++ workload
- Internet access on first configure so CMake can fetch JUCE

Configure and build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Chipper_VST3 chipper_render
```

The VST3 bundle is produced at:

```text
build\Chipper_artefacts\Release\VST3\Chipper.vst3
```

Run the test suite:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

For fast regression checks while developing, run the focused renderer/processor smoke tests first:

```powershell
ctest --test-dir build-codex -C Release -R "chipper_descriptor_smoke|processor_midi_cc_smoke|held_tail|preset_.*held" --output-on-failure
```

These cover high-risk playable-instrument paths such as NES DMC loop-off behavior and FM sustained-note/key-on regressions before a full suite run. For the FM sustain regression specifically, the CTest names include `held_tail` and assert renderer `tailRms` rather than only checking that rendering completed.

## GitHub Release Builds

GitHub Actions are intentionally quiet on normal pushes. The release workflow only runs when you publish a GitHub Release or start it manually from the Actions tab.

- Publish a release/tag such as `v0.2.0` when you want GitHub to build, test, package, and attach Windows, macOS, and Linux VST3 zips.
- Use the manual `Release VST3` workflow for candidate builds without creating a public release.
- The zip includes `Chipper.vst3`, `README.md`, `THIRD_PARTY_NOTICES.md`, and `LICENSE` when present.
- No build cache is configured yet, keeping the workflow simple and avoiding extra artifact/storage churn.

See [docs/release-builds.md](docs/release-builds.md) for the local build sequence, release workflow behavior, install verification, and release gate checklist.

## Install Locally

The root installer defaults to a user-scope install, so it does not need UAC:

```powershell
.\install-vst3.ps1
```

If `-BuildRoot` and `CHIPPER_BUILD_ROOT` are not set, the installer auto-selects the newest local VST3 bundle from `build-codex` or `build`. Pass `-BuildRoot` when you want a specific build folder.

### Make sure you installed the latest build

For day-to-day development, use this sequence from the repo root:

```powershell
cmake --build build-codex --config Release --target Chipper_VST3 chipper_render
.\install-vst3.ps1 -Scope User -BuildRoot build-codex
.\install-vst3.ps1 -VerifyOnly -BuildRoot build-codex
```

The installer prints a source build marker and the installed user/global build markers. The marker shown in Chipper's footer should match the installed marker, for example:

```text
Source build: v0.1.0 b9bfeeb283 (clean, built ...)
Installed build marker: ...\ChipperBuildInfo.txt
```

If Ableton or another host still shows an older footer hash, close the host and check whether both user and global copies exist:

```powershell
.\install-vst3.ps1 -VerifyOnly -BuildRoot build-codex
```

When the global copy is older, sync both locations from an elevated PowerShell:

```powershell
.\install-vst3.ps1 -Scope Both -BuildRoot build-codex
```

This matters because some hosts scan `C:\Program Files\Common Files\VST3` before `%LOCALAPPDATA%\Programs\Common\VST3`, so an old global bundle can shadow the freshly installed user bundle.

By default it installs to:

```text
%LOCALAPPDATA%\Programs\Common\VST3
```

To install to the global VST3 folder, use an elevated PowerShell:

```powershell
.\install-vst3.ps1 -Scope Global
```

Installer defaults can be steered with environment variables instead of editing the script:

| Variable | Purpose |
| --- | --- |
| `CHIPPER_BUILD_ROOT` | Build folder used by the installer; when unset, the newest local `build-codex` or `build` VST3 bundle is selected |
| `CHIPPER_INSTALL_SCOPE` | `User`, `Global`, or `Both`; default `User` |
| `CHIPPER_VST3_DESTINATION` | Explicit destination folder for one-scope installs |
| `CHIPPER_VST3_USER_DIR` | User-scope VST3 install folder |
| `CHIPPER_VST3_GLOBAL_DIR` | Global VST3 install folder |
| `CHIPPER_VST3_FALLBACK_DIR` | Optional fallback folder when fallback installs are enabled |
| `CHIPPER_VERIFY_INSTALL` | Set to `1`, `true`, or `yes` to report source/user/global build markers without copying files |
| `CHIPPER_ALLOW_FALLBACK_INSTALL` | Set to `1`, `true`, or `yes` to allow fallback copies when the requested destination cannot be replaced |

Examples:

```powershell
$env:CHIPPER_BUILD_ROOT = "build-debug"
.\install-vst3.ps1
```

```powershell
.\install-vst3.ps1 -BuildRoot build -Destination "D:\Audio\VST3"
```

For Codex/local development builds, you can still install the explicit build folder:

```powershell
.\install-vst3.ps1 -Scope User -BuildRoot build-codex
```

To verify which build a host may be loading without copying files:

```powershell
.\install-vst3.ps1 -VerifyOnly -BuildRoot build-codex
```

This prints the source build plus the user and global VST3 install markers. If those markers differ, update both scopes from an elevated PowerShell:

```powershell
.\install-vst3.ps1 -Scope Both -BuildRoot build-codex
```

The installer removes the previous `Chipper.vst3` bundle before copying the new one, writes a build marker into the installed bundle, and copies third-party legal notices into `Contents\Resources\Legal`. If a DAW has the plugin loaded, close the host and rerun the installer. If both user and global VST3 copies exist, keep them in sync with an elevated `.\install-vst3.ps1 -Scope Both -BuildRoot <your-build-folder>` so hosts do not load an older copy.

## Command-Line Renderer

`chipper_render` is the verification path for the engine. It accepts chip mode, behavior strictness, note/register events, render length, chip-specific parameters, and outputs WAV plus debug JSON. The command-line flag is still named `--accuracy` for compatibility with earlier tests and scripts, but it represents the same Inspired/Hybrid/Authentic strictness request shown in the plugin header:

```powershell
build\Release\chipper_render.exe --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --output-db -9 --out nes.wav --debug nes.json
```

List factory presets:

```powershell
build\Release\chipper_render.exe --list-presets --debug presets.json
build\Release\chipper_render.exe --list-presets --chip sid --debug sid-presets.json
```

Run the preset QA gate before adding or releasing factory sounds:

```powershell
.\scripts\verify-presets.ps1 -BuildRoot build-codex
```

## User Presets

The plugin header includes `Load` and `Save` buttons beside the preset browser. User presets are saved as plain `.chipperpreset` XML files so they can be shared, versioned, and inspected without a DAW-specific preset container.

User preset files store the same Chipper state used by host project recall, including chip mode, behavior strictness, play mode, the selected factory/user preset state, public parameters, register snapshots, and external sample-bank references. Audio sample data is not embedded. For portable sharing, put referenced NES DMC, SPC700, or Paula samples next to the preset or in a `Samples` folder beside it; Chipper will try those relative locations before falling back to the original local path.

The default save location is:

```text
Documents\Chipper Presets
```

Files saved there are scanned into the preset browser as a chip-filtered **User Presets** bank. Selecting a user preset from the browser loads the flat file directly; `Save` updates the loaded user preset file, `Save As` writes a new shareable copy, and a new sound asks for a `.chipperpreset` file name. A preset loaded from another folder also appears in the User Presets list for the current session, so downloaded/shared sounds stay reachable while you audition them.

The browser also includes an `Init Patch` entry at the top for a neutral chip-local starting point. It resets the current chip's sound controls while preserving the current Strictness and supported Play Mode.

Factory presets are grouped by chip-local musical categories in the browser, such as leads, basses, drums, UI sounds, FM patches, or sampler voices. The grouping is only presentation: selecting a preset still applies a normal Chipper parameter snapshot that can be edited, saved, and shared as a flat file.

`Save As` suggests the currently selected preset name when possible, which keeps variant files easy to recognize when building banks like `SID Dirty Bass 02.chipperpreset` or `SPC700 Echo Pad Soft.chipperpreset`.

## MIDI CC

Chipper keeps a stable MIDI CC map so hardware controllers and DAW automation can reach all current parameters. The compact map is shown in the plugin footer, exported in descriptor JSON, and documented in [docs/ui-architecture.md](docs/ui-architecture.md).

Important shared assignments:

| CC | Parameter |
| --- | --- |
| 70 | Chip Mode |
| 71 | Behavior Strictness (header label: Strictness) |
| 73 | Output Level |
| 74 | Preset recipe (internal compatibility parameter) |
| 75 | Play Mode |
| 76-79 | Chip-native performance controls |
| 80-87 | Source enable and level controls |
| 88-119 | Chip-specific register, envelope, routing, and sample controls |

Choice/register parameters quantize CC input to valid chip states. Continuous controls use the full 0-127 range.

## Licensing

First-party Chipper source code is licensed under **AGPL-3.0-or-later**. This is the conservative open-source path because the plugin currently uses JUCE under its open-source AGPL option unless a commercial JUCE license is used.

Current vendored emulator cores are permissive:

- `digital-sound-antiques/emu2149`: MIT
- `digital-sound-antiques/emu76489`: MIT
- `digital-sound-antiques/emu2413`: MIT
- `digital-sound-antiques/emu2212`: MIT
- `aaronsgiles/ymfm`: BSD-3-Clause

Do not import GPL/LGPL emulator code, preset banks, songs, samples, lookup tables, or patch dumps without a documented compatibility plan. Candidate and reference projects are tracked in [docs/emulator-source-map.md](docs/emulator-source-map.md), and bundled notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Documentation

- [docs/emulation-accuracy.md](docs/emulation-accuracy.md): per-chip implementation status, verification evidence, and known gaps
- [docs/emulator-source-map.md](docs/emulator-source-map.md): source candidates, license audit tiers, and reference-only projects
- [docs/product-spec.md](docs/product-spec.md): product goals, chip modes, and UX/DSP direction
- [docs/ui-architecture.md](docs/ui-architecture.md): adaptive UI strategy and MIDI/automation contract
- [docs/ui-priority-audit.md](docs/ui-priority-audit.md): current chip-aware layout rules and screenshot non-regression checklist
- [docs/snes-spc700-plan.md](docs/snes-spc700-plan.md): SPC700-style sampler roadmap
- [docs/priority-roadmap.md](docs/priority-roadmap.md): execution priorities
- [docs/product-gap-roadmap.md](docs/product-gap-roadmap.md): larger workflow gaps and non-regression policy
- [docs/release-builds.md](docs/release-builds.md): local install, GitHub release builds, and release gates

## Strictness And Verification Rule

Chipper should sound like an instrument, but it should not overclaim. The header's Strictness selector requests Inspired, Hybrid, or Authentic behavior; it does not prove that a chip mode is fully accurate. The host/MIDI parameter is named `Behavior Strictness` to make automation lanes self-explanatory. If a mode is not verified at register/timing level, label it as inspired, style, or partial. Cycle-accurate claims require accepted test suites, trusted emulator comparisons, or real hardware captures.
