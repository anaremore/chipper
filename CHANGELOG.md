# Changelog

All notable user-facing changes for Chipper are tracked here.

## Unreleased

## 0.2.0

- Redesign all 27 chip editors around their native voices, operators, routing, and shared hardware. Keep source levels visible at both supported widths, show effective inherited duty/routing/multiplier values, and improve FM envelope menus and sample previews.
- Expand the factory library to 401 presets and add a searchable browser with role/engine/tag filters, favorites, user banks, and a 16-sound Featured bank with playing tips.
- Add per-chip Motion Lab patterns with eight sample-accurate pitch, post-chip level, and hold/retrigger/cut steps, host-tempo rates, reusable templates, and native YM2149 noise-period sequencing.
- Add Wave Lab for HuC6280, Namco WSG, and SCC, with live 32-sample drawing, keyboard editing, cross-depth copy/paste, WAV/AIFF import, and project recall.
- Add a shared OPL3 four-operator editor with all four algorithms and native operator fields, plus OPL stereo routing and YM2151 LFO/detune controls.
- Add YM2612 DAC sample import and six-region OPNA/OPNB ADPCM-A bank editing, deterministic sample conversion, and decoded previews.
- Preserve bounded, checksummed sample fallback data in DAW projects so missing external assets can be recalled and relinked. Schema-10 state retains migrations for earlier projects and the stable host parameter IDs.
- Preserve right-routed chip audio in mono VST3 layouts and move sample encoding outside the audio callback lock during project saves.
- Fix undefined signed arithmetic in the vendored OPLL core and add exhaustive conversion coverage under UBSan.
- Split chip cores and project serialization into modules, share compiled plugin test code, and strengthen cross-platform, host-state, real-time, preset, and independent-reference verification.
- Refresh the README gallery for all 27 chips while preserving its existing audio examples. Keep verified release assets unchanged when a completed draft is published.

## 0.1.0

- Initial development version for local VST3 builds and chip-mode verification work.
- Package Windows, macOS, and Linux VST3 zips with SHA256 files, documentation, and legal notices.
- Add the initial README screenshot and click-to-play audio gallery.
