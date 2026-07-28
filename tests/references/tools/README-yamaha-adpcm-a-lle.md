# Yamaha ADPCM-A LLE reference reproduction

These fixtures are original synthetic register traces rendered by the
die-shot-level `nukeykt/YM2608-LLE` oracle at pinned commit
`7a2aca7b6830b96e48e3a4e1a40d15525993fa60`.

The oracle is GPL-2.0-or-later and is used only in an isolated reference build.
Its source and binaries are not vendored, linked into, or shipped with Chipper.
The generated WAVs contain no music, game data, presets, or upstream rhythm-ROM
content.

## Safe sparse checkout

The upstream repository contains an 8 KiB YM2608 rhythm-ROM header. Do not
checkout, inspect, compile, or redistribute that file. Fetch only the audited
source and license files:

```powershell
git clone --filter=blob:none --no-checkout https://github.com/nukeykt/YM2608-LLE.git C:\tmp\YM2608-LLE-safe
git -C C:\tmp\YM2608-LLE-safe sparse-checkout init --no-cone
git -C C:\tmp\YM2608-LLE-safe sparse-checkout set /fmopna_2608.c /fmopna_2608.h /fmopna_2610.c /fmopna_2610.h /fmopna_impl.c /fmopna_impl.h /LICENSE /README.md
git -C C:\tmp\YM2608-LLE-safe checkout 7a2aca7b6830b96e48e3a4e1a40d15525993fa60
```

Audited source hashes:

| File | SHA-256 |
| --- | --- |
| `fmopna_2608.c` | `CD09B74F0E6D291F9C7E50ADD7795D7EB182A9492E7F8F51300D89AD540F8AA5` |
| `fmopna_2610.c` | `10AA361BB883AD496D6977282D2304D85A3FC525DE7D87087027B0CEC8DDEC20` |
| `fmopna_impl.c` | `1398C91B1C6AE1D94864E857977AFA72EFDCCE7C4E609D5165EC567332088226` |
| `fmopna_impl.h` | `07E51E5C57B9D316FD2D844E1A11C10A82B73F479A4270EB45B8D102527C4880` |

## First-party inputs and OPNA ROM boundary

Generate Chipper's canonical banks and the temporary OPNA header:

```powershell
python tests\references\tools\generate_yamaha_adpcm_a_fixture_data.py `
  --opna tests\references\yamaha-adpcm-a-synthetic-opna.bin `
  --opnb tests\references\yamaha-adpcm-a-synthetic-opnb.bin `
  --opna-header C:\tmp\YM2608-LLE-safe\fmopna_rom.h
```

The checked-in bank bytes use the public high-nibble-first ADPCM-A file
convention. The LLE model exposes the internal YM2608 ROM's physical
low-nibble-first wiring, so the fixture-data tool swaps nibbles only while
writing the temporary `fmopna_rom.h`. The canonical `.bin` does not change.
YM2610 external memory is already high-nibble-first and requires no transform.

Expected bank hashes:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `yamaha-adpcm-a-synthetic-opna.bin` | 8192 | `e537223fa038a6cab245ec8ad1a13007e09f41fbc66b41e50403f0c4d79efdd4` |
| `yamaha-adpcm-a-synthetic-opnb.bin` | 768 | `21448b6d4d07d823abd176c6fc368d137891aef5ac9e73aaeec11bb2b7056194` |

## Pinned YM2610 compile guard

The pinned source references two YM2608-only GPIO fields in the YM2610 build.
Wrap the two assignments at `fmopna_impl.c:2443-2444`:

```c
#ifdef FMOPNA_YM2608
        chip->o_gpio_a_d = (chip->ssg_mode & 64) == 0;
        chip->o_gpio_b_d = (chip->ssg_mode & 128) == 0;
#endif
```

This is a build-only guard around fields absent from the YM2610 struct; it does
not change YM2608 or YM2610 sound logic.

## Build and capture

Compile each oracle wrapper exactly once; do not also compile
`fmopna_impl.c`:

```powershell
gcc -std=gnu11 -O2 -Wall -Wextra -DCHIPPER_LLE_OPNA `
  -I C:\tmp\YM2608-LLE-safe `
  tests\references\tools\generate_yamaha_adpcm_a_lle_reference.c `
  C:\tmp\YM2608-LLE-safe\fmopna_2608.c `
  -o C:\tmp\chipper-yamaha-opna-reference.exe

gcc -std=gnu11 -O2 -Wall -Wextra -DCHIPPER_LLE_OPNB `
  -I C:\tmp\YM2608-LLE-safe `
  tests\references\tools\generate_yamaha_adpcm_a_lle_reference.c `
  C:\tmp\YM2608-LLE-safe\fmopna_2610.c `
  -o C:\tmp\chipper-yamaha-opnb-reference.exe

C:\tmp\chipper-yamaha-opna-reference.exe `
  tests\references\yamaha-adpcm-a-synthetic-opna.bin `
  tests\references\yamaha-adpcm-a-opna-lle.wav

C:\tmp\chipper-yamaha-opnb-reference.exe `
  tests\references\yamaha-adpcm-a-synthetic-opnb.bin `
  tests\references\yamaha-adpcm-a-opnb-lle.wav
```

The capture tool follows the validated reset, register-bus, serial-output, and
YM2610 ROM-multiplex protocol used by Furnace at
`3d9060a468029d13cc0e6ef33202a1acb53953a9`. It holds reset for 576 master
cycles high, 576 low, and 576 high; writes only at the prescaler write slot;
reconstructs YM2610 ADPCM-A addresses from RMPX/RAD/RA8/RA20; and decodes
SH1/SH2 serial frames.

Native stereo frames are exact rational rates:

- OPNA: `7,987,200 / 144 = 166,400 / 3 Hz`
- OPNB: `8,000,000 / 144 = 500,000 / 9 Hz`

The generator converts those frames to 48 kHz with the same
accumulator-and-hold schedule used by `chipper_render`; it never labels a
fractional native rate as an integer-rate WAV.

Expected WAV hashes:

| File | SHA-256 |
| --- | --- |
| `yamaha-adpcm-a-opna-lle.wav` | `26d34c598cb904fa87a3effe0471fb663dff4356aa98167ad3b2d7c9d8ab2368` |
| `yamaha-adpcm-a-opnb-lle.wav` | `dad60ac442ecd4ecb61b6edefb4142080e7cf404ba50e10d9c1e7053115c58e8` |

The adjacent JSON files are authoritative for candidate render commands,
thresholds, measured baselines, and claim boundaries. Run
`tests/qualify_yamaha_adpcm_a_reference.py` to prove the gates reject a
nibble-order mutation and an OPNB page-register mutation.
