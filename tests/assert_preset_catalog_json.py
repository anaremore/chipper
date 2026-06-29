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
    "NES + VRC6 Leads",
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
    "YM2203 / OPN",
    "OPL2/OPL3 / DOS FM",
    "YM2151 / OPM",
}

IMPLEMENTED_CHIPS = {"nes", "nesVrc6", "dmg", "sid", "ym2149", "sn76489", "ym2612", "ym2203", "opl3", "ym2151", "spc700", "pokey", "paula", "huc6280", "namcoWsg", "ym2413", "scc"}

EXPECTED_CATEGORY_PREFIXES = {
    "nes": ("NES", "Arcade", "Classic"),
    "nesVrc6": ("NES + VRC6", "Arcade", "Classic"),
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
    "ym2203": ("OPN", "YM2203", "Arcade", "Classic", "Modern"),
    "opl3": ("OPL2", "OPL3", "Arcade", "Classic"),
    "ym2151": ("OPM", "YM2151", "Arcade", "Classic", "Modern"),
    "ym2413": ("OPLL", "YM2413", "Classic"),
    "scc": ("Konami", "Arcade", "Classic"),
}

EXPECTED_ROLES = {
    "Arp",
    "Bass",
    "Drums",
    "Keys / Pad",
    "Lead",
    "Patch",
    "SFX",
}

CORE_ROLE_COVERAGE = EXPECTED_ROLES - {"Patch"}


def tag_token(text: str) -> str:
    output: list[str] = []
    needs_separator = False
    for char in text.lower():
        if char.isalnum():
            if needs_separator and output:
                output.append("-")
            output.append(char)
            needs_separator = False
        else:
            needs_separator = bool(output)
    return "".join(output)


def main() -> int:
    parser = argparse.ArgumentParser(description="Assert factory preset catalog metadata quality.")
    parser.add_argument("path")
    parser.add_argument("--min-presets", type=int, default=180)
    parser.add_argument("--min-per-chip", type=int, default=12)
    parser.add_argument("--min-core-roles-per-chip", type=int, default=len(CORE_ROLE_COVERAGE))
    parser.add_argument("--min-role-density-per-chip", type=int, default=2)
    parser.add_argument("--min-reference-tags-per-target", type=int, default=5)
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
    role_counts: Counter[str] = Counter()
    engine_counts: Counter[str] = Counter()
    for preset in presets:
        preset_id = str(preset.get("id", ""))
        chip = str(preset.get("chipKey", ""))
        category = str(preset.get("category", ""))
        name = str(preset.get("name", ""))
        note = str(preset.get("note", ""))
        role = str(preset.get("role", ""))
        engine = str(preset.get("engine", ""))
        tags = preset.get("tags", [])

        if not preset_id:
            failures.append(f"preset missing id: {preset!r}")
        if not name:
            failures.append(f"{preset_id}: preset name is empty")
        if not category:
            failures.append(f"{preset_id}: preset category is empty")
        if len(note.strip()) < 12:
            failures.append(f"{preset_id}: preset note is too short or empty")
        if role not in EXPECTED_ROLES:
            failures.append(f"{preset_id}: preset role {role!r} is not one of {sorted(EXPECTED_ROLES)!r}")
        if not engine:
            failures.append(f"{preset_id}: preset engine is empty")
        if not isinstance(tags, list) or not tags:
            failures.append(f"{preset_id}: preset tags must be a non-empty list")
            tags = []
        else:
            normalized_tags = [str(tag) for tag in tags]
            if len(set(normalized_tags)) != len(normalized_tags):
                failures.append(f"{preset_id}: preset tags contain duplicates")
            for tag in normalized_tags:
                if not tag or tag != tag_token(tag):
                    failures.append(f"{preset_id}: preset tag {tag!r} is not normalized")
            if "factory" not in normalized_tags:
                failures.append(f"{preset_id}: preset tags should include factory")
            role_tag = tag_token(role)
            if role_tag and role_tag not in normalized_tags:
                failures.append(f"{preset_id}: preset tags should include role tag {role_tag!r}")

        role_counts[role] += 1
        engine_counts[engine] += 1

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
        chip_presets = by_chip.get(chip, [])
        count = len(chip_presets)
        if count < args.min_per_chip:
            failures.append(f"{chip}: expected at least {args.min_per_chip} presets, got {count}")
        chip_core_roles = {str(preset.get("role", "")) for preset in chip_presets} & CORE_ROLE_COVERAGE
        if len(chip_core_roles) < args.min_core_roles_per_chip:
            missing_roles = sorted(CORE_ROLE_COVERAGE - chip_core_roles)
            failures.append(
                f"{chip}: expected at least {args.min_core_roles_per_chip} core preset roles, "
                f"got {len(chip_core_roles)}; missing {', '.join(missing_roles)}"
            )
        if args.min_role_density_per_chip > 0:
            role_counts_for_chip = Counter(str(preset.get("role", "")) for preset in chip_presets)
            underfilled_roles = [
                f"{role}={role_counts_for_chip.get(role, 0)}"
                for role in sorted(CORE_ROLE_COVERAGE)
                if role_counts_for_chip.get(role, 0) < args.min_role_density_per_chip
            ]
            if underfilled_roles:
                failures.append(
                    f"{chip}: expected at least {args.min_role_density_per_chip} presets for each core role; "
                    f"underfilled {', '.join(underfilled_roles)}"
                )

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

    quality_targets = summary.get("qualityTargets", [])
    if not isinstance(quality_targets, list):
        failures.append("summary qualityTargets must be a list")
        quality_targets = []

    expected_target_chips = [args.only_chip] if args.only_chip else sorted(IMPLEMENTED_CHIPS)
    declared_target_chips: list[str] = []
    for target in quality_targets:
        if not isinstance(target, dict):
            failures.append(f"summary qualityTargets entry must be an object: {target!r}")
            continue

        chip = str(target.get("chipKey", ""))
        declared_target_chips.append(chip)
        if chip not in expected_target_chips:
            failures.append(f"quality target chip {chip!r} is not expected for this catalog")

        source = str(target.get("source", ""))
        if "Furnace-informed" not in source:
            failures.append(f"{chip}: quality target source should name the Furnace-informed clean-room policy")

        chip_presets = by_chip.get(chip, [])
        chip_preset_count = len(chip_presets)
        minimum_preset_count = target.get("minimumPresetCount")
        if not isinstance(minimum_preset_count, int) or isinstance(minimum_preset_count, bool) or minimum_preset_count <= 0:
            failures.append(f"{chip}: quality target minimumPresetCount must be a positive integer, got {minimum_preset_count!r}")
            minimum_preset_count = 0

        expected_missing_preset_count = max(0, minimum_preset_count - chip_preset_count)
        covered_preset_count = target.get("coveredPresetCount")
        missing_preset_count = target.get("missingPresetCount")
        if covered_preset_count != chip_preset_count:
            failures.append(f"{chip}: quality target coveredPresetCount expected {chip_preset_count}, got {covered_preset_count!r}")
        if missing_preset_count != expected_missing_preset_count:
            failures.append(f"{chip}: quality target missingPresetCount expected {expected_missing_preset_count}, got {missing_preset_count!r}")
        if expected_missing_preset_count:
            failures.append(
                f"{chip}: quality target requires at least {minimum_preset_count} presets, "
                f"got {chip_preset_count}"
            )

        required_roles = target.get("requiredRoles", [])
        if not isinstance(required_roles, list) or not required_roles:
            failures.append(f"{chip}: quality target requiredRoles must be a non-empty list")
            required_roles = []
        required_roles = [str(role) for role in required_roles]
        for role in required_roles:
            if role not in EXPECTED_ROLES:
                failures.append(f"{chip}: quality target role {role!r} is not one of {sorted(EXPECTED_ROLES)!r}")

        reference_tags = target.get("referenceTags", [])
        if not isinstance(reference_tags, list) or not reference_tags:
            failures.append(f"{chip}: quality target referenceTags must be a non-empty list")
            reference_tags = []
        reference_tags = [str(item) for item in reference_tags]
        if len(reference_tags) < args.min_reference_tags_per_target:
            failures.append(
                f"{chip}: quality target expected at least {args.min_reference_tags_per_target} reference tags, "
                f"got {len(reference_tags)}"
            )
        for tag in reference_tags:
            if not tag or tag != tag_token(tag):
                failures.append(f"{chip}: quality target reference tag {tag!r} is not normalized")

        def read_target_counts(field: str, name_key: str) -> dict[str, int]:
            items = target.get(field, [])
            if not isinstance(items, list):
                failures.append(f"{chip}: quality target {field} must be a list")
                return {}

            declared: dict[str, int] = {}
            for item in items:
                if not isinstance(item, dict):
                    failures.append(f"{chip}: quality target {field} entry must be an object: {item!r}")
                    continue
                name = str(item.get(name_key, ""))
                count = item.get("presetCount")
                if not name:
                    failures.append(f"{chip}: quality target {field} entry missing {name_key}: {item!r}")
                    continue
                if not isinstance(count, int):
                    failures.append(f"{chip}: quality target {field} {name}: presetCount must be an integer, got {count!r}")
                    continue
                if name in declared:
                    failures.append(f"{chip}: quality target {field} duplicates {name_key} {name}")
                declared[name] = count
            return declared

        chip_roles = {str(preset.get("role", "")) for preset in chip_presets}
        expected_role_counts = {
            role: sum(1 for preset in chip_presets if str(preset.get("role", "")) == role)
            for role in required_roles
        }
        declared_role_counts = read_target_counts("roleCounts", "role")
        if declared_role_counts != expected_role_counts:
            failures.append(f"{chip}: quality target roleCounts expected {expected_role_counts!r}, got {declared_role_counts!r}")

        expected_missing_roles = sorted(role for role in required_roles if role not in chip_roles)
        declared_missing_roles = sorted(str(role) for role in target.get("missingRoles", []))
        if declared_missing_roles != expected_missing_roles:
            failures.append(f"{chip}: quality target missingRoles expected {expected_missing_roles!r}, got {declared_missing_roles!r}")
        if declared_missing_roles:
            failures.append(f"{chip}: quality target is missing required roles: {', '.join(declared_missing_roles)}")

        target_role_count = target.get("targetRoleCount")
        covered_role_count = target.get("coveredRoleCount")
        if target_role_count != len(required_roles):
            failures.append(f"{chip}: quality target targetRoleCount expected {len(required_roles)}, got {target_role_count!r}")
        if covered_role_count != len(required_roles) - len(expected_missing_roles):
            failures.append(
                f"{chip}: quality target coveredRoleCount expected {len(required_roles) - len(expected_missing_roles)}, "
                f"got {covered_role_count!r}"
            )

        chip_tags = {str(tag) for preset in chip_presets for tag in preset.get("tags", [])}
        expected_reference_tag_counts = {
            tag: sum(1 for preset in chip_presets if tag in [str(item) for item in preset.get("tags", [])])
            for tag in reference_tags
        }
        declared_reference_tag_counts = read_target_counts("referenceTagCounts", "tag")
        if declared_reference_tag_counts != expected_reference_tag_counts:
            failures.append(
                f"{chip}: quality target referenceTagCounts expected {expected_reference_tag_counts!r}, "
                f"got {declared_reference_tag_counts!r}"
            )

        expected_missing_reference_tags = sorted(tag for tag in reference_tags if tag not in chip_tags)
        declared_missing_reference_tags = sorted(str(tag) for tag in target.get("missingReferenceTags", []))
        if declared_missing_reference_tags != expected_missing_reference_tags:
            failures.append(
                f"{chip}: quality target missingReferenceTags expected {expected_missing_reference_tags!r}, "
                f"got {declared_missing_reference_tags!r}"
            )
        if declared_missing_reference_tags:
            failures.append(f"{chip}: quality target is missing reference tags: {', '.join(declared_missing_reference_tags)}")

        target_reference_tag_count = target.get("targetReferenceTagCount")
        covered_reference_tag_count = target.get("coveredReferenceTagCount")
        if target_reference_tag_count != len(reference_tags):
            failures.append(
                f"{chip}: quality target targetReferenceTagCount expected {len(reference_tags)}, "
                f"got {target_reference_tag_count!r}"
            )
        if covered_reference_tag_count != len(reference_tags) - len(expected_missing_reference_tags):
            failures.append(
                f"{chip}: quality target coveredReferenceTagCount expected {len(reference_tags) - len(expected_missing_reference_tags)}, "
                f"got {covered_reference_tag_count!r}"
            )

        note = str(target.get("note", ""))
        if len(note.strip()) < 24:
            failures.append(f"{chip}: quality target note is too short")

    if sorted(declared_target_chips) != expected_target_chips:
        failures.append(f"quality target chips expected {expected_target_chips!r}, got {sorted(declared_target_chips)!r}")

    def read_named_counts(field: str, name_key: str) -> dict[str, int]:
        items = summary.get(field, [])
        if not isinstance(items, list):
            failures.append(f"summary {field} must be a list")
            return {}
        declared: dict[str, int] = {}
        for item in items:
            if not isinstance(item, dict):
                failures.append(f"summary {field} entry must be an object: {item!r}")
                continue
            name = str(item.get(name_key, ""))
            count = item.get("presetCount")
            if not name:
                failures.append(f"summary {field} entry missing {name_key}: {item!r}")
                continue
            if not isinstance(count, int):
                failures.append(f"summary {field} {name}: presetCount must be an integer, got {count!r}")
                continue
            if name in declared:
                failures.append(f"summary {field} duplicates {name_key} {name}")
            declared[name] = count
        return declared

    declared_role_counts = read_named_counts("roleCounts", "role")
    actual_role_counts = dict(role_counts)
    if declared_role_counts != actual_role_counts:
        failures.append(f"summary roleCounts expected {actual_role_counts!r}, got {declared_role_counts!r}")

    declared_engine_counts = read_named_counts("engineCounts", "engine")
    actual_engine_counts = dict(engine_counts)
    if declared_engine_counts != actual_engine_counts:
        failures.append(f"summary engineCounts expected {actual_engine_counts!r}, got {declared_engine_counts!r}")

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
