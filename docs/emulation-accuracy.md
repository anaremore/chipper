# Emulation Accuracy and Licensing

Chipper's simple UX must be backed by real chip behavior wherever possible. This document is the implementation contract for chip modes, accuracy labels, renderer tests, golden references, and licensing.

Source candidates, reference projects, validation targets, and license-audit tiers are tracked in `docs/emulator-source-map.md`. Nothing from that source map is vendored until this document and `THIRD_PARTY_NOTICES.md` are updated with the exact upstream revision and license posture.

## Accuracy Labels

- **Inspired:** musical approximation. The UI may say "NES-inspired" or "SID-inspired"; it must not claim accurate emulation.
- **Hybrid:** chip model plus modern conveniences. This is the default product mode.
- **Authentic:** register/channel/timing behavior closely follows the original chip where implemented and tested.
- **Cycle-accurate:** not exposed unless accepted test suites and/or real hardware captures prove it.

## Current Implementation Status

| Chip | Target | Initial Source Plan | License Status | Current Status |
| --- | --- | --- | --- | --- |
| NES / RP2A03 | Pulse, triangle, noise, DMC, nonlinear mixer | Clean-room register model; FigBug/RP2A03 may be reference/test-oracle material only | Internal; FigBug/RP2A03 is LGPL-2.1 and not vendored | Partial APU core with pulse/triangle/noise timers, tested pulse duty mapping, tested explicit pulse 2 duty override mapping, tested full `$400E` noise period endpoint mapping, simple envelopes, length-counter groundwork, triangle linear counter gating, 32-step triangle output, DMC direct DAC level via `$4011` event traces and a CC116/APVTS direct-level control, external `.dmc` byte stepping from generated renderer fixtures, tested `$4010` DMC rate-index selection with CC118/APVTS control, tested `$4010` DMC loop-bit control with CC68/APVTS and renderer debug state, a VST `.dmc` file/folder bank with checklist curation, CC117 slot selection, CC119 monophonic Note Map playback across checked bank slots plus Sample Map Only as a VST-side performance convenience, and CC69 map-root selection, nonlinear mixer, first-order output filter chain, basic pulse sweep update/mute behavior, tested `$4017` frame-counter mode/inhibit behavior, and tested pitched-channel Chip Poly allocation across pulse 1, pulse 2, and triangle; exact DMC DMA/address/IRQ timing, WAV-to-DMC conversion, exact frame sequencer cycle timing, advanced sweep edge cases, analog tolerance checks, and hardware validation still required |
| Game Boy / DMG APU | Pulse, wave RAM, noise, envelopes, length, sweep, stereo routing | Clean-room register model based on public docs such as Pan Docs, with SameBoy and FigBug/PAPU as possible references after audit | Internal; FigBug/PAPU is GPL-2.0 and not vendored | Partial core with trigger handling, tested pulse duty mapping, DAC gating, simple envelope clocks, register-backed NRx2 initial envelope volume mapping, length-counter groundwork, tested NR10 sweep-shift endpoint mapping, basic CH1 sweep, NR50/NR51 routing, tested user-facing NR51 Stereo Route choices for left, right, and split output, NR52 power/read-only status-bit write behavior, tested NR43 musical noise clock and 15-bit/7-bit width selection, DMG Wave RAM helper shapes, tested NR32 channel-3 output-level choices, and tested pitched-channel Chip Poly allocation across pulse 1, pulse 2, and wave; exact DIV-APU quirks, sweep obscure behavior, mixer analog details, full NR43 edge-case coverage, stereo/DAC edge cases, and hardware validation still required |
| SID / C64 | 6581/8580 oscillator, ADSR, sync, ring, filter | Clean-room voice/register groundwork first; FigBug/SID, reSIDfp, and Furnace may be reference/test-oracle material only | Internal; no SID emulator code is vendored. reSID-style cores and FigBug/SID are GPL-family references only unless Chipper adopts a compatible distribution model | Partial core with three SID oscillator voices, 24-bit frequency-register pitch mapping, tested per-voice waveform control bits for triangle/saw/pulse/noise, tested independent 12-bit pulse-width register mapping for voices 1-3 including 0x000/0xFFF endpoints, tested control-register sync/ring bit mapping for follower voices, gate-driven ADSR approximation using documented time tables, tested ADSR Speed mapping into AD/SR bytes and A/D/S/R nibbles, tested exact per-voice Attack/Decay/Sustain/Release nibble overrides for each SID AD/SR register pair, source trims, cutoff/resonance register writes, tested `$D417` voice filter-routing choices, directly tested `$D418` filter-mode choices for LP/BP/HP/bypass and combined LP+HP notch, LP+BP, BP+HP, and LP+BP+HP outputs, tested 6581/8580 model selector that changes Chipper's partial filter curve and output drive profile, a first-pass state-variable-style multimode output approximation, and tested Chip Poly allocation across voices 1-3; exact 6581/8580 analog filter variants, resonance calibration, hardware-calibrated combined-mode response, exact ADSR bugs, exact sync/ring timing and waveform interaction, waveform-combination quirks, external input, DAC nonlinearity, and hardware validation still required |
| YM2149 / AY | Three tones, shared noise, mixer, volume/envelope | Clean-room register model | Internal | Partial core with tone periods, tested shared noise period endpoint mapping through register 6, register 7 mixer choices for noise-only/tone-only/tone+noise, tested per-channel A/B/C tone/noise mixer overrides over register 7, source-aware mixer masks, fixed volume/envelope mode, tested friendly and exact 16-code envelope shape mapping through register 13, tested datasheet-style logarithmic DAC volume approximation, tested envelope period writes through registers 11/12, YM-style 32-step envelope counter behavior, tested envelope shape-register reset behavior, tested pitched-channel Chip Poly allocation across A/B/C, and optional modern Stereo Spread that defaults to mono; exact noise polynomial timing, exact load-dependent output curve, AY/YM variant differences, and hardware validation still required |
| SN76489 | Tone/noise/attenuation/register behavior | Clean-room register model; FigBug/SN76489 may be reference/test-oracle material only | Internal; FigBug/SN76489 is LGPL-2.1 and not vendored | Partial core with latch/data writes, tested volume-register data-byte updates, tested noise-register data-byte updates and LFSR resets, tone periods including independently tested period 0 and period 1 constant-output edge behavior, attenuation, noise modes, tested macro noise-control bias mapping, tested tone-3-clocked noise register trace, tested noise attenuation register endpoint mapping, tested tone-channel Chip Poly allocation, and optional modern Stereo Spread that defaults to mono; exact variant behavior and hardware-level output validation still required |
| YM2612 / OPN2 | Operators, algorithms, feedback, envelopes, DAC, stereo | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned; descriptor discloses `ymfm`-first source strategy, FM voice/operator UI intent, and missing core/adapter/DAC validation |
| OPL2/OPL3 | Operators, algorithms, rhythm, waveforms, stereo | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned; descriptor discloses `ymfm`-first source strategy, operator-pair/rhythm UI intent, and missing core/operator validation |
| SNES SPC700-style | Lo-fi sample playback | Clean-room partial sample-voice model; emu-rs/snes-apu and snes-echo are permissive-looking audit candidates; C700, blargg snes_spc, Game_Music_Emu, Furnace, and MAME remain reference or audit targets only | Internal; no SPC700, S-DSP, BRR, SNES, or tracker-player source code is vendored. C700 and blargg snes_spc are LGPL-sensitive; Furnace is GPL-family; snes-apu and snes-echo require file-level/provenance audit before any reuse | Partial core with eight generated lo-fi sample voices, simplified pitch/volume/ADSR/gain/key-on state, first-four UI/source exposure, clean-room Gaussian-style 4-tap sample interpolation, musical echo-color helper, tested generated sample-shape choices, tested descriptor/preset/render smoke behavior, and tested Chip Poly allocation across the four exposed voices; BRR decoding, source directory/sample memory addressing, S-DSP noise, pitch modulation, exact S-DSP Gaussian interpolation table behavior, FIR echo, exact ADSR/gain envelope timing, SPC700 CPU timing, output filtering, and hardware validation still required |
| Atari POKEY | Polynomial counters, distortion, timers | Clean-room partial model; web-pokey remains a permissive candidate/reference after audit | Internal; no POKEY emulator code is vendored | Partial core with four AUDF/AUDC/AUDV-style channels, note-to-timer writes, source gating, source level trims, tested Pure/Poly4/Poly5/Poly17 distortion choices, tested 4-bit AUDV volume nibble mapping, tested four-channel Chip Poly allocation, and optional modern Stereo Spread that defaults to mono; AUDCTL channel pairing, 1.79 MHz direct clock modes, high-pass filters, exact polynomial taps/timer edge behavior, DAC curve, Atari model differences, and hardware validation still required |
| Amiga Paula | Tracker sample channels, DMA-style playback | Clean-room partial sampler model; pt2-clone/libxmp/Furnace remain reference or audit targets only | Internal; no Paula, ProTracker, or module-player code is vendored | Partial core with four 8-bit generated sample channels, note-to-period mapping, 0-64 volume registers, enable/loop control bits, classic hard-panned channel layout with optional modern spread, source gating, source level trims, tested generated sample-shape choices, tested four-channel Chip Poly allocation, and renderer/debug JSON coverage; DMA pointer reload timing, exact sample memory addressing, interrupts, CIA/video timing, ProTracker effect commands, external IFF/8SVX/MOD sample import, analog output filtering, and hardware validation still required |
| PC Engine HuC6280 | Wavetable channels, noise, LFO | Clean-room register model | Internal | Partial core with six internal wavetable channels, channel-select/frequency/control/balance/wave-RAM/noise-control register paths, generated 32-sample 5-bit wave RAM templates, simplified upper-channel LFSR noise, first-four UI/source exposure, stack templates that can use channels 5-6 internally, tested descriptor/preset/render smoke behavior, and tested Chip Poly allocation across the four exposed channels; native LFO, exact DDA behavior, exact noise taps, stereo register routing, timer edge timing, DAC/output hardware validation, and a dedicated six-lane HuC6280 UI still required |
| Namco arcade WSG | Wavetable voices | Clean-room register model; MAME/Furnace references remain audit targets only | Internal; no Namco WSG emulator code is vendored | Partial core with eight simplified wavetable lanes, 32-sample 4-bit waveform RAM templates, frequency/volume/enable register paths, first-four UI/source exposure, stack templates that can use lanes 5-8 internally, tested descriptor/preset/render smoke behavior, and tested Chip Poly allocation across the four exposed lanes; exact Namco custom-chip variants, voice count differences, waveform addressing, DAC/output curve, timer edge timing, and hardware validation still required |
| YM2151 / OPM | Four-op FM | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned; descriptor discloses `ymfm`-first source strategy, four-operator UI intent, and missing OPM/LFO validation |
| YM2413 / OPLL | Preset FM | `ymfm` or `emu2413` candidate | BSD-3-Clause/MIT candidates; audit before vendoring | Planned; descriptor discloses permissive candidate strategy, preset-FM UI intent, and missing instrument/rhythm/patch validation |
| Konami SCC | Wavetable voices | Clean-room register model; `emu2212` remains a permissive candidate/reference after audit | Internal; no SCC emulator code is vendored | Partial core with five internal wavetable channels, 32-byte waveform RAM templates, frequency/volume/key-on register paths, first-four UI/source exposure, stack templates that can use channel 5 internally, tested descriptor/preset/render smoke behavior, and tested Chip Poly allocation across the four exposed channels; SCC+ bank behavior, channel 4/5 wave-memory quirks, exact DAC/output curve, timer edge timing, and hardware validation still required |

## Renderer Requirements

The command-line renderer is the truth source for core validation. It accepts:

- chip mode
- accuracy mode
- chip clock rate
- host sample rate
- render length
- register-write events
- note helper events

It outputs:

- WAV audio
- debug JSON with mode, clock, sample rate, output trim, event count, peak/RMS, per-channel stereo peak/RMS, zero crossings, implemented accuracy, and known limitations

## Regression Tests

Automated tests must cover:

- pitch and timer periods
- envelopes and length counters
- noise LFSR/polynomial behavior
- duty cycles
- mixer/output levels
- filter response where applicable
- DAC/sample behavior where applicable
- edge cases such as zero periods, max attenuation, register writes during active playback, and state reset

Golden WAV/reference outputs must include versioned metadata and source provenance. Trusted references may come from accepted emulator test suites, public hardware captures, or recordings produced from documented hardware test patches.

## Product Honesty Rules

- Do not expose **Cycle-accurate** unless verified.
- Do not call a mode accurate if it is only musically similar.
- Do not vendor GPL/LGPL code into the plugin without an explicit distribution decision.
- If a mode is planned but not implemented, the UI/docs must say so.
- The project owner is willing to open source Chipper if needed for emulator-core licensing, but the exact Chipper license and distribution model must be chosen before importing GPL/LGPL code.

## Public Documentation References

- NES / RP2A03 APU sweep, triangle, DMC, frame counter, and mixer behavior: https://www.nesdev.org/wiki/APU_Sweep, https://www.nesdev.org/wiki/APU_Triangle, https://www.nesdev.org/wiki/APU_DMC, https://www.nesdev.org/wiki/APU_Frame_Counter, and https://www.nesdev.org/wiki/APU_Mixer
- Game Boy / DMG APU register and timing behavior: https://gbdev.io/pandocs/Audio_Registers.html and https://gbdev.io/pandocs/Audio_details.html
- MOS 6581 SID register layout, waveform control bits, pulse-width registers, ADSR nibbles, and filter registers: https://www.cpcwiki.eu/imgs/9/9d/Mos_6581_sid.pdf
- AY-3-8910 envelope period, 16-state shape counter, amplitude mode, and logarithmic DAC behavior: https://computers.baffa.tec.br/pages/datasheet/AY-3-8910-data_manual.pdf
- YM2149 mixer, 5-bit envelope output, 32-count envelope pattern, envelope period, and shape-register reset behavior: https://temlib.org/AtariForumWiki/index.php/Yamaha_YM2149_Manual
- SN76489 PSG register, latch/data, noise, and attenuation behavior: https://www.smspower.org/Development/SN76489
- SNES SPC700-style behavior is currently implemented as a clean-room partial sample-voice model from public S-DSP/SPC700 documentation and cross-tracker behavior notes without vendored third-party source. The current source-map tracks C700, Furnace, blargg snes_spc, emu-rs/snes-apu, and snes-echo as references/candidates with distinct license risks; exact upstream commits, file headers, transitive provenance, and license obligations must be recorded before any external SPC700, BRR, S-DSP, Gaussian interpolation, or FIR echo code is imported.
- Atari POKEY register layout, audio frequency/control/volume concepts, and AUDCTL behavior: https://www.atariarchives.org/dere/chapt08.php
- Amiga Paula behavior is currently implemented as a clean-room partial tracker sampler from public period/volume/channel-layout documentation and cross-tracker behavior notes without vendored third-party source; exact upstream reference commits and licenses must be added to `docs/emulator-source-map.md` before any external Paula or ProTracker code is imported.
- PC Engine / TurboGrafx-16 HuC6280 PSG behavior is currently implemented from public register-level documentation and cross-emulator behavior notes without vendored third-party source; exact upstream reference commits and licenses must be added to `docs/emulator-source-map.md` before any external HuC6280 code is imported.
- Namco arcade WSG behavior is currently implemented from public register-level documentation and cross-emulator behavior notes without vendored third-party source; exact upstream reference commits and licenses must be added to `docs/emulator-source-map.md` before any external Namco WSG code is imported.
- Konami SCC wavetable behavior is currently implemented from public register-level documentation and cross-emulator behavior notes without vendored third-party source; exact upstream reference commits and licenses must be added to `docs/emulator-source-map.md` before any external SCC code is imported.
