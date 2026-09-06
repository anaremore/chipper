# Changelog

All notable user-facing changes for Chipper are tracked here.

## Unreleased

- Preserve right-routed chip audio in mono VST3 layouts with equal-weight downmixing.
- Move sample encoding outside the audio callback lock during project saves.
- Add native YM2149 noise-period Motion with schema-10 recall and unchanged host parameter IDs.
- Add a 16-sound Featured browser bank, playing tips, longer note/velocity audition gates, and clearer header/FM text.
- Split chip cores and project serialization into modules, share compiled plugin test code, and require complete Python-backed CI verification.

- Harden GitHub release packaging with platform VST3 zips, SHA256 checksum files, lean documentation payloads, and draft tag-created releases.
- Add README screenshots and click-to-play MP3 examples for all current chip modes.
- Add a global per-chip Motion Lab with eight sample-accurate pitch, post-chip level, and hold/retrigger/cut steps; host-tempo rates from 1/8 through 1/64; reusable templates; explicit Big Mono/Chip Poly behavior; and schema-v4 project recall.
- Add an inline native Wave Lab for HuC6280, Namco WSG, and SCC with live 32-sample drawing, keyboard editing, cross-depth copy/paste, WAV/AIFF import, generated-wave reset, and exact project recall.
- Add a shared OPL3 four-operator editor with all four YMF262 algorithms, explicit carrier/modulator roles, native multiplier/total-level/AR/DR/SL/RR controls, linked `$C0` register writes, exact renderer assertions, and two-width visual regression coverage.

## 0.1.0

- Initial development version for local VST3 builds and chip-mode verification work.
