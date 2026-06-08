import argparse
import json
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Assert chipper_render descriptor-list JSON properties.")
    parser.add_argument("path")
    parser.add_argument("--schema", default="chipper.descriptors.v1")
    parser.add_argument("--chip-count", type=int)
    parser.add_argument("--chip", action="append", default=[], metavar="KEY")
    parser.add_argument("--preset-count", action="append", default=[], metavar="KEY=COUNT")
    parser.add_argument("--implemented", action="append", default=[], metavar="KEY=0|1")
    parser.add_argument("--verification-badge", action="append", default=[], metavar="KEY=BADGE")
    parser.add_argument("--verification-cycle", action="append", default=[], metavar="KEY=0|1")
    parser.add_argument("--verification-hardware", action="append", default=[], metavar="KEY=0|1")
    parser.add_argument("--param-surface", action="append", default=[], metavar="KEY:ROLE=SURFACE")
    parser.add_argument("--param-kind", action="append", default=[], metavar="KEY:ROLE=KIND")
    parser.add_argument("--param-choices", action="append", default=[], metavar="KEY:ROLE=COUNT")
    parser.add_argument("--param-midi-cc", action="append", default=[], metavar="KEY:ROLE=CC")
    parser.add_argument("--source-lanes", action="append", default=[], metavar="KEY=VISIBLE/NATIVE")
    parser.add_argument("--midi-cc-count", type=int)
    parser.add_argument("--midi-cc", action="append", default=[], metavar="CC=PARAMETER_ID")
    parser.add_argument("--macro-label", action="append", default=[], metavar="KEY:MACRO=LABEL")
    parser.add_argument("--macro-source", action="append", default=[], metavar="KEY:MACRO=1010")
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
    midi_cc_mappings = {
        int(item.get("cc")): item
        for item in data.get("midiCcMappings", [])
        if isinstance(item, dict) and isinstance(item.get("cc"), int)
    }

    if args.chip_count is not None and len(descriptors) != args.chip_count:
        failures.append(f"chip count expected {args.chip_count}, got {len(descriptors)}")

    for chip in args.chip:
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found")

    for chip, descriptor in descriptors.items():
        descriptor_presets = descriptor.get("presets", [])
        if not isinstance(descriptor_presets, list):
            failures.append(f"{chip}.presets expected a list, got {type(descriptor_presets).__name__}")
            continue
        actual = descriptor.get("presetCount")
        if actual != len(descriptor_presets):
            failures.append(f"{chip}.presetCount expected {len(descriptor_presets)}, got {actual!r}")

    for assertion in args.preset_count:
        chip, _, expected_text = assertion.partition("=")
        if not chip or not expected_text:
            failures.append(f"bad preset count assertion {assertion!r}")
            continue
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            continue
        try:
            expected = int(expected_text)
        except ValueError:
            failures.append(f"bad preset count value {assertion!r}")
            continue
        actual = descriptors[chip].get("presetCount")
        if actual != expected:
            failures.append(f"{chip}.presetCount expected {expected}, got {actual!r}")

    for assertion in args.source_lanes:
        chip, _, expected_text = assertion.partition("=")
        if not chip or not expected_text:
            failures.append(f"bad source lanes assertion {assertion!r}")
            continue
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            continue
        visible_text, _, native_text = expected_text.partition("/")
        if not visible_text or not native_text:
            failures.append(f"bad source lanes value {assertion!r}")
            continue
        try:
            expected_visible = int(visible_text)
            expected_native = int(native_text)
        except ValueError:
            failures.append(f"bad source lanes count {assertion!r}")
            continue
        source_lanes = descriptors[chip].get("sourceLanes", {})
        actual_visible = source_lanes.get("visible")
        actual_native = source_lanes.get("native")
        actual_internal = bool(source_lanes.get("hasInternal", False))
        expected_internal = expected_native > expected_visible
        if actual_visible != expected_visible or actual_native != expected_native:
            failures.append(
                f"{chip}.sourceLanes expected {expected_visible}/{expected_native}, "
                f"got {actual_visible!r}/{actual_native!r}"
            )
        if actual_internal != expected_internal:
            failures.append(f"{chip}.sourceLanes.hasInternal expected {expected_internal}, got {actual_internal}")

    for preset in args.preset:
        if preset not in presets:
            failures.append(f"preset {preset!r} not found")

    if args.midi_cc_count is not None and len(midi_cc_mappings) != args.midi_cc_count:
        failures.append(f"MIDI CC count expected {args.midi_cc_count}, got {len(midi_cc_mappings)}")

    for assertion in args.midi_cc:
        cc_text, _, expected = assertion.partition("=")
        if not cc_text or not expected:
            failures.append(f"bad MIDI CC assertion {assertion!r}")
            continue
        try:
            cc = int(cc_text)
        except ValueError:
            failures.append(f"bad MIDI CC value {assertion!r}")
            continue
        if cc not in midi_cc_mappings:
            failures.append(f"MIDI CC {cc} not found")
            continue
        actual = midi_cc_mappings[cc].get("parameterId")
        if actual != expected:
            failures.append(f"MIDI CC {cc} expected {expected!r}, got {actual!r}")

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

    def macros_for(chip: str) -> dict[str, dict]:
        descriptor = descriptors.get(chip, {})
        return {
            item.get("kind"): item
            for item in descriptor.get("macros", [])
            if isinstance(item, dict) and item.get("kind")
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

    for assertion in args.verification_badge:
        chip, _, expected = assertion.partition("=")
        if not chip or not expected:
            failures.append(f"bad verification badge assertion {assertion!r}")
            continue
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            continue
        actual = descriptors[chip].get("verification", {}).get("badge")
        if actual != expected:
            failures.append(f"{chip}.verification.badge expected {expected!r}, got {actual!r}")

    for argument_name, assertions, field_name in (
        ("verification cycle", args.verification_cycle, "cycleAccurate"),
        ("verification hardware", args.verification_hardware, "hardwareValidated"),
    ):
        for assertion in assertions:
            chip, _, expected_text = assertion.partition("=")
            if not chip or not expected_text:
                failures.append(f"bad {argument_name} assertion {assertion!r}")
                continue
            if chip not in descriptors:
                failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
                continue
            expected = expected_text not in {"0", "false", "False"}
            actual = bool(descriptors[chip].get("verification", {}).get(field_name, False))
            if actual != expected:
                failures.append(f"{chip}.verification.{field_name} expected {expected}, got {actual}")

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

    for assertion in args.param_midi_cc:
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
            failures.append(f"bad MIDI CC value {assertion!r}")
            continue
        actual = parameters[role].get("midiCc")
        if actual != expected:
            failures.append(f"{chip}.{role}.midiCc expected {expected}, got {actual}")

    for assertion in args.macro_label:
        chip_and_macro, _, expected = assertion.partition("=")
        chip, _, macro = chip_and_macro.partition(":")
        if not chip or not macro or not expected:
            failures.append(f"bad macro label assertion {assertion!r}")
            continue
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            continue
        macros = macros_for(chip)
        if macro not in macros:
            failures.append(f"{chip} macro {macro!r} not found")
            continue
        actual = macros[macro].get("label")
        if actual != expected:
            failures.append(f"{chip}.{macro}.label expected {expected!r}, got {actual!r}")

    for assertion in args.macro_source:
        chip_and_macro, _, expected = assertion.partition("=")
        chip, _, macro = chip_and_macro.partition(":")
        if not chip or not macro or not expected:
            failures.append(f"bad macro source assertion {assertion!r}")
            continue
        if chip not in descriptors:
            failures.append(f"chip {chip!r} not found for assertion {assertion!r}")
            continue
        macros = macros_for(chip)
        if macro not in macros:
            failures.append(f"{chip} macro {macro!r} not found")
            continue
        actual_values = macros[macro].get("sourceEnabled", [])
        actual = "".join("1" if bool(value) else "0" for value in actual_values)
        if actual != expected:
            failures.append(f"{chip}.{macro}.sourceEnabled expected {expected!r}, got {actual!r}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
