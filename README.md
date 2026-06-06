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
- **Visual feedback:** chip-state source scopes, SID ADSR previews, and a post-trim final-output scope
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

The installer removes the previous `Chipper.vst3` bundle before copying the new build, which avoids stale mixed bundles when a host has cached an older plugin. Installing to `C:\Program Files\Common Files\VST3` requires an elevated shell; non-elevated global installs fail before deleting the old bundle.

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
| 84 | Source 1 Level |
| 85 | Source 2 Level |
| 86 | Source 3 Level |
| 87 | Source 4 Level |
| 88 | Envelope Decay / Speed |
| 89 | Wave Shape |
| 90 | YM Envelope Shape / SID Filter Mode |
| 91 | Noise Mode / SID Osc Mod |
| 92 | DMG Wave Level |
| 93 | Stereo Spread / SID Resonance |
| 94 | DMG Stereo Routing / SID Model |
| 95 | SID Voice 2 Waveform |
| 96 | SID Voice 3 Waveform |
| 97 | SID Attack |
| 98 | SID Release |
| 99 | SID Decay |
| 100 | SID Sustain |
| 101 | YM Channel A Mix |
| 102 | YM Channel B Mix |
| 103 | YM Channel C Mix |
| 104 | NES Pulse 2 Duty |
| 105 | SID Voice 2 Attack |
| 106 | SID Voice 2 Decay |
| 107 | SID Voice 2 Sustain |
| 108 | SID Voice 2 Release |
| 109 | SID Voice 3 Attack |
| 110 | SID Voice 3 Decay |
| 111 | SID Voice 3 Sustain |
| 112 | SID Voice 3 Release |
| 113 | SID Filter Routing |

Continuous parameters use the full 0-127 CC range. Choice and register-style parameters quantize to their available choices, and source enables switch off below the midpoint and on at or above the midpoint. Source Level trims are independent continuous parameters on CC84-87 for Source 1-4, so hardware controllers can balance pulse/wave/noise, SID voice 1/2/3, YM A/B/C/noise, or PSG tone/noise lanes without changing the selected macro. CC74 and macro-only host automation apply the selected chip-specific macro template to native controls, source enables, helper choices, resonance/spread helpers, and unity source-level defaults, matching the editor dropdown behavior; preset/state restores that change macro and controls together keep the restored snapshot. Factory presets can also pin SID Voice 2/3 waveform overrides and SID filter routing so the UI updates to the preset's mixed oscillator/filter stack. Noise Mode is chip-aware: NES/RP2A03 resolves Macro, Long LFSR, or Short LFSR into `$400E`; Game Boy/DMG resolves Macro, 15-bit LFSR, or 7-bit LFSR into `NR43`; and SN76489 resolves Macro or PSG noise-control choices into its noise register. NES Pulse 1 Duty stays on Native Control 1 / CC76, while NES Pulse 2 Duty is a dedicated register choice on CC104: Macro preserves the musical template, and explicit choices write pulse channel 2 duty bits independently. NES Noise Period stays on Native Control 3 / CC78 and reaches the full `$400E` period nibble. DMG Sweep Shift stays on Native Control 2 / CC77 and maps to the `NR10` channel-1 sweep shift bits used by Chipper's handheld pitch gestures. DMG Noise Clock also stays on Native Control 3 / CC78 and maps to the `NR43` clock shift used by Chipper's musical noise control. DMG Envelope Level stays on Native Control 4 / CC79 and maps to the NRx2 initial volume nibble used by the pulse envelope registers. SID Pulse Width is on CC76 and maps to the 12-bit pulse-width registers, SID Cutoff is on CC78 and writes the cutoff registers, SID Sustain is on CC79 and maps to the SR sustain nibble, SID ADSR Speed is on CC88 and maps macro envelope speed into ADSR nibbles, and SID Attack/Decay/Sustain/Release have `Macro` plus exact 0-15 nibble choices for the AD/SR registers on CC97/CC99/CC100/CC98 respectively while the editor readout shows the resolved AD/SR bytes plus A/D/S/R nibbles. Full ADSR surfaces should be exposed only for chips whose native model supports ADSR/operator envelopes, such as SID and FM chips; PSG/APU chips should expose their hardware envelope, length, attenuation, or decay behavior instead. SID Voice 1 Wave is on CC89 with Macro, Tri, Saw, Pulse, and Noise choices, SID Voice 2/3 Wave are on CC95/CC96 with the same choices and map independently to each voice control register, SID Filter Mode is on CC90 with Macro, LP, BP, HP, and Off choices, SID Osc Mod is on CC91 with Macro, Off, Sync, Ring, and Both choices for the voice control-register sync/ring bits, SID Resonance is on CC93 as the `$D417` resonance nibble, SID Model is on CC94 with Macro, 6581, and 8580 choices for Chipper's partial filter/output variant profile, and SID Filter Routing is on CC113 with Macro, All, V1, V2, V3, V1+V2, V1+V3, V2+V3, and None choices for the `$D417` low voice-routing bits. DMG Wave Level is on CC92 and maps to `NR32` channel-3 output level choices: Macro, Mute, 100%, 50%, and 25%. DMG Stereo Routing is also on CC94 in DMG mode and maps to `NR51` choices: Macro, Both, Left, Right, and Split. YM2149 Noise Pitch stays on Native Control 3 / CC78 and maps to register 6, the 5-bit shared noise period. YM2149 Tone/Noise Mix stays on Native Control 4 / CC79 and quantizes to register 7 mixer choices for noise-only, tone-only, or combined tone+noise output. YM2149 Channel A/B/C Mix live on CC101-103; Macro follows Native Control 4 while Tone, Noise, Both, and Off override the channel's register 7 tone/noise enable bits. SN76489 Noise Bias stays on CC78 and biases the macro-selected PSG noise-control register when Noise Mode is Macro; explicit Noise Mode choices override it. SN76489 Noise Level also stays on CC79 and maps to the 4-bit PSG noise attenuation register. YM2149 Envelope Speed is on CC88 and writes the hardware envelope period registers 11/12 while preserving the default period at zero; Wave Shape is CC89, YM Envelope Shape is CC90, and shared Noise Mode is CC91. Stereo Spread is on CC93 and is a modern output convenience for YM2149/AY and SN76489/Sega PSG: zero preserves mono-native chip output, while higher values spread native tone channels left/center/right. The plugin footer shows the fixed CC range, and its tooltip lists the full hardware-control map; individual control tooltips also show the relevant CC. Automated smoke tests assert that every APVTS parameter has exactly one default MIDI CC mapping, that each mapping works through the processor MIDI path, that MIDI and host macro-only changes apply macro templates through the processor path, and that every automatable chip descriptor control resolves to a MIDI-mapped APVTS parameter. The renderer descriptor JSON exports each visible control's APVTS parameter id and default CC plus a top-level `midiCcMappings` list for the full plugin parameter map.

The command-line renderer exposes the same NES Pulse 2 Duty override as `--nes-pulse2-duty macro|12.5|25|50|75`, the same SID Filter Routing choices as `--sid-filter-routing macro|all|v1|v2|v3|v1+v2|v1+v3|v2+v3|none`, and the same YM channel mixer overrides as `--ym-channel-a-mix`, `--ym-channel-b-mix`, and `--ym-channel-c-mix`. Debug JSON reports the stored choices, resolved register values, and per-channel tone/noise/filter-routing enables so UI, MIDI, renderer, and engine behavior can be tested against one contract.

## Command-Line Renderer

The separate renderer is the verification path for chip cores:

```powershell
cmake --build build --config Release --target chipper_render
build\Release\chipper_render.exe --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --output-db -9 --out nes.wav --debug nes.json
```

It accepts chip mode, accuracy, musical macro, native macro controls, output trim, clock rate, note input, register-write event files, render length, and writes WAV plus debug JSON. SID scripts may use `--sid-adsr-speed` as a readable alias for the shared `--envelope-decay` control, plus `--sid-attack macro|0..15`, `--sid-decay macro|0..15`, `--sid-sustain macro|0..15`, and `--sid-release macro|0..15` for exact AD/SR nibble overrides; debug JSON reports the resolved SID AD/SR register bytes and A/D/S/R nibbles. Factory presets apply the same output-trim snapshot in the renderer that the VST applies in the editor, and debug JSON records `outputDb` plus the resolved linear gain. Golden WAV/reference metadata should be added only from trusted emulator or hardware comparisons.

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
