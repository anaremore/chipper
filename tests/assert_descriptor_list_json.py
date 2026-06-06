import argparse
import json
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Assert chipper_render descriptor-list JSON properties.")
    parser.add_argument("path")
    parser.add_argument("--schema", default="chipper.descriptors.v1")
    parser.add_argument("--chip-count", type=int)
    parser.add_argument("--chip", action="append", default=[], metavar="KEY")
    parser.add_argument("--implemented", action="append", default=[], metavar="KEY=0|1")
    parser.add_argument("--param-surface", action="append", default=[], metavar="KEY:ROLE=SURFACE")
    parser.add_argument("--param-kind", action="append", default=[], metavar="KEY:ROLE=KIND")
    parser.add_argument("--param-choices", action="append", default=[], metavar="KEY:ROLE=COUNT")
    parser.add_argument("--preset", action="append", default=[], metavar="ID")
    args = parser.parse_args()

    with open(args.path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    failures: list[str] = []

    if data.get("schema") != args.schema:
        failures.append(f"schema expected {args.schema!r}, got {data.get('schema')!r}")

    descriptors = {
        item.get("chipKey"): item.get("descriptor", {})
        for item in data.get("descriptors", [])
        if isinstance(item, dict) and item.get("chipKey")
    }
    presets = {
        item.get("id"): item
        for item in data.get("presets", [])
        if isinstance(item, dict) and item.get("id")
    }

    if args.chip_count is not None and len(descriptors) != args.chip_count:
        failures.append(f"chip count expected {args.chip_count}, got {len(descriptors)}")

    for chip in args.chip:
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found")

    for preset in args.preset:
        if preset not in presets:
            failures.append(f"preset {preset!r} not found")

    def split_chip_assertion(assertion: str) -> tuple[str, str, str] | None:
        chip_and_role, _, value = assertion.partition("=")
        chip, _, role = chip_and_role.partition(":")
        if not chip or not role or not value:
            failures.append(f"bad chip assertion {assertion!r}")
            return None
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            return None
        return chip, role, value

    def parameters_for(chip: str) -> dict[str, dict]:
        descriptor = descriptors.get(chip, {})
        return {
            item.get("role"): item
            for item in descriptor.get("parameters", [])
            if isinstance(item, dict) and item.get("role")
        }

    for assertion in args.implemented:
        chip, _, expected_text = assertion.partition("=")
        if not chip or not expected_text:
            failures.append(f"bad implemented assertion {assertion!r}")
            continue
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            continue
        expected = expected_text not in {"0", "false", "False"}
        actual = bool(descriptors[chip].get("implemented", False))
        if actual != expected:
            failures.append(f"{chip}.implemented expected {expected}, got {actual}")

    for assertion in args.param_surface:
        parsed = split_chip_assertion(assertion)
        if parsed is None:
            continue
        chip, role, expected = parsed
        parameters = parameters_for(chip)
        if role not in parameters:
            failures.append(f"{chip} parameter {role!r} not found")
            continue
        actual = parameters[role].get("surface")
        if actual != expected:
            failures.append(f"{chip}.{role}.surface expected {expected!r}, got {actual!r}")

    for assertion in args.param_kind:
        parsed = split_chip_assertion(assertion)
        if parsed is None:
            continue
        chip, role, expected = parsed
        parameters = parameters_for(chip)
        if role not in parameters:
            failures.append(f"{chip} parameter {role!r} not found")
            continue
        actual = parameters[role].get("kind")
        if actual != expected:
            failures.append(f"{chip}.{role}.kind expected {expected!r}, got {actual!r}")

    for assertion in args.param_choices:
        parsed = split_chip_assertion(assertion)
        if parsed is None:
            continue
        chip, role, expected_text = parsed
        parameters = parameters_for(chip)
        if role not in parameters:
            failures.append(f"{chip} parameter {role!r} not found")
            continue
        try:
            expected = int(expected_text)
        except ValueError:
            failures.append(f"bad choice count {assertion!r}")
            continue
        actual = len(parameters[role].get("choices", []))
        if actual != expected:
            failures.append(f"{chip}.{role}.choices expected {expected}, got {actual}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
