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
.\scripts\install-vst3.ps1
```

The installer removes the previous `Chipper.vst3` bundle before copying the new build, which avoids stale mixed bundles when a host has cached an older plugin. Installing to `C:\Program Files\Common Files\VST3` may require an elevated shell.

For rapid development without a UAC prompt, install to the per-user VST3 folder:

```powershell
.\scripts\install-vst3.ps1 -Scope User
```

This copies to `%LOCALAPPDATA%\Programs\Common\VST3`, which is the user-level VST3 location. You can still pass `-Destination` for a custom host scan folder.

## MIDI CC Control

Chipper accepts MIDI input for notes and parameter control. All current plugin parameters have fixed default MIDI CC mappings:

| CC | Parameter |
| --- | --- |
| 70 | Chip Mode |
| 71 | Accuracy |
| 72 | Clock Rate Override |
| 73 | Output Level |
| 74 | Musical Macro |
| 75 | Play Mode |
| 76 | Native Control 1 |
| 77 | Native Control 2 |
| 78 | Native Control 3 |
| 79 | Native Control 4 |
| 80 | Source 1 Enabled |
| 81 | Source 2 Enabled |
| 82 | Source 3 Enabled |
| 83 | Source 4 Enabled |
| 84 | Envelope Decay |
| 85 | Wave Shape |
| 86 | YM Envelope Shape |
| 87 | Noise Mode |

Continuous parameters use the full 0-127 CC range. Choice and register-style parameters quantize to their available choices, and source enables switch off below the midpoint and on at or above the midpoint. Noise Mode is chip-aware: NES/RP2A03 resolves Macro, Long LFSR, or Short LFSR into `$400E`; Game Boy/DMG resolves Macro, 15-bit LFSR, or 7-bit LFSR into `NR43`; and SN76489 resolves Macro or PSG noise-control choices into its noise register. Automated smoke tests assert that every APVTS parameter has exactly one default MIDI CC mapping and that each mapping works through the processor MIDI path.

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
