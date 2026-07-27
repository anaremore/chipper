# Changelog

All notable user-facing changes for Chipper are tracked here.

## Unreleased

- Harden GitHub release packaging with platform VST3 zips, SHA256 checksum files, lean documentation payloads, and draft tag-created releases.
- Add README screenshots and click-to-play MP3 examples for all current chip modes.
- Add a global per-chip Motion Lab with eight sample-accurate pitch, post-chip level, and hold/retrigger/cut steps; host-tempo rates from 1/8 through 1/64; reusable templates; explicit Big Mono/Chip Poly behavior; and schema-v4 project recall.
- Add an inline native Wave Lab for HuC6280, Namco WSG, and SCC with live 32-sample drawing, keyboard editing, cross-depth copy/paste, WAV/AIFF import, generated-wave reset, and exact project recall.
- Add a shared OPL3 four-operator editor with all four YMF262 algorithms, explicit carrier/modulator roles, native multiplier/total-level/AR/DR/SL/RR controls, linked `$C0` register writes, exact renderer assertions, and two-width visual regression coverage.

## 0.1.0

- Initial development version for local VST3 builds and chip-mode verification work.
