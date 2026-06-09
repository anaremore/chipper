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
- **Visual feedback:** chip-state source scopes, SID ADSR previews, YM/AY envelope-shape previews, and a post-trim final-output scope
- **SFX helper:** kick, snare, hat, explosion, laser, jump, coin, power-up, hit, menu blip
- **Chip-specific templates:** the editor presents macro/template choices with chip-aware names such as NES Coin Blip, SID Dirty Bass, YM Noise Perc, PSG Drum, OPN2 Feedback Bass, SPC700 Drum Map, POKEY Buzzy Lead, Paula Tracker Bass, HuC6280 Glass Lead, Namco Arcade Lead, and SCC Arcade Lead while preserving stable host automation values; planned chips expose roadmap templates without implying audible or verified emulation
- **Presets:** chip-local curated banks covering NES, SID, YM, Sega PSG, SPC700-style, POKEY, Amiga Paula, PC Engine/HuC6280, Namco WSG, Konami SCC, arcade, tracker, FM, and modern hybrid chip categories; the editor shows the selected chip's preset count and choosing a preset updates visible controls such as SID per-voice waveforms, ADSR nibbles, filter routing, and source levels

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
| 90 | YM Envelope Shape / SID Filter Mode / OPN2 Envelope Shape |
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

Noise Mode is chip-aware: NES/RP2A03 resolves Follow, Long LFSR, or Short LFSR into `$400E`; Game Boy/DMG resolves Follow, 15-bit LFSR, or 7-bit LFSR into `NR43`; and SN76489 resolves Follow or PSG noise-control choices into its noise register. NES Pulse 1 Duty stays on Native Control 1 / CC76, while NES Pulse 2 Duty is a dedicated register choice on CC104: Follow preserves the musical template, and explicit choices write pulse channel 2 duty bits independently. NES DMC Direct Level appears in the NES Performance area, is on CC116, and maps to the 7-bit `$4011` DAC load for direct grit. The VST can load one `.dmc` file or stage a user-selected folder of `.dmc` files into a local checklist; checked entries become up to 32 in-memory DMC slots, and CC117 / NES DMC Sample Slot selects the active preloaded slot. The bank editor also has First 32, Invert, and Clear actions for quickly curating a playable folder bank without importing samples into the repo or host project. NES DMC Rate is on CC118 and maps to the `$4010` rate index; matching that rate to the sample's DPCM encode rate is important because a valid `.dmc` file can sound like shaped noise when clocked incorrectly. NES DMC Loop is on CC68 and maps to `$4010` bit 6, switching the loaded sample between one-shot playback and repeat-from-start loop playback. NES DMC Playback Mode is on CC119: Manual Slot plays the selected sample for every note, Note Map maps the checked bank across MIDI notes from the CC69 DMC Map Root on the monophonic NES DMC lane, and Sample Map Only uses the same bank mapping while suppressing pulse, triangle, and noise so the DMC lane behaves like a focused sample keyboard. In both map modes, notes outside the visible mapped key span leave the DMC lane silent instead of clamping to the first or last sample. The DMC status readout reports the selected or mapped sample's byte count, estimated playback duration, current one-shot/loop state, and current mapped key span at the current rate, which helps catch tiny files or rate mismatches quickly. Chipper stores DMC file paths and checked states in plugin state so host projects can recall the bank when files remain on disk, but it does not embed sample bytes in the project. The renderer also accepts an external `.dmc` path, `--nes-dmc-rate 0..15`, `--nes-dmc-loop 0|1`, and `--nes-dmc-only 0|1` for partial DPCM byte stepping; exact DMC DMA/address/IRQ behavior and WAV-to-DMC conversion remain planned, and sample content should stay user-provided rather than committed to the repo. DMG Wave Level is on CC92 and maps to `NR32` channel-3 output level choices: Follow, Mute, 100%, 50%, and 25%. DMG Stereo Routing is also on CC94 in DMG mode and maps to `NR51` choices: Follow, Both, Left, Right, and Split.

SID Voice 1 Pulse Width is on CC76, SID Voice 2 Pulse Width is on CC114, and SID Voice 3 Pulse Width is on CC115; each maps to its own 12-bit register pair. SID Cutoff is on CC78, SID Sustain is on CC79, SID ADSR Speed is on CC88, and SID Attack/Decay/Sustain/Release stepped controls use Follow plus exact 0-15 values for each voice's AD/SR registers. SID Voice 1 Wave is on CC89 with Follow, Tri, Saw, Pulse, Noise, and combined waveform choices, while SID Voice 2/3 Wave are on CC95/CC96 with the same choices. Voice 3 is treated as an audible oscillator voice unless its source is disabled; OSC3/ENV3 readback, silent utility/mod-source use, and model-specific Voice 3 leakage remain planned and are not claimed as verified emulation yet. SID Filter Mode is on CC90 with Follow, LP, BP, HP, Off, Notch, LP+BP, BP+HP, and All choices for `$D418`, SID Osc Interaction is on CC91 with Follow, Off, Sync, Ring, and Both choices, SID Resonance is on CC93 as the `$D417` resonance nibble, SID Model is on CC94 with Auto, 6581, and 8580 choices, and SID Filter Routing is on CC113 with Follow, All, V1, V2, V3, V1+V2, V1+V3, V2+V3, and None choices.

YM2612 / Genesis FM is backed by vendored BSD-3-Clause `ymfm` for a verified-partial OPN2 adapter. Algorithm Bias is on CC76, Feedback is on CC77, Operator Tone is on CC78, FM Level is on CC79, explicit Algorithm choice is on CC89 with Follow plus YM2612 algorithms 0-7, and OPN2 Pan is on CC94 with Follow, Both, Left, Right, and Alt choices that write the native `$B4` channel L/R output bits. DAC Mode is on CC91 with Follow, FM Ch6, and DAC Drum choices; DAC Drum enables the native channel-6 DAC through `$2B` and streams generated 8-bit drum bytes through `$2A` via the ymfm core. The YM2612 UI presents Algorithm as a dropdown plus a four-operator signal-flow preview so the selected or macro-resolved `$B0` algorithm is readable. The adapter now renders, allocates, and exposes all six YM2612 melodic channels; source channels 5 and 6 are host/MIDI-controllable via CC64-67 alongside the first four source lanes. User PCM sample import for the OPN2 DAC, a dedicated operator-grid UI, per-operator ADSR controls, LFO/AMS/PMS, and hardware validation remain planned.

OPL2/OPL3 / DOS FM currently uses vendored BSD-3-Clause `ymfm` through its YM3812/OPL2 core for a verified-partial adapter. All nine OPL2 melodic channels are exposed as playable lanes for Big Mono stacks and Chip Poly; channels 1-8 use the shared source CC block and channel 9 uses CC58/59 for enable/level. Operator Balance is on CC76, Feedback is on CC77, Operator Tone is on CC78, FM Level is on CC79, and OPL2 Waveform is on CC89 with Follow, Sine, Half-Sine, Abs-Sine, and Pseudo-Saw choices. OPL Rhythm Mode is on CC90 with Follow, Melodic, and Rhythm choices; Rhythm writes native `$BD` rhythm-enable/key bits and maps channels 7-9 into BD, HH+SD, and TOM+CYM through the ymfm core. The OPL UI presents Waveform as a dropdown plus a register-shaped preview so the selected or template-resolved `$E0` operator waveform is readable, while Rhythm Mode appears as a segmented chip-register control. OPL3 stereo, 18-channel mode, four-operator pairs, deep rhythm-kit editing, and deep per-operator ADSR UI remain planned.

YM2151 / OPM arcade FM is backed by vendored BSD-3-Clause `ymfm` for a verified-partial melodic adapter. Algorithm Bias is on CC76, Feedback is on CC77, Operator Tone is on CC78, FM Level is on CC79, and explicit Algorithm choice is on CC89 with Follow plus OPM algorithms 0-7. The YM2151 UI presents Algorithm as a dropdown plus a four-operator signal-flow preview so the selected or template-resolved `$20` algorithm and feedback bits are readable. All eight YM2151 melodic lanes are exposed through source enable/level controls and Chip Poly allocation; OPM LFO PM/AM, noise mode, timers, CSM behavior, and deep per-operator ADSR controls remain planned.

YM2413 / OPLL preset FM is backed by vendored MIT `emu2413` for a verified-partial preset-FM adapter. All nine melodic OPLL channels are exposed as playable lanes for Chip Poly and Big Mono stacks; channels 1-8 use the shared source CC block and channel 9 uses CC58/59 for enable/level. Instrument Bias is on CC76, Pitch Spread is on CC77, Motion is on CC78, Channel Volume is on CC79, and explicit Instrument choice is on CC89 with Follow plus Bell, Bass, Lead, and Organ musical groups mapped to YM2413 ROM preset instruments. OPLL Rhythm Mode is on CC90 with Follow, Melodic, and Rhythm choices; Rhythm writes native `$0E` rhythm-enable/key bits and maps channels 7-9 into BD, HH, SD, TOM, and CYM through emu2413. The YM2413 UI presents Instrument as a dropdown and Rhythm Mode as a segmented register control so the selected or template-resolved `$30` instrument, volume nibble, and `$0E` rhythm state are readable; deep rhythm-kit editing, VRC7/YMF281 patch sets, and custom Voice 0 editing remain planned.

YM2149 / AY is backed by vendored MIT emu2149 for PSG tone/noise/envelope generation. YM2149 Noise Pitch stays on Native Control 3 / CC78 and maps to register 6. YM2149 Tone/Noise Mix stays on Native Control 4 / CC79 and quantizes to register 7 mixer choices for noise-only, tone-only, or combined tone+noise output. YM2149 Envelope Shape is on CC90 with friendly Fixed/Fall/Rise/Saw/Triangle choices plus exact register-13 codes `0x00` through `0x0F`; the UI uses a dropdown because the full hardware set is too dense for segmented buttons. YM2149 Channel A/B/C Mix live on CC101-103; Follow uses Native Control 4 while Tone, Noise, Both, and Off override the channel's register 7 tone/noise enable bits. SN76489 / Sega PSG is backed by vendored MIT emu76489 for tone/noise generation; Chipper maps UI controls, renderer events, and source gates to PSG latch/register writes, then applies source trims and optional modern stereo spread. SN76489 Noise Bias stays on CC78 and biases the macro-selected PSG noise-control register when Noise Mode is Follow; explicit Noise Mode is on CC91 as a dropdown with Follow, Periodic Low, Periodic High, White Low, and White Tone 3 choices. YM2612 / Genesis FM is backed by vendored BSD-3-Clause ymfm; Algorithm is on CC89, Pan is on CC94 and writes `$B4` L/R output bits, Envelope Shape is on shared CC90 with Follow/Pluck/Lead/Pad/Perc choices that write OPN2 operator attack, decay, sustain-rate, and sustain-level/release registers, DAC Mode is on shared CC91 and can keep channel 6 as FM or enable native `$2B`/`$2A` DAC Drum playback, and Chip Poly now allocates across all six melodic OPN2 channels with all six source lanes exposed through host parameters and MIDI CC. SNES SPC700-style is now an audible partial clean-room mode: eight lo-fi sample voices are modeled with simplified pitch, volume, ADSR/gain, key-on state, generated sample templates, renderer-loaded and plugin-loaded single-file BRR sample decoding, loop/one-shot playback state, partial S-DSP-style noise source selection, clean-room Gaussian-style 4-tap interpolation, and a musical stereo echo helper; all eight source voices are exposed with host/MIDI-controllable enables and trims, Sample Shape is on CC89, Sample Playback is on CC94 as a dropdown with Follow Template, Loop While Held, and One Shot choices, Noise Source is on CC91 with Follow, Off, Low, Mid, and High choices, Pitch is on CC77, Echo Color is on CC78, Voice Volume is on CC79, and Chip Poly allocates notes across all eight exposed voices. Atari POKEY is now an audible partial clean-room mode: four source cards map to AUDF/AUDC/AUDV-style channels, Distortion Code lives on CC89 with Follow, Pure, Poly4, Poly5, and Poly17 choices, Distortion Bias is on CC78, Volume maps to the 4-bit AUDV nibble on CC79, AUDCTL Pairing is on CC94 with Follow, Off, 1+2, 3+4, and Both choices that write the modeled 16-bit channel-link bits, and AUDCTL Filter is on CC90 with Follow, Off, 1<-3, 2<-4, and Both choices that write the modeled high-pass filter bits. The POKEY high-pass path is a clean-room latch approximation where channel 3 can filter channel 1 and channel 4 can filter channel 2; exact latch timing and analog output coupling still need trusted emulator/hardware comparison. Chip Poly can allocate notes across all four channels normally, or across the two paired 16-bit lanes when both AUDCTL pairs are linked. Amiga Paula is now an audible partial clean-room mode: four 8-bit sample channels map to period, 0-64 volume, enable/loop control, fixed L/R/R/L tracker panning, generated sample templates, and plugin-loaded WAV/AIFF file or folder banks; checked folder entries become up to 32 playable sample slots with CC117 manual selection and CC119 note-map playback from the shared Sample Map Root on CC69. Output Filter is on CC91 with Follow, Raw, A500, LED, and LED+A500 choices; Raw keeps the bright nearest-sample DAC path, A500 models the permanent output softening, and LED models the classic power-LED low-pass color. Source cards and descriptor labels show the fixed Paula channel order, Sample Shape is on CC89, Period Motion is on CC77, Loop Bias is on CC78, channel volume is on CC79, and Chip Poly allocates notes across all four channels. PC Engine HuC6280 is now an audible partial clean-room mode: six internal 32-sample wavetable channels are modeled with register-style frequency/control/wave-RAM paths; all six source lanes are visible, host-automatable, and MIDI-controllable with source 5/6 on CC64-67, Wave Shape is on CC89, Noise Bias is on CC78, channel volume is on CC79, and Chip Poly allocates across all six lanes. Namco arcade WSG is now an audible partial clean-room mode: eight simplified 32-sample 4-bit wavetable lanes are modeled with frequency, volume, enable, and wave-RAM register paths; all eight lanes are visible, host-automatable, and MIDI-controllable, with lanes 5/6 on CC64-67 and lanes 7/8 on CC60-63. Wave Shape is on CC89, Wave Skew is on CC78, channel volume is on CC79, and Chip Poly allocates across all eight lanes. Konami SCC is backed by vendored MIT emu2212 as a partial SCC/SCC+ wavetable core: five internal 32-byte wavetable channels map to frequency, volume, key-on, and wave-RAM register writes, all five source lanes are visible, host-automatable, and MIDI-controllable with channel 5 on CC64/66, Wave Shape is on CC89, Wave Skew is on CC78, channel volume is on CC79, and Chip Poly allocates across all five lanes. The plugin footer shows the fixed CC range, and its tooltip lists the full hardware-control map; individual control tooltips also show the relevant CC.

SPC700 sample import supports user-loaded `.brr`, WAV, or AIFF files and folder banks in the plugin UI. BRR files decode through the clean-room BRR block path; WAV/AIFF files import as 8-bit sample memory for the current playable partial core. The first 32 checked readable samples become dropdown slots, the selected slot is recalled by host state as file paths, CC117 selects the manual sample slot in SNES mode, and CC119 / Sample Playback chooses whether played notes use that manual slot or map the checked folder bank from the Sample Map Root (CC69, default C1) upward. Notes outside the visible sample-bank span are silent in map modes instead of clamping to the nearest sample, so a loaded folder behaves like an intentional sample map. Sample bytes are not embedded in the project or committed to the repo, and true WAV-to-BRR conversion remains planned.

The SPC700 Sample Bank button opens a local checklist for folder imports. Users can include/exclude samples, select the first 32, invert, or clear the playable bank; checked entries drive both the dropdown and note map. This is intended to make SPC700-style mode usable as a small SNES sampler instrument while keeping the current mode labeled as a partial clean-room model rather than a verified S-DSP/SPC700 emulator.

SPC700 Noise Source adds a playable partial model of the S-DSP `NON` noise path. Follow uses noise for drum/hit/noise-template sounds, while Off/Low/Mid/High let users force BRR/sample playback or choose a coarse noise clock for hats, snares, and SNES-style hiss. Debug JSON reports the explicit choice, resolved mode, noise clock, and enabled noise-voice mask. Exact S-DSP noise timing, edge behavior, and hardware comparison remain planned.

When an SPC700 sample bank is loaded, the status line and SPC700 voice-card tooltips show the selected bank slot and note-map span so users can see which keys address the loaded sample set without opening hardware-style register details.

SPC700 voice cards also label the loaded sample source as Manual or Map, matching the Sample Playback mode, so users can tell whether notes are replaying the selected slot or browsing the folder bank.

Amiga Paula sample import supports a user-loaded WAV/AIFF file or folder bank in the plugin UI. Chipper converts imported audio into Paula-style 8-bit sample memory for the current partial clean-room core, keeps file paths/check states in plugin state, and does not embed sample bytes in the project. The Paula Bank button opens the same local checklist workflow as DMC/BRR: checked entries define the playable 32-slot dropdown, CC117 selects the manual slot, and CC119 / Sample Playback maps folder slots across MIDI notes from CC69 when Note Map or Map Only is active. Notes outside the visible mapped span are silent instead of clamping. Paula factory presets include raw, LED-filtered, octave, pad, hit, tick, arp, and tracker-bass variants so the mode is immediately playable before loading a user bank. IFF/8SVX/MOD import, tracker loop metadata, exact DMA timing, and hardware validation remain planned.

SPC700 Pitch Motion and note shaping are now audible in the partial core: the SNES Laser, Jump, and Power-Up templates bend rendered sample playback pitch over time, and SPC700-style voices use playable attack/decay/sustain/release behavior instead of hard note-off cuts. Debug JSON reports stored pitch registers, current swept pitch, motion depth/direction, and envelope stage/timing metadata. These are musical clean-room helpers, not verified S-DSP pitch-modulation or exact ADSR/gain timing implementations.

Automated smoke tests assert that every APVTS parameter has exactly one default MIDI CC mapping, that each mapping works through the processor MIDI path, that MIDI and host macro-only changes apply macro templates through the processor path, and that every automatable chip descriptor control resolves to a MIDI-mapped APVTS parameter. The renderer descriptor JSON exports each visible control's APVTS parameter id and default CC plus a top-level `midiCcMappings` list for the full plugin parameter map.

Factory presets can pin source-level balances, DMG Wave Level choices, SID filter-mode combinations, filter routing, per-voice waveforms, and per-voice ADSR nibble choices, so choosing a preset updates the visible controls to match the rendered patch rather than hiding those decisions inside the engine.

The command-line renderer exposes the same NES Pulse 2 Duty override as `--nes-pulse2-duty follow|12.5|25|50|75`, external NES DPCM sample bytes as `--nes-dmc-sample path.dmc`, the same DMC rate index as `--nes-dmc-rate 0..15`, the same SID Filter Mode choices as `--sid-filter-mode follow|lp|bp|hp|off|notch|lp+bp|bp+hp|all` plus exact `$D418` filter-bit values such as `0x50`, the same SID Filter Routing choices as `--sid-filter-routing follow|all|v1|v2|v3|v1+v2|v1+v3|v2+v3|none` plus exact `$D417` low-bit values `0x00..0x07`, the same independent SID pulse-width controls as `--sid-voice2-pulse-width` and `--sid-voice3-pulse-width`, the same POKEY high-pass routing choices as `--pokey-filter follow|off|1<-3|2<-4|both`, the same OPL rhythm choices as `--opl-rhythm follow|melodic|rhythm`, the same YM envelope shape choices as `--ym-envelope-shape fixed|fall|rise|saw|triangle|code0..code15|0x0..0xF`, and the same YM channel mixer overrides as `--ym-channel-a-mix`, `--ym-channel-b-mix`, and `--ym-channel-c-mix`. Debug JSON reports the stored choices, resolved register values, and per-channel tone/noise/filter-routing enables so UI, MIDI, renderer, and engine behavior can be tested against one contract. CLI choice arguments still accept `macro` as a stable script-friendly alias for the UI's Follow choice.

## Command-Line Renderer

The separate renderer is the verification path for chip cores:

```powershell
cmake --build build --config Release --target chipper_render
build\Release\chipper_render.exe --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --output-db -9 --out nes.wav --debug nes.json
```

It accepts chip mode, accuracy, musical macro, native macro controls, output trim, clock rate, note input, register-write event files, render length, and writes WAV plus debug JSON. SID scripts may use `--sid-adsr-speed` as a readable alias for the shared `--envelope-decay` control, plus `--sid-attack follow|0..15`, `--sid-decay follow|0..15`, `--sid-sustain follow|0..15`, and `--sid-release follow|0..15` for exact AD/SR nibble overrides; debug JSON reports the resolved SID AD/SR register bytes, A/D/S/R nibbles, and all three 12-bit pulse-width registers. Factory presets apply the same output-trim snapshot in the renderer that the VST applies in the editor, and debug JSON records `outputDb` plus the resolved linear gain. Golden WAV/reference metadata should be added only from trusted emulator or hardware comparisons.

The renderer can also export the playable factory preset catalog directly:

```powershell
build\Release\chipper_render.exe --list-presets --debug presets.json
build\Release\chipper_render.exe --list-presets --chip sid --debug sid-presets.json
```

The exported JSON is the same preset contract used by the editor and by automated tests. It includes a `summary.totalPresetCount` and per-chip `summary.chipCounts` block so chip-local preset browsers can verify their source catalog before showing users a bank. CI/CTest validates preset metadata and renders every factory preset to assert that each listed patch produces audible output, so a preset should not be treated as shipped until it appears in this catalog and passes the audibility gate.

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
