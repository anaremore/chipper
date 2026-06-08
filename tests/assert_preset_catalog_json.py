import argparse
import json
import sys
from collections import Counter, defaultdict


REQUIRED_CATEGORIES = {
    "Arcade UI",
    "Classic Game SFX",
    "DMG Drums",
    "DMG Pulse",
    "DMG Wave",
    "NES Bass",
    "NES Drums",
    "NES Leads",
    "PC Engine HuC6280",
    "Konami SCC",
    "Namco WSG",
    "Amiga Paula",
    "SNES SPC700-style",
    "SID Bass",
    "SID FX",
    "SID PWM Leads",
    "SN76489 / Sega PSG",
    "Atari POKEY",
    "YM Arps",
    "YM Beeps",
    "YM Noise Percussion",
    "YM2612 / OPN2",
    "OPL2/OPL3 / DOS FM",
    "YM2151 / OPM",
}

IMPLEMENTED_CHIPS = {"nes", "dmg", "sid", "ym2149", "sn76489", "ym2612", "opl3", "ym2151", "spc700", "pokey", "paula", "huc6280", "namcoWsg", "ym2413", "scc"}

EXPECTED_CATEGORY_PREFIXES = {
    "nes": ("NES", "Arcade", "Classic"),
    "dmg": ("DMG", "Arcade", "Classic"),
    "sid": ("SID", "Modern", "Classic"),
    "ym2149": ("YM", "Arcade", "Classic"),
    "sn76489": ("SN76489", "Arcade", "Classic"),
    "spc700": ("SNES", "Classic"),
    "pokey": ("Atari", "Arcade", "Classic"),
    "paula": ("Amiga", "Tracker", "Classic"),
    "huc6280": ("PC Engine", "Arcade", "Classic"),
    "namcoWsg": ("Namco", "Arcade", "Classic"),
    "ym2612": ("OPN2", "YM2612", "Arcade", "Classic", "Modern"),
    "opl3": ("OPL2", "OPL3", "Arcade", "Classic"),
    "ym2151": ("OPM", "YM2151", "Arcade", "Classic", "Modern"),
    "ym2413": ("OPLL", "YM2413", "Classic"),
    "scc": ("Konami", "Arcade", "Classic"),
}


def main() -> int:
    parser = argparse.ArgumentParser(description="Assert factory preset catalog metadata quality.")
    parser.add_argument("path")
    parser.add_argument("--min-presets", type=int, default=80)
    parser.add_argument("--min-per-chip", type=int, default=6)
    args = parser.parse_args()

    with open(args.path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    failures: list[str] = []
    presets = data.get("presets", [])
    if len(presets) < args.min_presets:
        failures.append(f"expected at least {args.min_presets} presets, got {len(presets)}")

    ids = [str(preset.get("id", "")) for preset in presets]
    names = [str(preset.get("name", "")) for preset in presets]
    for label, values in (("id", ids), ("name", names)):
        duplicates = sorted(value for value, count in Counter(values).items() if value and count > 1)
        if duplicates:
            failures.append(f"duplicate preset {label}s: {', '.join(duplicates)}")

    categories = {str(preset.get("category", "")) for preset in presets}
    missing_categories = sorted(REQUIRED_CATEGORIES - categories)
    if missing_categories:
        failures.append(f"missing required preset categories: {', '.join(missing_categories)}")

    by_chip: dict[str, list[dict]] = defaultdict(list)
    for preset in presets:
        preset_id = str(preset.get("id", ""))
        chip = str(preset.get("chipKey", ""))
        category = str(preset.get("category", ""))
        name = str(preset.get("name", ""))
        note = str(preset.get("note", ""))

        if not preset_id:
            failures.append(f"preset missing id: {preset!r}")
        if not name:
            failures.append(f"{preset_id}: preset name is empty")
        if not category:
            failures.append(f"{preset_id}: preset category is empty")
        if len(note.strip()) < 12:
            failures.append(f"{preset_id}: preset note is too short or empty")

        if chip:
            by_chip[chip].append(preset)

        allowed_prefixes = EXPECTED_CATEGORY_PREFIXES.get(chip)
        if allowed_prefixes and not category.startswith(allowed_prefixes):
            failures.append(f"{preset_id}: category {category!r} does not match chip {chip!r}")

        controls = preset.get("controls", [])
        if len(controls) != 4:
            failures.append(f"{preset_id}: expected 4 controls, got {len(controls)}")
        for index, value in enumerate(controls):
            if not isinstance(value, (int, float)) or not 0.0 <= float(value) <= 1.0:
                failures.append(f"{preset_id}: control {index + 1} out of normalized range: {value!r}")

        source_enabled = preset.get("sourceEnabled", [])
        if len(source_enabled) != 4:
            failures.append(f"{preset_id}: expected 4 sourceEnabled values, got {len(source_enabled)}")
        if source_enabled and not any(bool(value) for value in source_enabled):
            failures.append(f"{preset_id}: all sources are disabled")

    for chip in sorted(IMPLEMENTED_CHIPS):
        count = len(by_chip.get(chip, []))
        if count < args.min_per_chip:
            failures.append(f"{chip}: expected at least {args.min_per_chip} presets, got {count}")

    planned_chips_with_presets = sorted(set(by_chip) - IMPLEMENTED_CHIPS)
    if planned_chips_with_presets:
        failures.append(f"planned/unimplemented chips should not have factory presets yet: {', '.join(planned_chips_with_presets)}")

    if failures:
        print("preset catalog metadata failures:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(f"Validated {len(presets)} factory presets across {len(by_chip)} implemented chips.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
