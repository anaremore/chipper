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

## Install Locally

The root installer defaults to a user-scope install, so it does not need UAC:

```powershell
.\install-vst3.ps1
```

If `-BuildRoot` and `CHIPPER_BUILD_ROOT` are not set, the installer auto-selects the newest local VST3 bundle from `build-codex` or `build`. Pass `-BuildRoot` when you want a specific build folder.

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

The installer removes the previous `Chipper.vst3` bundle before copying the new one, writes a build marker into the installed bundle, and copies third-party legal notices into `Contents\Resources\Legal`. If a DAW has the plugin loaded, close the host and rerun the installer. If both user and global VST3 copies exist, keep them in sync with an elevated `.\install-vst3.ps1 -Scope Both -BuildRoot <your-build-folder>` so hosts do not load an older copy.

## Command-Line Renderer

`chipper_render` is the verification path for the engine. It accepts chip mode, accuracy, note/register events, render length, chip-specific parameters, and outputs WAV plus debug JSON:

```powershell
build\Release\chipper_render.exe --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --output-db -9 --out nes.wav --debug nes.json
```

List factory presets:

```powershell
build\Release\chipper_render.exe --list-presets --debug presets.json
build\Release\chipper_render.exe --list-presets --chip sid --debug sid-presets.json
```

## User Presets

The plugin header includes `Load` and `Save` buttons beside the preset browser. User presets are saved as plain `.chipperpreset` XML files so they can be shared, versioned, and inspected without a DAW-specific preset container.

User preset files store the same Chipper state used by host project recall, including chip mode, accuracy, template, play mode, public parameters, register snapshots, and external sample-bank references. Audio sample data is not embedded. For portable sharing, put referenced NES DMC, SPC700, or Paula samples next to the preset or in a `Samples` folder beside it; Chipper will try those relative locations before falling back to the original local path.

The default save location is:

```text
Documents\Chipper Presets
```

Files saved there are scanned into the preset browser as a chip-filtered **User Presets** bank. Selecting a user preset from the browser loads the flat file directly; `Save` updates the loaded user preset file, `Save As` writes a new shareable copy, and a new sound asks for a `.chipperpreset` file name. A preset loaded from another folder also appears in the User Presets list for the current session, so downloaded/shared sounds stay reachable while you audition them.

## MIDI CC

Chipper keeps a stable MIDI CC map so hardware controllers and DAW automation can reach all current parameters. The compact map is shown in the plugin footer, exported in descriptor JSON, and documented in [docs/ui-architecture.md](docs/ui-architecture.md).

Important shared assignments:

| CC | Parameter |
| --- | --- |
| 70 | Chip Mode |
| 71 | Accuracy |
| 73 | Output Level |
| 74 | Template / macro |
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
- [docs/snes-spc700-plan.md](docs/snes-spc700-plan.md): SPC700-style sampler roadmap
- [docs/priority-roadmap.md](docs/priority-roadmap.md): execution priorities

## Accuracy Rule

Chipper should sound like an instrument, but it should not overclaim. If a mode is not verified at register/timing level, label it as inspired, style, or partial. Cycle-accurate claims require accepted test suites, trusted emulator comparisons, or real hardware captures.
