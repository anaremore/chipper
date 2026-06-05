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
| NES / RP2A03 | Pulse, triangle, noise, DMC, nonlinear mixer | Clean-room register model; FigBug/RP2A03 may be reference/test-oracle material only | Internal; FigBug/RP2A03 is LGPL-2.1 and not vendored | Partial APU core with pulse/triangle/noise timers, simple envelopes, length-counter groundwork, triangle linear counter gating, 32-step triangle output, DMC direct DAC level, nonlinear mixer, first-order output filter chain, and basic pulse sweep update/mute behavior; DMC sample playback, exact frame sequencing, advanced sweep edge cases, analog tolerance checks, and hardware validation still required |
| Game Boy / DMG APU | Pulse, wave RAM, noise, envelopes, length, sweep, stereo routing | Clean-room register model based on public docs such as Pan Docs, with SameBoy and FigBug/PAPU as possible references after audit | Internal; FigBug/PAPU is GPL-2.0 and not vendored | Partial core with trigger handling, DAC gating, simple envelope clocks, length-counter groundwork, basic CH1 sweep, NR50/NR51 routing, and NR43 noise clock behavior; exact DIV-APU quirks, sweep obscure behavior, mixer analog details, stereo/DAC edge cases, and hardware validation still required |
| SID / C64 | 6581/8580 oscillator, ADSR, sync, ring, filter | Needs permissive core or clean-room implementation; FigBug/SID may be reference material only | reSID-style cores are commonly GPL-family; FigBug/SID is GPL-3.0 and not vendored | Planned, not accurate |
| YM2149 / AY | Three tones, shared noise, mixer, volume/envelope | Clean-room register model | Internal | Phase 1 partial core |
| SN76489 | Tone/noise/attenuation/register behavior | Clean-room register model; FigBug/SN76489 may be reference/test-oracle material only | Internal; FigBug/SN76489 is LGPL-2.1 and not vendored | Partial core with latch/data writes, tone periods, attenuation, noise modes, and tested tone-channel Chip Poly allocation; exact variant behavior and hardware-level output validation still required |
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
- debug JSON with mode, clock, sample rate, event count, peak/RMS, per-channel stereo peak/RMS, zero crossings, implemented accuracy, and known limitations

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
