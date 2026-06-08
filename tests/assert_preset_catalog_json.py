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
    parser.add_argument("--only-chip", help="Assert that every preset belongs to this chip key.")
    args = parser.parse_args()

    with open(args.path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    failures: list[str] = []
    presets = data.get("presets", [])
    if len(presets) < args.min_presets:
        failures.append(f"expected at least {args.min_presets} presets, got {len(presets)}")

    summary = data.get("summary")
    if not isinstance(summary, dict):
        failures.append("missing summary object")
        summary = {}
    if summary.get("totalPresetCount") != len(presets):
        failures.append(f"summary totalPresetCount expected {len(presets)}, got {summary.get('totalPresetCount')!r}")

    ids = [str(preset.get("id", "")) for preset in presets]
    names = [str(preset.get("name", "")) for preset in presets]
    for label, values in (("id", ids), ("name", names)):
        duplicates = sorted(value for value, count in Counter(values).items() if value and count > 1)
        if duplicates:
            failures.append(f"duplicate preset {label}s: {', '.join(duplicates)}")

    if not args.only_chip:
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
        if args.only_chip and chip != args.only_chip:
            failures.append(f"{preset_id}: expected chip {args.only_chip!r}, got {chip!r}")

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

    chips_to_check = [args.only_chip] if args.only_chip else sorted(IMPLEMENTED_CHIPS)
    for chip in chips_to_check:
        count = len(by_chip.get(chip, []))
        if count < args.min_per_chip:
            failures.append(f"{chip}: expected at least {args.min_per_chip} presets, got {count}")

    summary_chip_counts = summary.get("chipCounts", [])
    if not isinstance(summary_chip_counts, list):
        failures.append("summary chipCounts must be a list")
        summary_chip_counts = []
    declared_chip_counts: dict[str, int] = {}
    for item in summary_chip_counts:
        if not isinstance(item, dict):
            failures.append(f"summary chipCounts entry must be an object: {item!r}")
            continue
        chip = str(item.get("chipKey", ""))
        count = item.get("presetCount")
        if not chip:
            failures.append(f"summary chipCounts entry missing chipKey: {item!r}")
            continue
        if not isinstance(count, int):
            failures.append(f"summary chipCounts {chip}: presetCount must be an integer, got {count!r}")
            continue
        if chip in declared_chip_counts:
            failures.append(f"summary chipCounts duplicates chip {chip}")
        declared_chip_counts[chip] = count

    actual_chip_counts = {chip: len(values) for chip, values in by_chip.items()}
    if declared_chip_counts != actual_chip_counts:
        failures.append(f"summary chipCounts expected {actual_chip_counts!r}, got {declared_chip_counts!r}")

    if not args.only_chip:
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
