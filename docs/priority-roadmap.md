# Chipper Priority Roadmap

This list ranks near-term work by user value, implementation effort, and confidence. Value and confidence are scored 1-10, where 10 is highest. Effort is scored 1-10, where 10 is largest. "Confidence improves with" names the evidence or preparation that would make the work safer to execute.

| Rank | Todo | User Value | Effort | Confidence | Confidence Improves With |
| --- | --- | ---: | ---: | ---: | --- |
| 1 | Make implemented chip UIs read as real instruments while preserving chip-native controls. Start with clearer SID voice cards, per-voice ADSR readability, filter-first SID tone controls, and legible output monitoring. | 10 | 5 | 8 | Visual checks in the standalone/VST host, reusable source-card layout helpers, and descriptor screenshots for each chip. |
| 2 | Expand the verification harness so every implemented chip has pitch, envelope, noise, mixer, source-enable, MIDI CC, preset, and debug JSON assertions tied to descriptor metadata. | 10 | 6 | 8 | More compact assertion helpers and reference metadata exported from `chipper_render --list-descriptors`. |
| 3 | Improve SID authenticity where it most affects musicians: per-voice ADSR edge cases, sync/ring interactions, filter routing, 6581/8580 response differences, and output drive. | 10 | 8 | 6 | Trusted SID references, hardware captures or accepted emulator comparisons, and narrow renderer traces for each behavior. |
| 4 | Add a reusable register-control component system so discrete chip fields never appear as generic smooth sliders. | 9 | 6 | 8 | A stronger `ChipParameterSpec` layout schema and UI smoke checks for legal choices per chip. |
| 5 | Make factory presets more delightful and source-informed for implemented chips, with visible controls matching every preset snapshot. | 9 | 4 | 9 | Preset audition passes, renderer loudness checks, and descriptor JSON checks for pinned controls. |
| 6 | Improve NES/RP2A03 authenticity around DMC, frame sequencer timing, sweep edge cases, nonlinear mixer tolerances, and test trace coverage. | 9 | 7 | 7 | Test ROM references, emulator comparison renders, and golden metadata for existing clean-room behavior. |
| 7 | Improve DMG authenticity around wave RAM writes, length/envelope timing, sweep overflow behavior, NR52 power behavior, and stereo routing. | 8 | 7 | 7 | Pan Docs-style public references, focused register traces, and stronger edge-case renderer tests. |
| 8 | Improve YM2149/AY and SN76489 usability with chip-specific mixer/noise panels, preset categories, and clearer channel source cards. | 8 | 5 | 8 | Shared source-card component improvements and current renderer JSON for mixer/noise state. |
| 9 | Add an explicit UI/renderer accuracy badge per chip mode that distinguishes Inspired, Hybrid, Authentic, and unverified roadmap modes without overclaiming. | 8 | 4 | 9 | A single descriptor field for verified behaviors and docs reviewed against current test coverage. |
| 10 | Audit compatible emulator cores and license paths for planned FM/sample/wavetable chips before any vendoring. | 8 | 6 | 6 | License matrix, provenance notes, and source-map updates for each candidate core. |
| 11 | Implement the next chip core only after a scoped accuracy plan exists; likely OPL2/OPL3 or YM2612 using a compatible high-quality FM core. | 8 | 9 | 5 | Completed license audit, a thin adapter spike, and renderer golden tests before VST UI exposure. |
| 12 | Add hardware/reference comparison documentation and tolerance thresholds for each implemented chip. | 7 | 8 | 5 | Real hardware recordings, reproducible capture settings, and agreed spectral/timing tolerances. |

## Current Execution Rule

Each completed development slice should:

- Improve either emulation truth, musical usability, verification, presets, documentation, or installability.
- Keep DAW parameter IDs and MIDI CC mappings stable unless a migration plan exists.
- Build `Chipper_VST3`, run the relevant tests, install the VST3, commit, and push.
- Avoid product claims stronger than the current renderer tests and documented verification support.
