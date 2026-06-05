# Third-Party Notices

Chipper is being developed with an explicit licensing rule: every chip mode must be backed by either clean-room/internal code or a third-party emulation core whose license has been audited before vendoring.

## Current Vendored Emulation Cores

None.

The current partial chip cores are clean-room/internal implementations in `Source/Engine/ChipCore.cpp`:

- NES / RP2A03 partial APU model
- Game Boy / DMG partial APU model
- YM2149 / AY partial PSG model
- SN76489 partial PSG model

These implementations are not claimed to be cycle-accurate. See `docs/emulation-accuracy.md` for current accuracy status and known limitations.

## Candidate Cores Not Yet Vendored

These projects may be evaluated later. Listing them here is not an endorsement that they are already compatible or included.

The project owner is willing to open source Chipper if required for emulator-core licensing. Even so, GPL/LGPL code must not be imported until Chipper's license, source-distribution obligations, dynamic/static linking approach, and commercial distribution plan are explicitly chosen.

- `ymfm`: candidate for Yamaha FM chips such as YM2612/OPN2, YM2151/OPM, YM2413/OPLL, and OPL2/OPL3. License audit required before vendoring.
- SameBoy APU code: candidate reference or source for Game Boy/DMG behavior. License audit required before vendoring.
- [FigBug/SID](https://github.com/FigBug/SID): C64 SID VST/AU emulation. GitHub reports GPL-3.0. Reference/inspiration only unless Chipper adopts a GPL-compatible distribution model.
- [FigBug/RP2A03](https://github.com/FigBug/RP2A03): NES RP2A03 VST/AU synth. GitHub reports LGPL-2.1. Reference/test-oracle candidate only unless LGPL linking and redistribution requirements are explicitly accepted.
- [FigBug/PAPU](https://github.com/FigBug/PAPU): Game Boy PAPU VST/AU emulation. GitHub reports GPL-2.0. Reference/inspiration only unless Chipper adopts a GPL-compatible distribution model.
- [FigBug/SN76489](https://github.com/FigBug/SN76489): Sega Master System SN76489 VST/AU emulation. GitHub reports LGPL-2.1. Reference/test-oracle candidate only unless LGPL linking and redistribution requirements are explicitly accepted.
- SID, SPC700, and other emulator cores: many available implementations use GPL/LGPL-family licenses. Do not vendor without an explicit distribution decision.

## Contribution Rule

When adding any third-party code:

1. Add the exact project name, upstream URL, commit/release, and license here.
2. Include the full license text if required by that license.
3. Document whether the code is vendored, linked, used only as a reference, or used only for tests.
4. Update `docs/emulation-accuracy.md` with the resulting accuracy label and validation status.
