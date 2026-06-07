# Emulator Source Candidate Map

This document captures emulator projects shared as possible Chipper source material. These entries are candidates, references, or validation targets only. No code from these projects is currently vendored into Chipper.

Every project listed here still needs a file-level license audit before any code is imported, linked, translated, or bundled. "Reference" means behavior comparison, UX comparison, rendered-output comparison, or documentation research. It does not mean copying source code.

## Licensing Posture

- Prefer permissive sources first: MIT, BSD-3-Clause, Apache-style, zlib, or similarly compatible licenses.
- Treat LGPL sources as license-sensitive. Decide dynamic/static linking, source distribution, relinkability, attribution, and commercial distribution before importing.
- Treat GPL-family sources as reference or validation targets unless Chipper intentionally adopts a GPL-compatible distribution model.
- Treat MAME as a special case. MAME is extremely useful for documentation and edge cases; the project as a whole is GPL-2.0+, while many individual files are BSD-3-Clause. Audit each file header before reuse.
- Keep the UI honest. A mode is "inspired" until register/timing behavior is implemented and verified.

## Open-Source Path

The owner is willing to make Chipper open source if that is the right product and licensing move. That improves the set of viable emulator-core options, but it does not remove the need for deliberate source hygiene.

Before importing GPL-family or LGPL code, choose Chipper's project license and distribution model, then record the exact obligations in `THIRD_PARTY_NOTICES.md`. Open-source compatibility must be checked at the file level, not only at the repository level, because emulator projects often combine cores, tools, lookup tables, optional backends, generated data, and third-party dependencies under different terms.

Until that decision is made, use GPL-family projects such as Furnace, reSIDfp, FigBug/SID, FigBug/PAPU, MAME aggregate sources, and similar trackers/emulators as references and validation targets only. Preset design may learn from public chip workflows, but Chipper presets must remain original Chipper parameter snapshots unless a separate rights review says otherwise.

## Latest Source Check

Checked 2026-06-07 from upstream GitHub pages/license files:

- FigBug/SID is a C64 SID VST/AU reference and GitHub reports GPL-3.0. Keep reference-only unless Chipper adopts a compatible distribution model.
- FigBug/RP2A03 is an NES RP2A03 VST/AU reference and GitHub reports LGPL-2.1. Treat as license-sensitive comparison material until an LGPL compliance path is chosen.
- FigBug/PAPU is a Game Boy PAPU VST/AU reference and GitHub reports GPL-2.0. Keep reference-only unless Chipper adopts a compatible distribution model.
- FigBug/SN76489 is an SN76489 VST/AU reference and GitHub reports LGPL-2.1. Treat as license-sensitive comparison material until an LGPL compliance path is chosen.
- Furnace's top-level license file says most Furnace is GPLv2-or-later, ASIO-enabled builds become GPLv3, and included components may carry their own licenses. Use Furnace as a tracker/product/reference index, not as a source shortcut.
- ymfm's repository describes BSD-licensed Yamaha FM cores and GitHub reports BSD-3-Clause, making it the first FM-family candidate to audit for YM2612, OPL2/OPL3, YM2151, and YM2413.

## Preferred Implementation Tiers

### Tier 1: Permissive-First Candidates

These should be evaluated first because their upstream license posture appears most compatible with broad VST distribution, subject to file-level audit.

| Project | Candidate Use | Upstream License Posture |
| --- | --- | --- |
| [ymfm](https://github.com/aaronsgiles/ymfm) | YM2149, YM2151/OPM, YM2612/OPN2, OPL2/OPL3, YM2413/OPLL | GitHub reports BSD-3-Clause |
| [digital-sound-antiques/emu2149](https://github.com/digital-sound-antiques/emu2149) | YM2149 / AY PSG | GitHub reports MIT |
| [digital-sound-antiques/emu76489](https://github.com/digital-sound-antiques/emu76489) | SN76489 / Sega PSG | GitHub reports MIT |
| [digital-sound-antiques/emu2413](https://github.com/digital-sound-antiques/emu2413) | YM2413 / OPLL | GitHub reports MIT |
| [digital-sound-antiques/emu2212](https://github.com/digital-sound-antiques/emu2212) | Konami SCC | GitHub reports MIT |
| [ayumi](https://github.com/true-grue/ayumi) | AY-3-8910 / YM2149 | GitHub reports MIT |
| [SameBoy](https://github.com/LIJI32/SameBoy) | Game Boy / DMG reference or possible extraction | Upstream reports an Expat-style license, with folder-specific exceptions to audit |
| [web-pokey](https://github.com/mrk-its/web-pokey) | Atari POKEY behavior reference or port candidate | GitHub reports MIT |
| [pt2-clone](https://github.com/8bitbubsy/pt2-clone) | Amiga Paula / ProTracker behavior reference | Candidate; audit before reuse |

### Tier 2: Strong But License-Sensitive

These may be excellent accuracy choices, but require an explicit LGPL compliance plan before any code reuse.

| Project | Candidate Use | Upstream License Posture |
| --- | --- | --- |
| [Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2) | High-accuracy YM3438/YM2612 | GitHub reports LGPL-2.1 |
| [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3) | High-accuracy YMF262/OPL3 | GitHub reports LGPL-2.1 |
| [FigBug/RP2A03](https://github.com/FigBug/RP2A03) | NES/RP2A03 VST reference and test-oracle candidate | GitHub reports LGPL-2.1 |
| [FigBug/SN76489](https://github.com/FigBug/SN76489) | SN76489 VST reference and test-oracle candidate | GitHub reports LGPL-2.1 |
| [Game_Music_Emu](https://github.com/libgme/game-music-emu) | Playback/reference for many console music formats and chips | GitHub reports LGPL-2.1; upstream notes GPL implications for some optional MAME YM2612 usage |

### Tier 3: Reference / Validation Only Unless Chipper Goes GPL-Compatible

These are musically and technically valuable, but direct embedding should wait until the project license and distribution model are intentionally chosen.

| Project | Reference Use | Upstream License Posture |
| --- | --- | --- |
| [Furnace](https://github.com/tildearrow/furnace) | Musical completeness, tracker UX, supported-chip behavior, test material, and broad chip-coverage comparison | Upstream `LICENSE` says most Furnace is GPLv2-or-later and ASIO-enabled builds become GPLv3; reference only unless Chipper intentionally adopts a compatible GPL distribution model and audits file-level third-party cores |
| [MAME](https://www.mamedev.org/about.html) | Hardware behavior, edge cases, file-level docs, possible per-file reuse | Project as a whole is GPL-2.0+; many files BSD-3-Clause |
| [FigBug/SID](https://github.com/FigBug/SID) | SID/C64 VST reference | GitHub reports GPL-3.0 |
| [FigBug/PAPU](https://github.com/FigBug/PAPU) | Game Boy PAPU VST reference | GitHub reports GPL-2.0 |
| [libsidplayfp / reSIDfp](https://github.com/libsidplayfp/libsidplayfp) | SID behavior and C64/SID validation target | GitHub reports GPL-2.0 |

## Per-Chip Candidate Map

| Chipper Mode | First Candidates | Reference / Validation Targets | Current Recommendation |
| --- | --- | --- | --- |
| NES / RP2A03 | Clean-room register model; possible LGPL path later | FigBug/RP2A03, Game_Music_Emu, Furnace | Continue clean-room APU work first; use FigBug/GME as comparison targets, not copied code |
| Game Boy / DMG | Clean-room model; SameBoy as audited reference/extraction candidate | SameBoy, FigBug/PAPU, Game_Music_Emu, Furnace | Continue Pan Docs-backed clean-room work; compare tracker-facing controls and behavior against Furnace/SameBoy, but evaluate SameBoy file-level license before extraction and keep Furnace reference-only unless Chipper goes GPL-compatible |
| SID / C64 | Clean-room voice/register model first | libsidplayfp/reSIDfp, FigBug/SID, Furnace, MOS 6581 datasheet | Continue internal partial SID voice work; keep GPL SID projects reference-only until Chipper chooses a GPL-compatible path or a permissive SID core is selected |
| YM2149 / AY | emu2149 or ayumi; ymfm if useful | Game_Music_Emu, Furnace, MAME | Evaluate emu2149/ayumi first; keep Chipper's current clean-room PSG as baseline |
| SN76489 / Sega PSG | emu76489 | FigBug/SN76489, Game_Music_Emu, Furnace | Evaluate emu76489 first; use FigBug as LGPL comparison target |
| YM2612 / Genesis FM | ymfm | Nuked-OPN2, Game_Music_Emu, Furnace | Use ymfm first for compatibility; consider Nuked-OPN2 only with an LGPL plan |
| OPL2 / OPL3 / DOS FM | ymfm | Nuked-OPL3, Furnace | Use ymfm first; reserve Nuked-OPL3 for optional stricter mode if licensing is settled |
| SNES SPC700-style | Sample-engine design; Game_Music_Emu as reference | Furnace, MAME | Treat as sample playback/tracker mode, not an oscillator mode |
| Atari POKEY | Current clean-room partial model; web-pokey remains a future audit candidate | Game_Music_Emu, MAME, Furnace | Continue clean-room POKEY work for now; audit web-pokey and MAME file headers before any reuse |
| Amiga Paula | Clean-room tracker sampler; pt2-clone reference | Paula Tracker / PaulaLib, libxmp, Furnace | Start with clean-room sampler behavior and compare against tracker references |
| PC Engine HuC6280 | Clean-room wavetable model | MAME c6280, Game_Music_Emu, Furnace, HuSIC | Use public docs/MAME references carefully; audit before copying |
| Namco arcade WSG | Clean-room wavetable model | MAME Namco WSG, Furnace | Use register docs and audited MAME files only |
| YM2151 / arcade FM | ymfm | Furnace, MAME | Use ymfm as first implementation candidate |
| YM2413 / OPLL | emu2413 or ymfm | Nuked-OPLL, Furnace | Prefer emu2413/ymfm; keep Nuked-OPLL reference-only unless licensing changes |
| Konami SCC | emu2212 | vgsound_emu, Furnace | Evaluate emu2212 first |

## Furnace Review Notes

Reviewed 2026-06-06 from `tildearrow/furnace` upstream `master` at commit `436d5222d5111273c3ef2b6ea2e5b4e10dcea65d`.

Furnace is valuable to Chipper as a broad comparison target because its public README lists direct overlap with nearly every requested Chipper family: NES/RP2A03, Game Boy, SID, AY/YM2149, SN76489, YM2612, OPL2/OPL3, SNES, POKEY, Amiga, HuC6280, Namco WSG, YM2151, YM2413, and Konami SCC/SCC+. It is also a useful product reference for chip-specific macros, tracker workflows, per-chip export/render behavior, and practical preset vocabulary.

Furnace should be treated as a discovery index, not a license shortcut. Its README says it uses quality emulation cores including Nuked, MAME, SameBoy, Mednafen PCE, NSFplay, puNES, reSID, Stella, SAASound, vgsound_emu, and ymfm. If Chipper evaluates one of those cores, audit that core at its own upstream source, commit, and license files rather than relying on Furnace's aggregate tree.

Licensing posture: Furnace's top-level `LICENSE` says most of Furnace is GPLv2-or-later, ASIO-enabled builds become GPLv3, and certain components have their own source/tree licenses. Keep Furnace reference-only unless Chipper intentionally adopts a compatible GPL distribution model and performs file-level audits for the exact source being reused.

## Preset Reference Notes

Open-source trackers and chip instruments may be used to improve Chipper's preset vocabulary, but not as unreviewed preset banks or code sources. Acceptable use includes comparing which chip-native controls matter to musicians, studying public chip workflow conventions, confirming useful instrument roles such as bass, lead, fake chord, noise percussion, UI blip, sweep, and riser, and building Chipper presets from its own clean-room/register-backed parameters.

Do not copy songs, copyrighted preset banks, binary modules, patch dumps, or code-derived lookup tables into Chipper without a separate rights and license review. When a project is GPL-family or LGPL-sensitive, keep preset work at the level of behavior research and attribution unless Chipper's distribution model intentionally adopts the needed obligations. The first source-balanced factory presets use this rule: they are Chipper-authored settings over existing internal engines, inspired by public chip conventions and open-source tracker/instrument workflows, with no third-party code or patch data vendored.

## Audit Checklist Before Vendoring

1. Record upstream URL, commit hash, release tag, license file, and all source file SPDX/header licenses.
2. Confirm whether the imported files bring additional third-party code, tables, ROM data, generated data, or submodules.
3. Decide how the core is integrated: vendored source, static library, dynamic library, test-only tool, or reference-only.
4. Add required license text and attribution to `THIRD_PARTY_NOTICES.md`.
5. Add renderer tests that prove the selected core actually improves authenticity for that chip.
6. Update `docs/emulation-accuracy.md` with the chip's implementation source and honest accuracy label.
