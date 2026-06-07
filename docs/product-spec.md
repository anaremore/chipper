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
- Play Mode: Big Mono, Chip Poly where the selected chip has tested finite-channel allocation; Manual and Clone are reserved until tracker routing and hybrid engine cloning are implemented.
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

All current VST parameters have fixed default MIDI CC mappings. Chip-aware controls use the same stable APVTS parameters for UI, host automation, preset recall, and MIDI CC control; register-style choices quantize to legal chip values instead of exposing fake continuous states. Current examples include NES Pulse 2 Duty override bits, NES `$4011` DMC Direct Level, NES DMC Sample Slot for selecting a preloaded `.dmc` bank entry, NES `$4010` DMC Rate for matching DPCM sample clocking, NES `$4010` DMC Loop for one-shot versus looped playback, NES DMC Playback Mode for Manual Slot versus monophonic Note Map bank playback, NES DMC Map Root for choosing the first MIDI note of the sample-bank map, DMG `NR32` Wave Level, DMG `NR51` Stereo Route, independent SID 12-bit pulse-width controls for voices 1-3, SID `$D418` Filter Mode choices including combined LP/BP/HP bit outputs, per-voice SID AD/SR attack/decay/sustain/release nibble overrides, SID voice control-register sync/ring Osc Interaction, SID 6581/8580 Model profile, SID `$D417` Resonance, SID `$D417` voice filter routing, and YM2149 register 7 per-channel A/B/C tone/noise mixer overrides. Inherited register choices should be shown as `Follow`, not `Macro`, so users understand that the patch is following a macro/preset-resolved chip state rather than selecting zero/off/none. Full ADSR controls should appear where the chip model actually owns ADSR or operator envelopes, such as SID and FM chips; APU/PSG chips should expose their native envelope, length, attenuation, or decay controls.

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

Factory preset v1 should prioritize implemented chips and avoid implying that planned chips already have audio cores. The first executable factory presets are:

- NES Hero Pulse
- NES Triangle Bass
- NES Noise Snare
- NES Coin Blip
- NES Boss Damage
- NES Power-Up Rise
- NES Thin Duty Pluck
- NES Duty Arp Runner
- NES Triangle Kick
- NES Menu Confirm
- DMG Wave Bass
- DMG Pocket Arp
- DMG Noise Hat
- DMG Power-Up Rise
- DMG Pulse Lead
- DMG Wave Glass
- DMG Noise Kick
- DMG Split Chord
- SID Dirty Bass
- SID PWM Lead
- SID Robot Arp
- SID Noise Drum
- SID Filter Pluck
- SID 8580 Round Bass
- SID Sync Saw Lead
- SID Ring Bell
- SID Noise Riser
- SID Notch PWM Keys
- SID LP+BP Sweep Stack
- SID BP+HP Metal Bell
- SID All-Mode Growl
- YM Three-Voice Arp
- YM Fake Chord Stack
- YM Menu Beep
- YM Noise Hat
- YM Bright Beep
- YM Envelope Bell
- YM Arcade Clang
- YM Fast Minor Arp
- Sega PSG Lead
- Sega PSG Coin
- Sega PSG Arcade Laser
- PSG Noise Hit
- PSG Tone Stack
- PSG Arcade Bass
- PSG Periodic Zap
- PSG Warning Alarm

Roadmap preset targets, kept as product direction until their chip cores exist:

- Genesis FM Bass
- OPL Bell Lead

## DSP Architecture Outline

- `AudioProcessorValueTreeState` owns all DAW-facing parameters with stable IDs.
- A chip-core interface owns clock rate, register writes, note helper mapping, sample rendering, debug JSON, and accuracy status.
- The VST and command-line renderer use the same chip-core interface.
- Clean-room cores use register-level state where practical.
- Third-party cores must pass a license audit before vendoring.
- The VST maps musical controls to register writes and mode-specific helpers rather than drawing audio from generic oscillators.
- One plugin instance owns one patch. That patch may use all native chip channels. Big Mono stacks channels for one playable sound; Chip Poly allocates notes onto finite chip channels where implemented; Manual and Clone are reserved for tracker-style routing and hybrid cloned-poly behavior.
- Host tempo is read from the playhead. The arp emits note/register events at sample offsets inside the block.
- Post rendering applies only honest modern conveniences: output trim, optional safety limiting/DC cleanup, and labeled hybrid effects.
- State save/restore uses APVTS XML plus an internal register replay snapshot for the active core where practical.

## UI Layout Plan

Single-screen layout:

- **Top bar:** category, preset, randomize, Init Lead, Init Bass, Init Drum, Init SFX, output.
- **Chip:** Chip Mode, Accuracy, chip model variant, clock, and mode-specific musical controls.
- **Register View:** optional debug panel for renderer/developer builds; hidden from normal musical workflow.
- **Mixer:** chip-native channels and levels.
- **Shape:** only chip-authentic controls plus clearly labeled hybrid tone/crunch when enabled.
- **Envelope/Motion:** maps to chip envelopes, sweeps, timers, FM envelopes, or sample playback behavior.
- **SFX:** type, length, pitch movement, brightness, crunch, noise.
- **Visuals:** source cards may show register-resolved waveform/envelope state, while the Output performance cell may show actual post-trim audio from processor telemetry.

Controls use plain names and tooltips. Mode-specific controls remain musically labeled and avoid register names, hex values, and chip-documentation jargon in the main workflow.
