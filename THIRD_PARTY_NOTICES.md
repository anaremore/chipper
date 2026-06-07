# Third-Party Notices

Chipper is being developed with an explicit licensing rule: every chip mode must be backed by either clean-room/internal code or a third-party emulation core whose license has been audited before vendoring.

## Current Vendored Emulation Cores

### emu2413

- Project: `digital-sound-antiques/emu2413`
- Upstream URL: https://github.com/digital-sound-antiques/emu2413
- Vendored revision: `813cff619f5f01c47bd5c0588c1fd8435530b125`
- Vendored files: `ThirdParty/emu2413/emu2413.c`, `ThirdParty/emu2413/emu2413.h`, `ThirdParty/emu2413/LICENSE`, `ThirdParty/emu2413/README.md`
- License: MIT
- Use in Chipper: linked into `chipper_engine` as the YM2413/OPLL synthesis core. Chipper's adapter maps user-facing musical controls and renderer note events to YM2413 register writes for preset melodic channels.
- Accuracy claim: verified partial only. Rhythm mode, custom patch editing, VRC7/YMF281 patch-set selection, golden emulator comparisons, and hardware validation are not complete.

MIT License text from upstream:

```text
The MIT License (MIT)

Copyright (c) 2001-2019 Mitsutaka Okazaki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### emu76489

- Project: `digital-sound-antiques/emu76489`
- Upstream URL: https://github.com/digital-sound-antiques/emu76489
- Vendored revision: `2da50f887bf998e796c700e1a03f76c62a3809ff`
- Vendored files: `ThirdParty/emu76489/emu76489.c`, `ThirdParty/emu76489/emu76489.h`, `ThirdParty/emu76489/LICENSE`, `ThirdParty/emu76489/README.md`
- License: MIT
- Use in Chipper: linked into `chipper_engine` as the SN76489 / Sega PSG tone/noise generation core. Chipper's adapter maps user-facing musical controls, renderer note events, source gates, and register-write events to SN76489 latch/register writes, then applies Chipper-side source trims and optional modern stereo spread.
- Accuracy claim: verified partial only. Exact SN76489-family variant behavior, golden emulator comparison, and hardware validation are not complete.
- Provenance note: upstream source comments cite the SN76489 data sheet, MAME's `sn76489.c`, and SMSPower documentation as behavioral references. The vendored source and header are distributed by upstream under the MIT license shown below.

MIT License text from upstream:

```text
The MIT License (MIT)

Copyright (c) 2014 Mitsutaka Okazaki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The other current partial chip cores are clean-room/internal implementations in `Source/Engine/ChipCore.cpp`:

- NES / RP2A03 partial APU model
- Game Boy / DMG partial APU model
- SID / C64 partial voice/register model
- YM2149 / AY partial PSG model
- SNES SPC700-style partial sample-voice model
- Atari POKEY partial polynomial/timer model
- Amiga Paula partial tracker-sampler model
- PC Engine HuC6280 partial wavetable/noise model
- Namco arcade WSG partial wavetable model
- Konami SCC partial wavetable model

These implementations are not claimed to be cycle-accurate. See `docs/emulation-accuracy.md` for current accuracy status and known limitations.

## Candidate Cores Not Yet Vendored

These projects may be evaluated later. Listing them here is not an endorsement that they are already compatible or included.

The project owner is willing to open source Chipper if required for emulator-core licensing. Even so, GPL/LGPL code must not be imported until Chipper's license, source-distribution obligations, dynamic/static linking approach, and commercial distribution plan are explicitly chosen.

The full candidate map is maintained in `docs/emulator-source-map.md`.

Current priority candidates:

- Permissive-first candidates: [ymfm](https://github.com/aaronsgiles/ymfm), [digital-sound-antiques/emu2149](https://github.com/digital-sound-antiques/emu2149), [digital-sound-antiques/emu2212](https://github.com/digital-sound-antiques/emu2212), [ayumi](https://github.com/true-grue/ayumi), [SameBoy](https://github.com/LIJI32/SameBoy), [web-pokey](https://github.com/mrk-its/web-pokey), and Paula/ProTracker references. Audit required before vendoring. `emu2413` has passed the initial file-level MIT audit and is now vendored for YM2413/OPLL; `emu76489` has passed the initial file-level MIT audit and is now vendored for SN76489 / Sega PSG.
- SNES/SPC700 candidates: [emu-rs/snes-apu](https://github.com/emu-rs/snes-apu) reports BSD-2-Clause but must be audited for file-level provenance and its attributed higan/Blargg-derived behavior before reuse; [nyanpasu64-backup/snes-echo](https://github.com/nyanpasu64-backup/snes-echo) reports BSD-3-Clause and may be useful for echo/FIR research, but its README says it targets audible similarity rather than bit accuracy. No code from either project is vendored.
- LGPL-sensitive candidates: [FigBug/RP2A03](https://github.com/FigBug/RP2A03), [FigBug/SN76489](https://github.com/FigBug/SN76489), [osoumen/C700](https://github.com/osoumen/C700), [blarggs-audio-libraries/snes_spc](https://github.com/blarggs-audio-libraries/snes_spc), [Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2), [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3), and [Game_Music_Emu](https://github.com/libgme/game-music-emu). Do not import without an LGPL compliance plan.
- GPL/reference candidates: [FigBug/SID](https://github.com/FigBug/SID), [FigBug/PAPU](https://github.com/FigBug/PAPU), [libsidplayfp/reSIDfp](https://github.com/libsidplayfp/libsidplayfp), [Furnace](https://github.com/tildearrow/furnace), [MAME](https://www.mamedev.org/about.html), and GPL-family alternatives. Use as references or validation targets only unless Chipper adopts a compatible distribution model. Furnace was reviewed on 2026-06-06 as a broad chip-coverage, tracker UX, and emulator-core discovery reference; no Furnace code is vendored. Upstream states most Furnace is GPLv2-or-later, ASIO-enabled builds become GPLv3, and individual components may carry their own licenses.
- MAME-specific note: MAME is valuable for hardware behavior and edge cases, but project-level and per-file licensing must be checked before any reuse.

Latest source/license check on 2026-06-07 confirmed this posture for the user-suggested sources: `emu2413` and `emu76489` are MIT and vendored as listed above; FigBug/SID is GPL-3.0, FigBug/PAPU is GPL-2.0, FigBug/RP2A03 and FigBug/SN76489 are LGPL-2.1, Furnace is GPL-family at the aggregate project level with component-level licenses, ymfm is BSD-3-Clause, C700 and blargg snes_spc are LGPL-2.1, emu-rs/snes-apu reports BSD-2-Clause with important upstream attribution/provenance to audit, and snes-echo reports BSD-3-Clause. Non-vendored entries are still candidates or references only; no license text from those projects is bundled because no source from them is currently included.

Preset and macro design may be informed by public chip documentation, open-source tracker workflows, and reference instruments listed above, but Chipper's factory presets are authored as Chipper parameter snapshots. Do not import third-party preset banks, songs, modules, patch dumps, samples, lookup tables, or code-derived assets without adding a specific license and rights review here.

## Contribution Rule

When adding any third-party code:

1. Add the exact project name, upstream URL, commit/release, and license here.
2. Include the full license text if required by that license.
3. Document whether the code is vendored, linked, used only as a reference, or used only for tests.
4. Update `docs/emulation-accuracy.md` with the resulting accuracy label and validation status.
