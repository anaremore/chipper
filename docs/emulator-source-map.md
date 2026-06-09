# Emulator Source Candidate Map

This document captures emulator projects shared as possible Chipper source material. Some permissive cores are now vendored, and the remaining entries are candidates, references, or validation targets only. Check `THIRD_PARTY_NOTICES.md` before assuming any source is included.

Every non-vendored project listed here still needs a file-level license audit before any code is imported, linked, translated, or bundled. "Reference" means behavior comparison, UX comparison, rendered-output comparison, or documentation research. It does not mean copying source code.

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
- ymfm's repository describes BSD-licensed Yamaha FM cores and its top-level `LICENSE` is BSD-3-Clause. A local file/header audit on 2026-06-07 at commit `17decfae857b92ab55fbb30ade2287ace095a381` found BSD-3-Clause headers on the core source files inspected (`src/ymfm*.h`, `src/ymfm*.cpp`, `src/ymfm_fm.ipp`) and README/GeneralInfo provenance notes that the cores began in MAME, use MAME-style design, and are cross-checked against Nemesis/Exodus and Nuked research while explicitly aiming for high audible accuracy rather than cycle accuracy. Chipper now vendors ymfm for verified-partial YM2612/OPN2, OPL2/YM3812, and YM2151/OPM melodic adapters; OPL3-specific behavior remains future work.
- osoumen/C700 is a dedicated SNES sampler/emulation VST reference and GitHub reports LGPL-2.1. Treat as license-sensitive comparison material, especially for BRR/sample-bank UI and SPC file workflow.
- blarggs-audio-libraries/snes_spc is a foundational SPC700/S-DSP emulator library and its README/license metadata report LGPL-2.1. Treat as license-sensitive unless an LGPL compliance path is chosen.
- emu-rs/snes-apu reports BSD-2-Clause, but its README attributes core SMP work to higan and DSP envelope work to Blargg's snes_spc. Treat as a promising candidate only after file-level and provenance audit.
- nyanpasu64-backup/snes-echo reports BSD-3-Clause and is a Faust SNES echo/reverb model that explicitly does not strive for bit accuracy. Treat as a useful echo/FIR reference or possible permissive DSP candidate after file-level audit.

## Preferred Implementation Tiers

### Tier 1: Permissive-First Candidates

These should be evaluated first because their upstream license posture appears most compatible with broad VST distribution, subject to file-level audit.

| Project | Candidate Use | Upstream License Posture |
| --- | --- | --- |
| [ymfm](https://github.com/aaronsgiles/ymfm) | YM2612/OPN2, OPL2/YM3812, and YM2151/OPM now; OPL3-specific expansion future candidate | BSD-3-Clause; vendored at `17decfae857b92ab55fbb30ade2287ace095a381`; core source headers inspected carry BSD-3-Clause, README notes MAME origins/design and high-audible-accuracy rather than cycle-accuracy goals |
| [digital-sound-antiques/emu2149](https://github.com/digital-sound-antiques/emu2149) | YM2149 / AY PSG | MIT; vendored at `02fc5f0b411c35e3d69cc7f161595b56b4fda4f2`; source header cites psg.vhd, NEZplug, MAME `ay8910.c`, MSX-Datapack, and AY datasheet as references, so provenance is explicitly noted |
| [digital-sound-antiques/emu76489](https://github.com/digital-sound-antiques/emu76489) | SN76489 / Sega PSG | MIT; vendored at `2da50f887bf998e796c700e1a03f76c62a3809ff` |
| [digital-sound-antiques/emu2413](https://github.com/digital-sound-antiques/emu2413) | YM2413 / OPLL | MIT; vendored at `813cff619f5f01c47bd5c0588c1fd8435530b125` |
| [digital-sound-antiques/emu2212](https://github.com/digital-sound-antiques/emu2212) | Konami SCC | MIT; vendored at `ce7ade109942d9ba1b8067a27af47ddf37795fd7` |
| [ayumi](https://github.com/true-grue/ayumi) | AY-3-8910 / YM2149 | GitHub reports MIT |
| [SameBoy](https://github.com/LIJI32/SameBoy) | Game Boy / DMG reference or possible extraction | Upstream reports an Expat-style license, with folder-specific exceptions to audit |
| [web-pokey](https://github.com/mrk-its/web-pokey) | Atari POKEY behavior reference or port candidate | GitHub reports MIT |
| [emu-rs/snes-apu](https://github.com/emu-rs/snes-apu) | SNES SPC700/S-DSP behavior candidate | GitHub reports BSD-2-Clause, but upstream attribution to higan and Blargg requires provenance audit before reuse |
| [nyanpasu64-backup/snes-echo](https://github.com/nyanpasu64-backup/snes-echo) | SNES echo/FIR behavior reference or DSP candidate | GitHub reports BSD-3-Clause; README says it targets audible similarity rather than bit accuracy |
| [pt2-clone](https://github.com/8bitbubsy/pt2-clone) | Amiga Paula / ProTracker behavior reference | Candidate; audit before reuse |

### Tier 2: Strong But License-Sensitive

These may be excellent accuracy choices, but require an explicit LGPL compliance plan before any code reuse.

| Project | Candidate Use | Upstream License Posture |
| --- | --- | --- |
| [Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2) | High-accuracy YM3438/YM2612 | GitHub reports LGPL-2.1 |
| [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3) | High-accuracy YMF262/OPL3 | GitHub reports LGPL-2.1 |
| [FigBug/RP2A03](https://github.com/FigBug/RP2A03) | NES/RP2A03 VST reference and test-oracle candidate | GitHub reports LGPL-2.1 |
| [FigBug/SN76489](https://github.com/FigBug/SN76489) | SN76489 VST reference and test-oracle candidate | GitHub reports LGPL-2.1 |
| [osoumen/C700](https://github.com/osoumen/C700) | SNES sampler/VST workflow, BRR/SPC loading, sample-bank UX, and hardware-sync reference | GitHub reports LGPL-2.1 |
| [blarggs-audio-libraries/snes_spc](https://github.com/blarggs-audio-libraries/snes_spc) | SPC700/S-DSP emulator and reference-output candidate | GitHub reports LGPL-2.1 |
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
| YM2149 / AY | emu2149 vendored; ayumi and ymfm remain alternates | Game_Music_Emu, Furnace, MAME | MIT emu2149 now backs the partial PSG core; preserve Chipper's register/macro adapter and add golden comparisons before increasing accuracy claims |
| SN76489 / Sega PSG | emu76489 vendored | FigBug/SN76489, Game_Music_Emu, Furnace | MIT emu76489 now backs the partial PSG core; use FigBug as LGPL comparison target and add golden comparisons/hardware validation before upgrading accuracy claims |
| YM2612 / Genesis FM | ymfm vendored at `17decfae857b92ab55fbb30ade2287ace095a381` | Nuked-OPN2, Game_Music_Emu, Furnace | ymfm now backs the first verified-partial six-channel melodic adapter with all six source lanes exposed; add a dedicated operator UI, DAC, LFO, and golden comparisons before increasing accuracy claims |
| OPL2 / OPL3 / DOS FM | ymfm vendored at `17decfae857b92ab55fbb30ade2287ace095a381` for OPL2/YM3812 | Nuked-OPL3, Furnace | ymfm now backs the first verified-partial OPL2 melodic adapter; expand to OPL3 stereo/18-channel/four-op behavior before increasing OPL3 claims |
| SNES SPC700-style | Current clean-room partial sample-voice model; emu-rs/snes-apu and snes-echo are permissive-looking audit candidates | C700, blargg snes_spc, Game_Music_Emu, Furnace, MAME | Continue clean-room sample-voice work; audit snes-apu provenance and snes-echo DSP before reuse; keep C700, blargg snes_spc, GME, Furnace, and MAME as comparison/reference material until licensing is deliberately chosen |
| Atari POKEY | Current clean-room partial model; web-pokey remains a future audit candidate | Game_Music_Emu, MAME, Furnace | Continue clean-room POKEY work for now; audit web-pokey and MAME file headers before any reuse |
| Amiga Paula | Current clean-room partial tracker sampler; pt2-clone reference remains a future audit candidate | Paula Tracker / PaulaLib, libxmp, Furnace | Continue clean-room period/volume/channel work; compare against tracker references, but do not reuse Paula/MOD code or sample data until exact source licenses are recorded |
| PC Engine HuC6280 | Current clean-room partial wavetable/noise model | MAME c6280, Game_Music_Emu, Furnace, HuSIC | Continue clean-room work; use MAME/GME/Furnace/HuSIC as comparison and audit targets only unless an exact source/license review is recorded |
| Namco arcade WSG | Current clean-room partial wavetable model | MAME Namco WSG, Furnace | Continue clean-room work; keep MAME/Furnace as comparison and audit targets only unless exact source/file licenses are recorded |
| YM2151 / arcade FM | ymfm vendored at `17decfae857b92ab55fbb30ade2287ace095a381` | Furnace, MAME | ymfm now backs the first verified-partial OPM melodic adapter; expand from four exposed lanes to full eight-voice UI, OPM LFO/noise/timer controls, and golden comparisons before increasing accuracy claims |
| YM2413 / OPLL | emu2413 vendored; ymfm remains broader FM candidate | Nuked-OPLL, Furnace | emu2413 MIT core is used for the first partial OPLL melodic-channel adapter; keep Nuked-OPLL reference-only unless licensing changes |
| Konami SCC | emu2212 vendored | vgsound_emu, Furnace, MAME | MIT emu2212 now backs the partial SCC/SCC+ core; preserve Chipper's register/macro adapter and add golden comparisons before increasing accuracy claims |

## Furnace Review Notes

Reviewed 2026-06-06 from `tildearrow/furnace` upstream `master` at commit `436d5222d5111273c3ef2b6ea2e5b4e10dcea65d`.

Furnace is valuable to Chipper as a broad comparison target because its public README lists direct overlap with nearly every requested Chipper family: NES/RP2A03, Game Boy, SID, AY/YM2149, SN76489, YM2612, OPL2/OPL3, SNES, POKEY, Amiga, HuC6280, Namco WSG, YM2151, YM2413, and Konami SCC/SCC+. It is also a useful product reference for chip-specific macros, tracker workflows, per-chip export/render behavior, and practical preset vocabulary.

Furnace should be treated as a discovery index, not a license shortcut. Its README says it uses quality emulation cores including Nuked, MAME, SameBoy, Mednafen PCE, NSFplay, puNES, reSID, Stella, SAASound, vgsound_emu, and ymfm. If Chipper evaluates one of those cores, audit that core at its own upstream source, commit, and license files rather than relying on Furnace's aggregate tree.

Licensing posture: Furnace's top-level `LICENSE` says most of Furnace is GPLv2-or-later, ASIO-enabled builds become GPLv3, and certain components have their own source/tree licenses. Keep Furnace reference-only unless Chipper intentionally adopts a compatible GPL distribution model and performs file-level audits for the exact source being reused.

## SNES / SPC700 Review Notes

The SNES path is tracked in more detail in `docs/snes-spc700-plan.md`. Chipper should treat SNES as an eight-voice BRR sample-player instrument with S-DSP envelopes, Gaussian interpolation, voice pitch modulation/noise options, and shared echo/FIR behavior. The mode should not be designed like the raw PSG/APU chips or a generic oscillator synth.

The current source posture is: C700 and Blargg `snes_spc` are valuable but LGPL-sensitive; Furnace is reference-only unless Chipper goes GPL-compatible; `emu-rs/snes-apu` is a promising BSD-2-Clause candidate that needs provenance audit because it credits higan and Blargg-derived behavior; `snes-echo` is BSD-3-Clause and useful for echo research, but it explicitly targets audible similarity rather than bit accuracy. No SNES third-party code is currently vendored.

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
