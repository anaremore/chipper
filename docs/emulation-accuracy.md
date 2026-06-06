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
| NES / RP2A03 | Pulse, triangle, noise, DMC, nonlinear mixer | Clean-room register model; FigBug/RP2A03 may be reference/test-oracle material only | Internal; FigBug/RP2A03 is LGPL-2.1 and not vendored | Partial APU core with pulse/triangle/noise timers, tested pulse duty mapping, tested explicit pulse 2 duty override mapping, tested full `$400E` noise period endpoint mapping, simple envelopes, length-counter groundwork, triangle linear counter gating, 32-step triangle output, DMC direct DAC level, nonlinear mixer, first-order output filter chain, basic pulse sweep update/mute behavior, tested `$4017` frame-counter mode/inhibit behavior, and tested pitched-channel Chip Poly allocation across pulse 1, pulse 2, and triangle; DMC sample playback, exact frame sequencer cycle timing, advanced sweep edge cases, analog tolerance checks, and hardware validation still required |
| Game Boy / DMG APU | Pulse, wave RAM, noise, envelopes, length, sweep, stereo routing | Clean-room register model based on public docs such as Pan Docs, with SameBoy and FigBug/PAPU as possible references after audit | Internal; FigBug/PAPU is GPL-2.0 and not vendored | Partial core with trigger handling, tested pulse duty mapping, DAC gating, simple envelope clocks, register-backed NRx2 initial envelope volume mapping, length-counter groundwork, tested NR10 sweep-shift endpoint mapping, basic CH1 sweep, NR50/NR51 routing, tested user-facing NR51 Stereo Route choices for left, right, and split output, NR52 power/read-only status-bit write behavior, tested NR43 musical noise clock and 15-bit/7-bit width selection, DMG Wave RAM helper shapes, tested NR32 channel-3 output-level choices, and tested pitched-channel Chip Poly allocation across pulse 1, pulse 2, and wave; exact DIV-APU quirks, sweep obscure behavior, mixer analog details, full NR43 edge-case coverage, stereo/DAC edge cases, and hardware validation still required |
| SID / C64 | 6581/8580 oscillator, ADSR, sync, ring, filter | Clean-room voice/register groundwork first; FigBug/SID, reSIDfp, and Furnace may be reference/test-oracle material only | Internal; no SID emulator code is vendored. reSID-style cores and FigBug/SID are GPL-family references only unless Chipper adopts a compatible distribution model | Partial core with three SID oscillator voices, 24-bit frequency-register pitch mapping, tested per-voice waveform control bits for triangle/saw/pulse/noise, 12-bit pulse-width register mapping, tested control-register sync/ring bit mapping for follower voices, gate-driven ADSR approximation using documented time tables, tested ADSR Speed mapping into AD/SR bytes and A/D/S/R nibbles, tested exact per-voice Attack/Decay/Sustain/Release nibble overrides for each SID AD/SR register pair, source trims, cutoff/resonance register writes, tested `$D418` filter-mode choices for LP/BP/HP/bypass, tested 6581/8580 model selector that changes Chipper's partial filter curve and output drive profile, a first-pass state-variable-style multimode output approximation, and tested Chip Poly allocation across voices 1-3; exact 6581/8580 analog filter variants, resonance calibration, per-voice filter routing, simultaneous filter-mode combinations, exact ADSR bugs, exact sync/ring timing and waveform interaction, waveform-combination quirks, external input, DAC nonlinearity, and hardware validation still required |
| YM2149 / AY | Three tones, shared noise, mixer, volume/envelope | Clean-room register model | Internal | Partial core with tone periods, tested shared noise period endpoint mapping through register 6, register 7 mixer choices for noise-only/tone-only/tone+noise, tested per-channel A/B/C tone/noise mixer overrides over register 7, source-aware mixer masks, fixed volume/envelope shape registers, tested datasheet-style logarithmic DAC volume approximation, tested envelope period writes through registers 11/12, YM-style 32-step envelope counter behavior, tested envelope shape-register reset behavior, tested pitched-channel Chip Poly allocation across A/B/C, and optional modern Stereo Spread that defaults to mono; exact noise polynomial timing, exact load-dependent output curve, AY/YM variant differences, and hardware validation still required |
| SN76489 | Tone/noise/attenuation/register behavior | Clean-room register model; FigBug/SN76489 may be reference/test-oracle material only | Internal; FigBug/SN76489 is LGPL-2.1 and not vendored | Partial core with latch/data writes, tested volume-register data-byte updates, tested noise-register data-byte updates and LFSR resets, tone periods including independently tested period 0 and period 1 constant-output edge behavior, attenuation, noise modes, tested macro noise-control bias mapping, tested tone-3-clocked noise register trace, tested noise attenuation register endpoint mapping, tested tone-channel Chip Poly allocation, and optional modern Stereo Spread that defaults to mono; exact variant behavior and hardware-level output validation still required |
| YM2612 / OPN2 | Operators, algorithms, feedback, envelopes, DAC, stereo | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned |
| OPL2/OPL3 | Operators, algorithms, rhythm, waveforms, stereo | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned |
| SNES SPC700-style | Lo-fi sample playback | Needs separate core/sampler design | Audit required | Planned |
| Atari POKEY | Polynomial counters, distortion, timers | Clean-room or permissive emulator core | Audit required | Planned |
| Amiga Paula | Tracker sample channels, DMA-style playback | Clean-room sampler model feasible | Internal possible | Planned |
| PC Engine HuC6280 | Wavetable channels, noise, LFO | Clean-room register model feasible | Internal possible | Planned |
| Namco arcade WSG | Wavetable voices | Clean-room register model feasible | Internal possible | Planned |
| YM2151 / OPM | Four-op FM | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned |
| YM2413 / OPLL | Preset FM | `ymfm` candidate | BSD-3-Clause candidate; audit before vendoring | Planned |
| Konami SCC | Wavetable voices | Clean-room register model feasible | Internal possible | Planned |

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
