import argparse
import json
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Assert chipper_render debug JSON properties.")
    parser.add_argument("path")
    parser.add_argument("--implemented-contains")
    parser.add_argument("--verification-badge")
    parser.add_argument("--cycle-accurate", type=int, choices=[0, 1])
    parser.add_argument("--preset")
    parser.add_argument("--macro")
    parser.add_argument("--play-mode")
    parser.add_argument("--output-db", type=float)
    parser.add_argument("--output-gain", type=float)
    parser.add_argument("--stereo-spread", type=float)
    parser.add_argument("--min-peak", type=float)
    parser.add_argument("--max-peak", type=float)
    parser.add_argument("--min-rms", type=float)
    parser.add_argument("--max-rms", type=float)
    parser.add_argument("--min-left-peak", type=float)
    parser.add_argument("--max-left-peak", type=float)
    parser.add_argument("--min-right-peak", type=float)
    parser.add_argument("--max-right-peak", type=float)
    parser.add_argument("--min-left-rms", type=float)
    parser.add_argument("--max-left-rms", type=float)
    parser.add_argument("--min-right-rms", type=float)
    parser.add_argument("--max-right-rms", type=float)
    parser.add_argument("--min-tail-rms", type=float)
    parser.add_argument("--max-tail-rms", type=float)
    parser.add_argument("--min-stereo-rms-delta", type=float)
    parser.add_argument("--register-writes", type=int)
    parser.add_argument("--note-events", type=int)
    parser.add_argument("--min-zero-crossings", type=int)
    parser.add_argument("--max-zero-crossings", type=int)
    parser.add_argument("--core-max", action="append", default=[], metavar="FIELD=VALUE")
    parser.add_argument("--core-min", action="append", default=[], metavar="FIELD=VALUE")
    parser.add_argument("--descriptor-param-kind", action="append", default=[], metavar="ROLE=KIND")
    parser.add_argument("--descriptor-param-surface", action="append", default=[], metavar="ROLE=SURFACE")
    parser.add_argument("--descriptor-param-label", action="append", default=[], metavar="ROLE=LABEL")
    parser.add_argument("--descriptor-param-choices", action="append", default=[], metavar="ROLE=COUNT")
    args = parser.parse_args()

    with open(args.path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    failures: list[str] = []

    if args.implemented_contains and args.implemented_contains not in data.get("implementedAccuracy", ""):
        failures.append(f"implementedAccuracy does not contain {args.implemented_contains!r}: {data.get('implementedAccuracy')!r}")

    if args.verification_badge and data.get("verificationBadge") != args.verification_badge:
        failures.append(f"verificationBadge expected {args.verification_badge!r}, got {data.get('verificationBadge')!r}")

    if args.cycle_accurate is not None:
        expected = args.cycle_accurate == 1
        actual = bool(data.get("cycleAccurate", False))
        if actual != expected:
            failures.append(f"cycleAccurate expected {expected}, got {actual}")

    if args.preset and data.get("preset") != args.preset:
        failures.append(f"preset expected {args.preset!r}, got {data.get('preset')!r}")

    if args.macro and data.get("macro") != args.macro:
        failures.append(f"macro expected {args.macro!r}, got {data.get('macro')!r}")

    if args.play_mode and data.get("playMode") != args.play_mode:
        failures.append(f"playMode expected {args.play_mode!r}, got {data.get('playMode')!r}")

    if args.output_db is not None:
        actual = float(data.get("outputDb", 0.0))
        if abs(actual - args.output_db) > 0.001:
            failures.append(f"outputDb expected {args.output_db}, got {actual}")

    if args.output_gain is not None:
        actual = float(data.get("outputGain", 0.0))
        if abs(actual - args.output_gain) > 0.001:
            failures.append(f"outputGain expected {args.output_gain}, got {actual}")

    if args.stereo_spread is not None:
        actual = float(data.get("stereoSpread", 0.0))
        if abs(actual - args.stereo_spread) > 0.001:
            failures.append(f"stereoSpread expected {args.stereo_spread}, got {actual}")

    if args.min_peak is not None and float(data.get("peak", 0.0)) < args.min_peak:
        failures.append(f"peak expected >= {args.min_peak}, got {data.get('peak')}")

    if args.max_peak is not None and float(data.get("peak", 0.0)) > args.max_peak:
        failures.append(f"peak expected <= {args.max_peak}, got {data.get('peak')}")

    if args.min_rms is not None and float(data.get("rms", 0.0)) < args.min_rms:
        failures.append(f"rms expected >= {args.min_rms}, got {data.get('rms')}")

    if args.max_rms is not None and float(data.get("rms", 0.0)) > args.max_rms:
        failures.append(f"rms expected <= {args.max_rms}, got {data.get('rms')}")

    for field, lower, upper in (
        ("leftPeak", args.min_left_peak, args.max_left_peak),
        ("rightPeak", args.min_right_peak, args.max_right_peak),
        ("leftRms", args.min_left_rms, args.max_left_rms),
        ("rightRms", args.min_right_rms, args.max_right_rms),
        ("tailRms", args.min_tail_rms, args.max_tail_rms),
    ):
        actual = float(data.get(field, 0.0))
        if lower is not None and actual < lower:
            failures.append(f"{field} expected >= {lower}, got {actual}")
        if upper is not None and actual > upper:
            failures.append(f"{field} expected <= {upper}, got {actual}")

    if args.min_stereo_rms_delta is not None:
        actual = abs(float(data.get("leftRms", 0.0)) - float(data.get("rightRms", 0.0)))
        if actual < args.min_stereo_rms_delta:
            failures.append(f"abs(leftRms-rightRms) expected >= {args.min_stereo_rms_delta}, got {actual}")

    if args.register_writes is not None and int(data.get("registerWriteCount", -1)) != args.register_writes:
        failures.append(f"registerWriteCount expected {args.register_writes}, got {data.get('registerWriteCount')}")

    if args.note_events is not None and int(data.get("noteEventCount", -1)) != args.note_events:
        failures.append(f"noteEventCount expected {args.note_events}, got {data.get('noteEventCount')}")

    zero_crossings = int(data.get("zeroCrossings", 0))
    if args.min_zero_crossings is not None and zero_crossings < args.min_zero_crossings:
        failures.append(f"zeroCrossings expected >= {args.min_zero_crossings}, got {zero_crossings}")

    if args.max_zero_crossings is not None and zero_crossings > args.max_zero_crossings:
        failures.append(f"zeroCrossings expected <= {args.max_zero_crossings}, got {zero_crossings}")

    core_state = data.get("coreState", {})
    for assertion in args.core_max:
        field, _, value = assertion.partition("=")
        if not field or not value:
            failures.append(f"bad --core-max assertion {assertion!r}")
            continue
        actual = float(core_state.get(field, 0.0))
        expected = float(value)
        if actual > expected:
            failures.append(f"coreState.{field} expected <= {expected}, got {actual}")

    for assertion in args.core_min:
        field, _, value = assertion.partition("=")
        if not field or not value:
            failures.append(f"bad --core-min assertion {assertion!r}")
            continue
        actual = float(core_state.get(field, 0.0))
        expected = float(value)
        if actual < expected:
            failures.append(f"coreState.{field} expected >= {expected}, got {actual}")

    descriptor = data.get("descriptor", {})
    parameters = {
        item.get("role"): item
        for item in descriptor.get("parameters", [])
        if isinstance(item, dict) and item.get("role")
    }

    def split_descriptor_assertion(assertion: str) -> tuple[str, str] | None:
        role, _, value = assertion.partition("=")
        if not role or not value:
            failures.append(f"bad descriptor assertion {assertion!r}")
            return None
        if role not in parameters:
            failures.append(f"descriptor parameter {role!r} not found")
            return None
        return role, value

    for assertion in args.descriptor_param_kind:
        parsed = split_descriptor_assertion(assertion)
        if parsed is None:
            continue
        role, expected = parsed
        actual = parameters[role].get("kind")
        if actual != expected:
            failures.append(f"descriptor.{role}.kind expected {expected!r}, got {actual!r}")

    for assertion in args.descriptor_param_surface:
        parsed = split_descriptor_assertion(assertion)
        if parsed is None:
            continue
        role, expected = parsed
        actual = parameters[role].get("surface")
        if actual != expected:
            failures.append(f"descriptor.{role}.surface expected {expected!r}, got {actual!r}")

    for assertion in args.descriptor_param_label:
        parsed = split_descriptor_assertion(assertion)
        if parsed is None:
            continue
        role, expected = parsed
        actual = parameters[role].get("label")
        if actual != expected:
            failures.append(f"descriptor.{role}.label expected {expected!r}, got {actual!r}")

    for assertion in args.descriptor_param_choices:
        parsed = split_descriptor_assertion(assertion)
        if parsed is None:
            continue
        role, expected_text = parsed
        try:
            expected = int(expected_text)
        except ValueError:
            failures.append(f"bad descriptor choice count {assertion!r}")
            continue
        actual = len(parameters[role].get("choices", []))
        if actual != expected:
            failures.append(f"descriptor.{role}.choices expected {expected}, got {actual}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
