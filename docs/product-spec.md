# Chipper Product Spec

## Concise Product Spec

Chipper is a classic chip synth VST3 focused on iconic console, computer, and arcade sound chips. It should feel authentic enough for chiptune users while staying immediate for modern producers who want a usable lead, bass, arp, drum, blip, laser, coin, jump, or explosion sound in seconds.

The plugin must not use generic retro oscillators with famous chip labels. Each mode is backed by either a compatible-license emulation core or a clean-room register-level model. Modes without register/timing verification are labeled chip-inspired.

- **NES/RP2A03:** two pulse channels, triangle, noise, DMC behavior, nonlinear mixer.
- **Game Boy/DMG:** two pulse channels, wave RAM channel, noise, envelopes, length, sweep.
- **SID/C64:** 6581/8580 variants, three oscillator voices, pulse/saw/triangle/noise, PWM, sync/ring mod, ADSR quirks, filter, resonance, drive.
- **YM2149/AY:** three square tone channels backed by `emu2149`, shared noise, mixer bits, hardware envelope shapes, buzzy arcade/computer character.
- **SN76489:** three tone channels, noise modes, attenuation, Sega PSG behavior. The current partial core is backed by vendored MIT emu76489.
- **YM2612/OPN2:** six-channel FM, operators, algorithms, feedback, LFO, DAC behavior, stereo.
- **OPL2/OPL3:** two-operator FM, rhythm mode, waveform variants, envelopes, stereo for OPL3.
- **Expansion:** SPC700-style sample playback, Paula, HuC6280 wavetable, Namco WSG, YM2151, YM2413/OPLL, Konami SCC. SPC700-style, POKEY, Paula, HuC6280, Namco WSG, YM2413/OPLL, and SCC have started as audible partial expansion cores; YM2413/OPLL is backed by vendored MIT emu2413.

Accuracy levels:

- **Inspired:** musical approximation with smoothed edges.
- **Hybrid:** default chip flavor with modern playability.
- **Authentic:** register/channel/timing behavior closely follows the original chip where implemented.
- **Cycle-accurate:** only allowed after accepted test suites or real hardware captures prove it.

## Parameter List

- Chip Mode: NES/RP2A03, Game Boy/DMG, SID/C64, YM2149/AY, SN76489, YM2612, OPL2/OPL3, SPC700, POKEY, Paula, HuC6280, Namco WSG, YM2151, YM2413/OPLL, SCC
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

All current VST parameters have fixed default MIDI CC mappings. Chip-aware controls use the same stable APVTS parameters for UI, host automation, preset recall, and MIDI CC control; register-style choices quantize to legal chip values instead of exposing fake continuous states. Current examples include NES Pulse 2 Duty override bits, NES `$4011` DMC Direct Level, NES DMC Sample Slot for selecting a preloaded `.dmc` bank entry, NES `$4010` DMC Rate for matching DPCM sample clocking, NES `$4010` DMC Loop for one-shot versus looped playback, NES DMC Playback Mode for Manual Slot, monophonic Note Map bank playback, and VST-side Sample Map Only playback, NES DMC Map Root for choosing the first MIDI note of the sample-bank map, DMG `NR32` Wave Level, DMG `NR51` Stereo Route, independent SID 12-bit pulse-width controls for voices 1-3, SID `$D418` Filter Mode choices including combined LP/BP/HP bit outputs, per-voice SID AD/SR attack/decay/sustain/release nibble overrides, SID voice control-register sync/ring Osc Interaction, SID 6581/8580 Model profile, SID `$D417` Resonance, SID `$D417` voice filter routing, and YM2149 register 7 per-channel A/B/C tone/noise mixer overrides. Inherited register choices should be shown as `Follow`, not `Macro`, so users understand that the patch is following a macro/preset-resolved chip state rather than selecting zero/off/none. Full ADSR controls should appear where the chip model actually owns ADSR or operator envelopes, such as SID and FM chips; APU/PSG chips should expose their native envelope, length, attenuation, or decay controls.

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
- SNES SPC700-style
- Atari POKEY
- Amiga Paula
- PC Engine HuC6280
- Namco WSG
- Konami SCC
- Genesis FM
- DOS FM
- Arcade FM
- Tracker Samples
- Arcade UI
- Classic Game SFX
- Modern Chip Hybrid

## Initial Preset Bank

Factory preset v1 should prioritize implemented chips and avoid implying that planned chips already have audio cores. The first executable factory presets are:

Planned chip modes may expose chip-specific roadmap templates in the Template dropdown, such as OPN2 FM Bass Plan, but those templates are UI/planning metadata only until the chip has an audited or clean-room audio core. They should not be listed as factory presets or described as audible patches.

The authoritative current preset list is the renderer-exported catalog, not this prose list:

```powershell
chipper_render --list-presets --debug presets.json
chipper_render --list-presets --chip sid --debug sid-presets.json
```

Every shipped factory preset must appear in that catalog, include chip/category/note metadata, map to an implemented chip, and pass the automated factory-preset audibility render. New presets should be original Chipper parameter snapshots; do not import third-party preset banks, song patches, module data, DMC samples, or tracker instrument data without a separate license and rights review.

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
- SPC700 Soft Bass
- SPC700 Bell Lead
- SPC700 Voice Arp
- SPC700 Drum Map
- HuC6280 Glass Lead
- HuC6280 Wave Bass
- HuC6280 Six-Wave Arp
- HuC6280 Noise Tap
- Paula Tracker Bass
- Paula Chip Lead
- Paula Tracker Arp
- Paula Noise Tick
- Namco Arcade Lead
- Namco Wave Bass
- Namco Tracker Arp
- Namco Wave Tick
- SCC Arcade Lead
- SCC Wave Bass
- SCC Five-Voice Arp
- SCC Wave Tick
- OPM Arcade Bass
- OPM Metallic Lead
- OPM Arcade Arp
- OPM UI Chime
- OPM Damage Hit
- OPM Laser Sweep
- OPM Power Sweep
- OPM Glass Keys
- OPM Warning Stab
- OPM Hollow Pad
- OPM Noise Zap
- OPLL UI Chime
- OPLL Preset Bass
- OPLL Brass Lead
- OPLL Organ Arp
- OPLL Impact Bell
- OPLL Power Organ
- OPLL Soft Marimba
- OPLL Robot Alert
- OPLL Slow Choir
- OPLL Rhythm Starter
- OPLL Soft Keys

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
- Paula sample banks accept WAV, AIFF, and uncompressed IFF/8SVX files. Compressed 8SVX, MOD sample extraction, loop-point import, and tracker effect metadata remain planned.

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

Mode-specific layouts should adapt structurally, not just change labels. NES and SN76489 can stay generator/register-strip oriented; SID should use three deep voice panels plus a global filter; SNES should become an eight-voice sample-bank workstation with BRR/sample selection, ADSR/GAIN, memory budget, and echo/FIR controls; FM chips should use operator and algorithm views. Reuse Chipper components where possible, but let each chip's native instrument shape determine the layout.
