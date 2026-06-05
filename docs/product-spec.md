# Chipper Product Spec

## Concise Product Spec

Chipper is a classic chip synth VST3 focused on iconic console, computer, and arcade sound chips. It should feel authentic enough for chiptune users while staying immediate for modern producers who want a usable lead, bass, arp, drum, blip, laser, coin, jump, or explosion sound in seconds.

The plugin must not use generic retro oscillators with famous chip labels. Each mode is backed by either a compatible-license emulation core or a clean-room register-level model. Modes without register/timing verification are labeled chip-inspired.

- **NES/RP2A03:** two pulse channels, triangle, noise, DMC behavior, nonlinear mixer.
- **Game Boy/DMG:** two pulse channels, wave RAM channel, noise, envelopes, length, sweep.
- **SID/C64:** 6581/8580 variants, three oscillator voices, pulse/saw/triangle/noise, PWM, sync/ring mod, ADSR quirks, filter, resonance, drive.
- **YM2149/AY:** three square tone channels, shared noise, mixer bits, hardware envelope shapes, buzzy arcade/computer character.
- **SN76489:** three tone channels, noise modes, attenuation, Sega PSG behavior.
- **YM2612/OPN2:** six-channel FM, operators, algorithms, feedback, LFO, DAC behavior, stereo.
- **OPL2/OPL3:** two-operator FM, rhythm mode, waveform variants, envelopes, stereo for OPL3.
- **Expansion:** SPC700-style sample playback, POKEY, Paula, HuC6280 wavetable, Namco WSG, YM2151, YM2413/OPLL, Konami SCC.

Accuracy levels:

- **Inspired:** musical approximation with smoothed edges.
- **Hybrid:** default chip flavor with modern playability.
- **Authentic:** register/channel/timing behavior closely follows the original chip where implemented.
- **Cycle-accurate:** only allowed after accepted test suites or real hardware captures prove it.

## Parameter List

- Chip Mode: NES/RP2A03, Game Boy/DMG, SID/C64, YM2149/AY, SN76489, YM2612, OPL2/OPL3, SPC700, POKEY, Paula, HuC6280, Namco WSG, YM2151, YM2413/OPLL, SCC, Arcade, Custom
- Accuracy: Inspired, Hybrid, Authentic
- Output Level
- Musical Macro: Coin, Bass, Lead, Arp, Drum, Hit, Laser, Jump, Power-Up
- Clock Rate override and model variant where applicable
- Register-backed channel controls for each implemented chip
- Duty, noise mode, sweep, length/envelope controls for pulse/noise chips
- Operator, algorithm, feedback, LFO, DAC, and stereo controls for FM chips
- Sample slot, rate, loop, and bit-depth controls for sampler chips
- Wavetable index, wave RAM/SCC waveform, and channel mix controls for wavetable chips
- Arp, retrigger, pitch motion, vibrato, glide, output trim
- Debug/validation export controls in the renderer, not the VST audio thread

## Preset Categories

- NES Leads
- NES Bass
- NES Drums
- DMG Pulse
- DMG Wave
- SID Bass
- SID PWM Leads
- SID FX
- YM Beeps
- YM Arps
- SN76489 / Sega PSG
- Genesis FM
- DOS FM
- Arcade FM
- Tracker Samples
- Arcade UI
- Classic Game SFX
- Modern Chip Hybrid

## Initial Preset Bank

- NES Hero Pulse
- NES Triangle Bass
- NES Noise Snare
- NES Coin Blip
- DMG Wave Bass
- DMG Noise Hat
- SID Dirty Bass
- SID PWM Lead
- SID Robot Arp
- SID Filter Sweep
- YM Three-Voice Arp
- YM Menu Beep
- YM Noise Hat
- Sega PSG Lead
- Genesis FM Bass
- OPL Bell Lead
- Arcade Laser
- Fake Chord Stack
- Boss Damage
- Power-Up Rise

## DSP Architecture Outline

- `AudioProcessorValueTreeState` owns all DAW-facing parameters with stable IDs.
- A chip-core interface owns clock rate, register writes, note helper mapping, sample rendering, debug JSON, and accuracy status.
- The VST and command-line renderer use the same chip-core interface.
- Clean-room cores use register-level state where practical.
- Third-party cores must pass a license audit before vendoring.
- The VST maps musical controls to register writes and mode-specific helpers rather than drawing audio from generic oscillators.
- Host tempo is read from the playhead. The arp emits note/register events at sample offsets inside the block.
- Post rendering applies only honest modern conveniences: output trim, optional safety limiting/DC cleanup, and labeled hybrid effects.
- State save/restore uses APVTS XML.

## UI Layout Plan

Single-screen layout:

- **Top bar:** category, preset, randomize, Init Lead, Init Bass, Init Drum, Init SFX, output.
- **Chip:** Chip Mode, Accuracy, chip model variant, clock, and mode-specific musical controls.
- **Register View:** optional debug panel for renderer/developer builds; hidden from normal musical workflow.
- **Mixer:** chip-native channels and levels.
- **Shape:** only chip-authentic controls plus clearly labeled hybrid tone/crunch when enabled.
- **Envelope/Motion:** maps to chip envelopes, sweeps, timers, FM envelopes, or sample playback behavior.
- **SFX:** type, length, pitch movement, brightness, crunch, noise.

Controls use plain names and tooltips. Mode-specific controls remain musically labeled and avoid register names, hex values, and chip-documentation jargon in the main workflow.
