# Third-Party Notices

Chipper is being developed with an explicit licensing rule: every chip mode must be backed by either clean-room/internal code or a third-party emulation core whose license has been audited before vendoring.

## Current Vendored Emulation Cores

None.

The current partial chip cores are clean-room/internal implementations in `Source/Engine/ChipCore.cpp`:

- NES / RP2A03 partial APU model
- Game Boy / DMG partial APU model
- SID / C64 partial voice/register model
- YM2149 / AY partial PSG model
- SN76489 partial PSG model
- Atari POKEY partial polynomial/timer model
- PC Engine HuC6280 partial wavetable/noise model
- Konami SCC partial wavetable model

These implementations are not claimed to be cycle-accurate. See `docs/emulation-accuracy.md` for current accuracy status and known limitations.

## Candidate Cores Not Yet Vendored

These projects may be evaluated later. Listing them here is not an endorsement that they are already compatible or included.

The project owner is willing to open source Chipper if required for emulator-core licensing. Even so, GPL/LGPL code must not be imported until Chipper's license, source-distribution obligations, dynamic/static linking approach, and commercial distribution plan are explicitly chosen.

The full candidate map is maintained in `docs/emulator-source-map.md`.

Current priority candidates:

- Permissive-first candidates: [ymfm](https://github.com/aaronsgiles/ymfm), [digital-sound-antiques/emu2149](https://github.com/digital-sound-antiques/emu2149), [digital-sound-antiques/emu76489](https://github.com/digital-sound-antiques/emu76489), [digital-sound-antiques/emu2413](https://github.com/digital-sound-antiques/emu2413), [digital-sound-antiques/emu2212](https://github.com/digital-sound-antiques/emu2212), [ayumi](https://github.com/true-grue/ayumi), [SameBoy](https://github.com/LIJI32/SameBoy), [web-pokey](https://github.com/mrk-its/web-pokey), and Paula/ProTracker references. Audit required before vendoring.
- LGPL-sensitive candidates: [FigBug/RP2A03](https://github.com/FigBug/RP2A03), [FigBug/SN76489](https://github.com/FigBug/SN76489), [Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2), [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3), and [Game_Music_Emu](https://github.com/libgme/game-music-emu). Do not import without an LGPL compliance plan.
- GPL/reference candidates: [FigBug/SID](https://github.com/FigBug/SID), [FigBug/PAPU](https://github.com/FigBug/PAPU), [libsidplayfp/reSIDfp](https://github.com/libsidplayfp/libsidplayfp), [Furnace](https://github.com/tildearrow/furnace), [MAME](https://www.mamedev.org/about.html), and GPL-family alternatives. Use as references or validation targets only unless Chipper adopts a compatible distribution model. Furnace was reviewed on 2026-06-06 as a broad chip-coverage, tracker UX, and emulator-core discovery reference; no Furnace code is vendored. Upstream states most Furnace is GPLv2-or-later, ASIO-enabled builds become GPLv3, and individual components may carry their own licenses.
- MAME-specific note: MAME is valuable for hardware behavior and edge cases, but project-level and per-file licensing must be checked before any reuse.

Latest source/license check on 2026-06-07 confirmed the current no-vendored-code posture for the user-suggested sources: FigBug/SID is GPL-3.0, FigBug/PAPU is GPL-2.0, FigBug/RP2A03 and FigBug/SN76489 are LGPL-2.1, Furnace is GPL-family at the aggregate project level with component-level licenses, and ymfm is BSD-3-Clause according to upstream GitHub/repository documentation. These entries are still candidates or references only; no license text from those projects is bundled because no source from them is currently included.

Preset and macro design may be informed by public chip documentation, open-source tracker workflows, and reference instruments listed above, but Chipper's factory presets are authored as Chipper parameter snapshots. Do not import third-party preset banks, songs, modules, patch dumps, samples, lookup tables, or code-derived assets without adding a specific license and rights review here.

## Contribution Rule

When adding any third-party code:

1. Add the exact project name, upstream URL, commit/release, and license here.
2. Include the full license text if required by that license.
3. Document whether the code is vendored, linked, used only as a reference, or used only for tests.
4. Update `docs/emulation-accuracy.md` with the resulting accuracy label and validation status.
