import argparse
import json
import math
import pathlib
import shutil
import subprocess
import sys


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, text=True, capture_output=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Render every factory preset and assert each one is audible.")
    parser.add_argument("--renderer", required=True, help="Path to chipper_render executable.")
    parser.add_argument("--work-dir", required=True, help="Directory for generated WAV/debug JSON files.")
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--seconds", type=float, default=0.05)
    parser.add_argument("--note", type=int, default=69)
    parser.add_argument("--min-peak", type=float, default=0.005)
    parser.add_argument("--max-peak", type=float, default=0.98)
    args = parser.parse_args()

    renderer = pathlib.Path(args.renderer)
    work_dir = pathlib.Path(args.work_dir)
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    descriptor_json = work_dir / "factory-preset-catalog.json"
    listed = run([str(renderer), "--list-descriptors", "--debug", str(descriptor_json)])
    if listed.returncode != 0:
        sys.stderr.write(listed.stdout)
        sys.stderr.write(listed.stderr)
        return listed.returncode

    with descriptor_json.open("r", encoding="utf-8") as handle:
        catalog = json.load(handle)

    presets = catalog.get("presets", [])
    failures: list[str] = []
    seen: set[str] = set()

    if not presets:
        failures.append("factory preset catalog is empty")

    for preset in presets:
        preset_id = preset.get("id")
        if not preset_id:
            failures.append(f"preset entry missing id: {preset!r}")
            continue
        if preset_id in seen:
            failures.append(f"duplicate preset id {preset_id!r}")
            continue
        seen.add(preset_id)

        wav_path = work_dir / f"{preset_id}.wav"
        debug_path = work_dir / f"{preset_id}.json"
        rendered = run([
            str(renderer),
            "--preset",
            preset_id,
            "--rate",
            str(args.rate),
            "--seconds",
            str(args.seconds),
            "--note",
            str(args.note),
            "--out",
            str(wav_path),
            "--debug",
            str(debug_path),
        ])
        if rendered.returncode != 0:
            failures.append(f"{preset_id}: render failed\n{rendered.stdout}{rendered.stderr}")
            continue
        if not debug_path.exists():
            failures.append(f"{preset_id}: debug JSON was not written")
            continue
        if not wav_path.exists() or wav_path.stat().st_size <= 44:
            failures.append(f"{preset_id}: WAV was not written or contains no audio data")
            continue

        with debug_path.open("r", encoding="utf-8") as handle:
            debug = json.load(handle)

        peak = float(debug.get("peak", 0.0))
        rms = float(debug.get("rms", 0.0))
        rendered_samples = int(debug.get("renderedSamples", 0))
        output_db = float(debug.get("outputDb", 0.0))
        if debug.get("preset") != preset_id:
            failures.append(f"{preset_id}: debug preset id is {debug.get('preset')!r}")
        if "partial" not in str(debug.get("implementedAccuracy", "")).lower():
            failures.append(f"{preset_id}: implemented accuracy did not report partial clean-room status")
        if rendered_samples <= 0:
            failures.append(f"{preset_id}: renderedSamples is {rendered_samples}")
        if not math.isfinite(output_db):
            failures.append(f"{preset_id}: outputDb is not finite")
        if peak < args.min_peak:
            failures.append(f"{preset_id}: peak {peak:.6f} is below minimum {args.min_peak:.6f}")
        if peak > args.max_peak:
            failures.append(f"{preset_id}: peak {peak:.6f} exceeds maximum {args.max_peak:.6f}")
        if rms <= 0.0:
            failures.append(f"{preset_id}: rms {rms:.6f} is not positive")

    if failures:
        sys.stderr.write("factory preset audibility failures:\n")
        for failure in failures:
            sys.stderr.write(f"  - {failure}\n")
        return 1

    print(f"Rendered {len(seen)} factory presets; all are audible.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
