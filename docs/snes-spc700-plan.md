# SNES SPC700 / S-DSP Plan

Chipper's SNES mode should become a hardware-shaped sampler, not another oscillator synth. The musical workflow can stay modern, but the engine should move toward the real SPC700/S-DSP model: eight sample voices, BRR compression, S-DSP envelopes, Gaussian interpolation, pitch modulation, noise, and echo/FIR behavior.

## User Value Target

- Let users build SNES-style patches quickly from folders of samples or BRR assets.
- Keep the first screen musical: voice selection, sample, pitch, loop, envelope, gain, echo send, and output.
- Make hardware limits visible only where they create good musical decisions: eight voices, 64 KB audio RAM budget, BRR blocks, loop points, and shared echo memory.
- Keep the claim honest. Until BRR, S-DSP timing, envelope/gain behavior, interpolation, and echo are verified against trusted references, label this mode as verified partial or inspired/hybrid rather than accurate emulation.

## Native Hardware Model To Expose

### Voices

- Eight monophonic sample voices.
- Per voice: sample source, pitch, volume, pan, ADSR or GAIN mode, echo send, key on/off, loop status.
- Voice linking features to plan: pitch modulation from previous voice, noise substitution, and finite voice allocation.

### Samples

- Primary target: BRR sample import/playback.
- Modern convenience: WAV/AIFF directory import with background conversion preview, but do not imply hardware authenticity until converted to BRR or rendered through a verified BRR decoder.
- UI should show memory budget and loop state. A directory browser with checkboxes fits the current NES DMC direction and should scale to SNES sample banks.
- Done first pass: plugin sample-bank status reports checked-slot payload against a practical 64 KB SNES audio-RAM budget and warns when the bank is near or over that ceiling before driver/echo overhead.

### Interpolation And Output

- Implement S-DSP Gaussian interpolation before claiming an authentic playback path.
- Keep the current clean-room Gaussian-style helper labeled as partial until the exact table/overflow behavior is implemented and tested.
- Done first pass: per-voice left/right volume register state is exported and debugged from Chipper's voice spread model.
- Add per-voice and final-output oscilloscopes once the sample path is more stable; SNES benefits from seeing both raw BRR decode and post-echo output.

### Envelope / Gain

- Expose ADSR per voice, because S-DSP voices have meaningful envelope behavior.
- Also expose GAIN mode for direct, linear, exponential, and bent-line style changes once implemented.
- Keep the UI voice-centric: sample waveform at top, ADSR/GAIN below, echo/noise/pitch-mod toggles at the bottom.

### Echo / FIR

- The SNES echo is a major part of the instrument identity, not a generic reverb.
- User controls should map to S-DSP-like behavior: echo enable per voice, delay, feedback, FIR preset/taps, wet level, and memory budget warning.
- Done first pass: active source lanes now produce an echo-enable mask and explicit echo input level that feed the musical echo helper.
- `snes-echo` is useful as a permissive-looking audible reference, but it explicitly does not target bit accuracy, so it should not be the only validation source.

## Source Candidate Posture

Checked 2026-06-07:

- `osoumen/C700`: strong VST/product reference for SPC/BRR workflows and sample-bank UX. GitHub reports LGPL-2.1, so keep reference-only unless Chipper adopts an LGPL compliance plan.
- `tildearrow/furnace`: excellent tracker workflow and broad chip behavior reference. Aggregate project licensing is GPL-family with component-level licenses, so use as reference/validation only unless Chipper intentionally adopts a compatible distribution model.
- `blarggs-audio-libraries/snes_spc`: high-value SPC700/S-DSP reference and possible oracle. GitHub reports LGPL-2.1, so it needs an LGPL plan before any code is reused.
- `emu-rs/snes-apu`: promising BSD-2-Clause candidate, but its README attributes core SMP work to higan and DSP envelope work to Blargg's `snes_spc`; audit provenance and file headers before reuse.
- `nyanpasu64-backup/snes-echo`: BSD-3-Clause Faust echo/FIR reference. Useful for echo sound design and possible permissive DSP study, but its README says it aims for audible similarity rather than bit accuracy.

## Implementation Milestones

1. **SNES UI Foundation**
   - Replace the generic expansion layout with an eight-voice sample instrument layout.
   - Add voice cards with sample slot, waveform preview, pitch, level, pan, ADSR/GAIN summary, echo send, and active state.
   - Keep only the first four voices visible in compact mode if needed, with a clear 1-8 voice selector/page.

2. **BRR Playback Path**
   - Done first pass: renderer can feed `.brr` bytes or inline BRR hex into the clean-room SPC700 sample voice model.
   - Done first pass: clean-room BRR block decode covers range, filter, end flag, loop flag metadata, and decoded output levels in regression tests.
   - Done first pass: plugin UI can load one user-provided `.brr` file into the same clean-room sample voice path and stores the file path for host-state recall when the file remains on disk.
   - Remaining: BRR loop-address behavior, sample table/addressing, exact S-DSP interpolation table behavior, and comparison against trusted emulator output.

3. **Directory Sample Bank**
   - Load a directory of `.brr` files first, using the renderer BRR decode path as the engine foundation.
   - Add optional WAV/AIFF discovery later, with conversion preview and clear "converted to BRR" status.
   - Add MIDI CC/sample-slot mapping with a finite bank selection model, matching the NES DMC sample-bank pattern.
   - Done first pass: local BRR/WAV/AIFF folders can be staged as checked 32-slot banks, mapped from a root MIDI note, and shown with a practical 64 KB audio-RAM budget cue.

4. **S-DSP Voice Behavior**
   - Add ADSR/GAIN envelope tests.
   - Add pitch register tests and interpolation tests.
   - Add noise and pitch-mod behavior tests.

5. **Echo / FIR**
   - Done first pass: add S-DSP-style echo-enable state and renderer debug JSON for echo send, feedback, delay, echo mask, and echo input level.
   - Add FIR taps and FIR coefficient register debug JSON.
   - Compare against trusted emulator output before upgrading the accuracy label.

6. **Validation**
   - Use C700, Furnace, Blargg `snes_spc`, `emu-rs/snes-apu`, hardware captures, and public SPC/BRR fixtures as comparison targets only after their licensing/provenance is recorded.
   - Store golden WAV/reference metadata with source, license, renderer version, and tolerance notes.

## UI Principle

SNES should structurally differ from NES/SID. NES is generator/register-strip oriented; SID is three deep synth voices plus filter; SNES is an eight-voice sample workstation with hardware memory and echo constraints. Reusing Chipper controls is good, but the layout should adapt to the chip's real instrument shape.
