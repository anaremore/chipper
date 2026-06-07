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
- **Chip-specific templates:** the editor presents macro/template choices with chip-aware names such as NES Coin Blip, SID Dirty Bass, YM Noise Perc, and PSG Drum while preserving stable host automation values
- **Presets:** curated bank covering NES, SID, YM, Sega PSG, arcade, tracker, FM, and modern hybrid chip categories, including SID patches that pin per-voice waveforms, ADSR nibbles, filter routing, and source levels

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

Install the current local development build:

```powershell
.\install-vst3.ps1
```

The root installer defaults to `-BuildRoot build-codex -Scope User`, which copies the latest Codex-built artifact to `%LOCALAPPDATA%\Programs\Common\VST3` without a UAC prompt. It removes the previous `Chipper.vst3` bundle before copying the new build, which avoids stale mixed bundles when a host has cached an older plugin.

If Ableton or another host still has Chipper loaded, the root installer fails instead of writing a fallback copy somewhere your host may not scan. Close the host and rerun the command. To intentionally allow a fallback folder for manual scanning, pass `-AllowFallback`.

For an explicit build folder, pass `-BuildRoot`:

```powershell
.\install-vst3.ps1 -BuildRoot build
```

User-scope installs also retry replacement after repairing the current user's access to a stale `Chipper.vst3` bundle, which keeps rapid build/install loops out of UAC when the host has been closed. You can still pass `-Destination` for a custom host scan folder.

Install to the common Windows VST3 folder only from an elevated shell:

```powershell
.\install-vst3.ps1 -Scope Global
```

Global installs target `C:\Program Files\Common Files\VST3`; non-elevated global installs fail before deleting the old bundle. The lower-level `.\scripts\install-vst3.ps1` remains available and defaults to `-BuildRoot build -Scope Global` for release-style workflows.

If both the user and global VST3 folders contain `Chipper.vst3`, the installer reports the other copy after a successful install. Some hosts scan the global VST3 folder before the user VST3 folder, so a stale global copy can make the plugin look old even when the no-UAC user install succeeded.

To update both copies in one pass, open PowerShell as Administrator and run:

```powershell
.\install-vst3.ps1 -Scope Both
```

## MIDI CC Control

Chipper accepts MIDI input for notes and parameter control. All current plugin parameters have fixed default MIDI CC mappings:

| CC | Parameter |
| --- | --- |
| 68 | NES DMC Loop |
| 69 | NES DMC Map Root |
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
| 91 | Noise Mode / SID Osc Interaction |
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
| 114 | SID Voice 2 Pulse Width |
| 115 | SID Voice 3 Pulse Width |
| 116 | NES DMC Direct Level |
| 117 | NES DMC Sample Slot |
| 118 | NES DMC Rate |
| 119 | NES DMC Playback Mode |

Continuous parameters use the full 0-127 CC range. Choice and register-style parameters quantize to their available choices, and source enables switch off below the midpoint and on at or above the midpoint. Source Level trims are independent continuous parameters on CC84-87 for Source 1-4, so hardware controllers can balance pulse/wave/noise, SID voice 1/2/3, YM A/B/C/noise, or PSG tone/noise lanes without changing the selected macro. CC74 and macro-only host automation apply the selected chip-specific macro template to native controls, source enables, helper choices, resonance/spread helpers, and unity source-level defaults, matching the editor dropdown behavior; preset/state restores that change macro and controls together keep the restored snapshot.

Choice zero is presented as `Follow` where it means "follow the selected macro, preset, or resolved voice state." That keeps register controls honest without making users choose confusing `Macro`, `Off`, or `None` values when they really want the curated patch behavior. Factory presets can pin SID Voice 2/3 waveform overrides, independent Voice 2/3 pulse-width controls, SID filter modes, and SID filter routing so the UI updates to the preset's mixed oscillator/filter stack.

Noise Mode is chip-aware: NES/RP2A03 resolves Follow, Long LFSR, or Short LFSR into `$400E`; Game Boy/DMG resolves Follow, 15-bit LFSR, or 7-bit LFSR into `NR43`; and SN76489 resolves Follow or PSG noise-control choices into its noise register. NES Pulse 1 Duty stays on Native Control 1 / CC76, while NES Pulse 2 Duty is a dedicated register choice on CC104: Follow preserves the musical template, and explicit choices write pulse channel 2 duty bits independently. NES DMC Direct Level appears in the NES Performance area, is on CC116, and maps to the 7-bit `$4011` DAC load for direct grit. The VST can load one `.dmc` file or stage a user-selected folder of `.dmc` files into a local checklist; checked entries become up to 32 in-memory DMC slots, and CC117 / NES DMC Sample Slot selects the active preloaded slot. The bank editor also has First 32, Invert, and Clear actions for quickly curating a playable folder bank without importing samples into the repo or host project. NES DMC Rate is on CC118 and maps to the `$4010` rate index; matching that rate to the sample's DPCM encode rate is important because a valid `.dmc` file can sound like shaped noise when clocked incorrectly. NES DMC Loop is on CC68 and maps to `$4010` bit 6, switching the loaded sample between one-shot playback and repeat-from-start loop playback. NES DMC Playback Mode is on CC119: Manual Slot plays the selected sample for every note, while Note Map maps the checked bank across MIDI notes from the CC69 DMC Map Root on the monophonic NES DMC lane. The DMC status readout reports the selected or mapped sample's byte count, estimated playback duration, current one-shot/loop state, and current Note Map key span at the current rate, which helps catch tiny files or rate mismatches quickly. Chipper stores DMC file paths and checked states in plugin state so host projects can recall the bank when files remain on disk, but it does not embed sample bytes in the project. The renderer also accepts an external `.dmc` path, `--nes-dmc-rate 0..15`, and `--nes-dmc-loop 0|1` for partial DPCM byte stepping; exact DMC DMA/address/IRQ behavior and WAV-to-DMC conversion remain planned, and sample content should stay user-provided rather than committed to the repo. DMG Wave Level is on CC92 and maps to `NR32` channel-3 output level choices: Follow, Mute, 100%, 50%, and 25%. DMG Stereo Routing is also on CC94 in DMG mode and maps to `NR51` choices: Follow, Both, Left, Right, and Split.

SID Voice 1 Pulse Width is on CC76, SID Voice 2 Pulse Width is on CC114, and SID Voice 3 Pulse Width is on CC115; each maps to its own 12-bit register pair. SID Cutoff is on CC78, SID Sustain is on CC79, SID ADSR Speed is on CC88, and SID Attack/Decay/Sustain/Release stepped controls use Follow plus exact 0-15 values for each voice's AD/SR registers. SID Voice 1 Wave is on CC89 with Follow, Tri, Saw, Pulse, Noise, and combined waveform choices, while SID Voice 2/3 Wave are on CC95/CC96 with the same choices. Voice 3 is treated as an audible oscillator voice unless its source is disabled; OSC3/ENV3 readback, silent utility/mod-source use, and model-specific Voice 3 leakage remain planned and are not claimed as verified emulation yet. SID Filter Mode is on CC90 with Follow, LP, BP, HP, Off, Notch, LP+BP, BP+HP, and All choices for `$D418`, SID Osc Interaction is on CC91 with Follow, Off, Sync, Ring, and Both choices, SID Resonance is on CC93 as the `$D417` resonance nibble, SID Model is on CC94 with Auto, 6581, and 8580 choices, and SID Filter Routing is on CC113 with Follow, All, V1, V2, V3, V1+V2, V1+V3, V2+V3, and None choices.

YM2149 Noise Pitch stays on Native Control 3 / CC78 and maps to register 6. YM2149 Tone/Noise Mix stays on Native Control 4 / CC79 and quantizes to register 7 mixer choices for noise-only, tone-only, or combined tone+noise output. YM2149 Envelope Shape is on CC90 with friendly Fixed/Fall/Rise/Saw/Triangle choices plus exact register-13 codes `0x00` through `0x0F`; the UI uses a dropdown because the full hardware set is too dense for segmented buttons. YM2149 Channel A/B/C Mix live on CC101-103; Follow uses Native Control 4 while Tone, Noise, Both, and Off override the channel's register 7 tone/noise enable bits. SN76489 Noise Bias stays on CC78 and biases the macro-selected PSG noise-control register when Noise Mode is Follow; explicit Noise Mode choices override it. The plugin footer shows the fixed CC range, and its tooltip lists the full hardware-control map; individual control tooltips also show the relevant CC.

Automated smoke tests assert that every APVTS parameter has exactly one default MIDI CC mapping, that each mapping works through the processor MIDI path, that MIDI and host macro-only changes apply macro templates through the processor path, and that every automatable chip descriptor control resolves to a MIDI-mapped APVTS parameter. The renderer descriptor JSON exports each visible control's APVTS parameter id and default CC plus a top-level `midiCcMappings` list for the full plugin parameter map.

Factory presets can pin source-level balances, DMG Wave Level choices, SID filter-mode combinations, filter routing, per-voice waveforms, and per-voice ADSR nibble choices, so choosing a preset updates the visible controls to match the rendered patch rather than hiding those decisions inside the engine.

The command-line renderer exposes the same NES Pulse 2 Duty override as `--nes-pulse2-duty follow|12.5|25|50|75`, external NES DPCM sample bytes as `--nes-dmc-sample path.dmc`, the same DMC rate index as `--nes-dmc-rate 0..15`, the same SID Filter Mode choices as `--sid-filter-mode follow|lp|bp|hp|off|notch|lp+bp|bp+hp|all` plus exact `$D418` filter-bit values such as `0x50`, the same SID Filter Routing choices as `--sid-filter-routing follow|all|v1|v2|v3|v1+v2|v1+v3|v2+v3|none` plus exact `$D417` low-bit values `0x00..0x07`, the same independent SID pulse-width controls as `--sid-voice2-pulse-width` and `--sid-voice3-pulse-width`, the same YM envelope shape choices as `--ym-envelope-shape fixed|fall|rise|saw|triangle|code0..code15|0x0..0xF`, and the same YM channel mixer overrides as `--ym-channel-a-mix`, `--ym-channel-b-mix`, and `--ym-channel-c-mix`. Debug JSON reports the stored choices, resolved register values, and per-channel tone/noise/filter-routing enables so UI, MIDI, renderer, and engine behavior can be tested against one contract. CLI choice arguments still accept `macro` as a stable script-friendly alias for the UI's Follow choice.

## Command-Line Renderer

The separate renderer is the verification path for chip cores:

```powershell
cmake --build build --config Release --target chipper_render
build\Release\chipper_render.exe --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --output-db -9 --out nes.wav --debug nes.json
```

It accepts chip mode, accuracy, musical macro, native macro controls, output trim, clock rate, note input, register-write event files, render length, and writes WAV plus debug JSON. SID scripts may use `--sid-adsr-speed` as a readable alias for the shared `--envelope-decay` control, plus `--sid-attack follow|0..15`, `--sid-decay follow|0..15`, `--sid-sustain follow|0..15`, and `--sid-release follow|0..15` for exact AD/SR nibble overrides; debug JSON reports the resolved SID AD/SR register bytes, A/D/S/R nibbles, and all three 12-bit pulse-width registers. Factory presets apply the same output-trim snapshot in the renderer that the VST applies in the editor, and debug JSON records `outputDb` plus the resolved linear gain. Golden WAV/reference metadata should be added only from trusted emulator or hardware comparisons.

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
