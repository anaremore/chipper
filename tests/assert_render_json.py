import argparse
import json
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Assert chipper_render debug JSON properties.")
    parser.add_argument("path")
    parser.add_argument("--implemented-contains")
    parser.add_argument("--macro")
    parser.add_argument("--play-mode")
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
    parser.add_argument("--register-writes", type=int)
    parser.add_argument("--note-events", type=int)
    parser.add_argument("--min-zero-crossings", type=int)
    parser.add_argument("--max-zero-crossings", type=int)
    parser.add_argument("--core-max", action="append", default=[], metavar="FIELD=VALUE")
    parser.add_argument("--core-min", action="append", default=[], metavar="FIELD=VALUE")
    args = parser.parse_args()

    with open(args.path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    failures: list[str] = []

    if args.implemented_contains and args.implemented_contains not in data.get("implementedAccuracy", ""):
        failures.append(f"implementedAccuracy does not contain {args.implemented_contains!r}: {data.get('implementedAccuracy')!r}")

    if args.macro and data.get("macro") != args.macro:
        failures.append(f"macro expected {args.macro!r}, got {data.get('macro')!r}")

    if args.play_mode and data.get("playMode") != args.play_mode:
        failures.append(f"playMode expected {args.play_mode!r}, got {data.get('playMode')!r}")

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
    ):
        actual = float(data.get(field, 0.0))
        if lower is not None and actual < lower:
            failures.append(f"{field} expected >= {lower}, got {actual}")
        if upper is not None and actual > upper:
            failures.append(f"{field} expected <= {upper}, got {actual}")

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

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
