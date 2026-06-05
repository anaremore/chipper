# Chipper

Chipper is a focused chip synth VST3 for fast classic game sounds: NES-style pulse leads and triangle basses, SID/C64-style PWM grit and filter movement, YM2149/AY-style beeps and arps, Sega PSG tones, FM-chip basses, tracker-style samples, and arcade SFX.

The product rule is strict: UX can be simple, but chip modes must not be generic retro oscillators with famous labels. Modes are either backed by a permissive-license emulation core or by a clean-room register-level model. If a mode is not verified at the register/timing level, it must be labeled **chip-inspired**, not accurate emulation.

## Product Shape

- **Core Chip Modes:** NES/RP2A03, Game Boy/DMG APU, SID 6581/8580, YM2149/AY, SN76489, YM2612/OPN2, OPL2/OPL3
- **Expansion Modes:** SPC700-style, POKEY, Paula, HuC6280, Namco WSG, YM2151, YM2413/OPLL, Konami SCC
- **Accuracy:** Inspired, Hybrid, Authentic; Cycle-accurate is only allowed after accepted tests or hardware captures prove it
- **Signal flow:** Chip mode and source mix -> envelope -> motion -> crunch/tone/filter -> output
- **Core sounds:** register-backed pulse/square/noise/FM/sample/wavetable behavior where implemented
- **Performance:** host-synced arp, retrigger, vibrato, glide, pitch rise/drop/coin/jump/laser motion
- **SFX helper:** kick, snare, hat, explosion, laser, jump, coin, power-up, hit, menu blip
- **Presets:** curated bank covering NES, SID, YM, Sega PSG, arcade, tracker, FM, and modern hybrid chip categories

## Build

Requirements:

- CMake 3.22 or newer
- A Windows C++ toolchain, preferably Visual Studio 2019/2022 with the Desktop C++ workload
- Internet access on first configure so CMake can fetch JUCE 8.0.13

Configure and build:

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release --target Chipper_VST3
```

If Visual Studio 2022 is installed, use:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Chipper_VST3
```

The built plugin bundle is expected at:

```text
build\Chipper_artefacts\Release\VST3\Chipper.vst3
```

Install to the common Windows VST3 folder:

```powershell
Copy-Item -Recurse -Force "build\Chipper_artefacts\Release\VST3\Chipper.vst3" "C:\Program Files\Common Files\VST3\"
```

Copying to `C:\Program Files\Common Files\VST3` may require an elevated shell.

The repo also includes a helper:

```powershell
.\scripts\install-vst3.ps1
```

If you do not have an elevated shell, a per-user fallback location is:

```powershell
Copy-Item -Recurse -Force "build\Chipper_artefacts\Release\VST3\Chipper.vst3" "$env:LOCALAPPDATA\Programs\Common\VST3\"
```

## Command-Line Renderer

The separate renderer is the verification path for chip cores:

```powershell
cmake --build build --config Release --target chipper_render
build\Release\chipper_render.exe --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --out nes.wav --debug nes.json
```

It accepts chip mode, accuracy, musical macro, native macro controls, clock rate, note input, register-write event files, render length, and writes WAV plus debug JSON. Golden WAV/reference metadata should be added only from trusted emulator or hardware comparisons.

Event trace files support:

```text
write <sample> <address> <value>
note_on <sample> <note> <velocity>
note_off <sample> <note>
note <sample> <note> <velocity> <lengthSamples>
```

## Design Notes

Chipper keeps chip limitations creative instead of bureaucratic. Modes expose musical controls rather than registers, but those controls map to the internal chip model. The `Authentic` setting should follow register/channel/timing behavior where feasible; `Inspired` is allowed to be musical but must be labeled honestly; `Hybrid` is the default product mode.

See [docs/product-spec.md](docs/product-spec.md) for the concise spec, parameter list, preset list, DSP architecture, and UI plan.
See [docs/emulation-accuracy.md](docs/emulation-accuracy.md) for the accuracy and licensing matrix.
See [docs/emulator-source-map.md](docs/emulator-source-map.md) for emulator source candidates, references, validation targets, and license-audit tiers.
See [docs/ui-architecture.md](docs/ui-architecture.md) for the data-driven adaptive UI shell and stable automation strategy.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for emulator credits, candidate cores, and license-audit notes.
