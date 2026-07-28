import argparse
import json
import math
import subprocess
import sys
from pathlib import Path


CLOCK_HZ = 7_670_454.0
SOURCE_RATE_HZ = 48_000.0
SOURCE_BYTES = bytes((0x80, 0x80, 0x80, 0x80, 0xC0, 0xC0, 0xFF, 0xFF, 0x40, 0x40))
TRIM_START = 2
TRIM_END = 8


def fnv1a(values: bytes) -> int:
    checksum = 2_166_136_261
    for value in values:
        checksum ^= value
        checksum = (checksum * 16_777_619) & 0xFFFFFFFF
    return checksum


def render(renderer: Path, workdir: Path, host_rate: int, tail: str) -> dict:
    stem = f"opn2-dac-timing-{host_rate}-{tail}"
    debug_path = workdir / f"{stem}.json"
    wav_path = workdir / f"{stem}.wav"
    hex_bytes = "".join(f"{value:02x}" for value in SOURCE_BYTES)
    command = [
        str(renderer),
        "--chip",
        "ym2612",
        "--accuracy",
        "authentic",
        "--macro",
        "drum",
        "--opn2-dac",
        "dac",
        "--opn2-dac-hex",
        hex_bytes,
        "--opn2-dac-rate",
        str(int(SOURCE_RATE_HZ)),
        "--opn2-dac-root",
        "48",
        "--opn2-dac-trim-start",
        str(TRIM_START),
        "--opn2-dac-trim-end",
        str(TRIM_END),
        "--opn2-dac-tail",
        tail,
        "--source1",
        "0",
        "--source2",
        "0",
        "--source3",
        "0",
        "--source4",
        "0",
        "--source5",
        "0",
        "--source6",
        "1",
        "--clock",
        str(int(CLOCK_HZ)),
        "--rate",
        str(host_rate),
        "--seconds",
        "0.02",
        "--note",
        "48",
        "--out",
        str(wav_path),
        "--debug",
        str(debug_path),
    ]
    subprocess.run(command, cwd=workdir, check=True)
    with debug_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def render_legacy_raw(renderer: Path, workdir: Path) -> dict:
    raw_path = workdir / "opn2-dac-timing-legacy.raw"
    raw_path.write_bytes(SOURCE_BYTES)
    debug_path = workdir / "opn2-dac-timing-legacy.json"
    wav_path = workdir / "opn2-dac-timing-legacy.wav"
    command = [
        str(renderer),
        "--chip", "ym2612",
        "--accuracy", "authentic",
        "--macro", "drum",
        "--opn2-dac", "dac",
        "--opn2-dac-sample", str(raw_path),
        "--opn2-dac-root", "48",
        "--opn2-dac-trim-start", str(TRIM_START),
        "--opn2-dac-trim-end", str(TRIM_END),
        "--opn2-dac-tail", "hold",
        "--source1", "0",
        "--source2", "0",
        "--source3", "0",
        "--source4", "0",
        "--source5", "0",
        "--source6", "1",
        "--clock", str(int(CLOCK_HZ)),
        "--rate", "48000",
        "--seconds", "0.02",
        "--note", "48",
        "--out", str(wav_path),
        "--debug", str(debug_path),
    ]
    subprocess.run(command, cwd=workdir, check=True)
    with debug_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Qualify OPN2 DAC source-rate timing, byte scheduling, trim, and tail behavior."
    )
    parser.add_argument("--renderer", required=True)
    parser.add_argument("--workdir", required=True)
    args = parser.parse_args()

    renderer = Path(args.renderer).resolve()
    workdir = Path(args.workdir).resolve()
    workdir.mkdir(parents=True, exist_ok=True)

    runs = {
        (44_100, "hold"): render(renderer, workdir, 44_100, "hold"),
        (96_000, "hold"): render(renderer, workdir, 96_000, "hold"),
        (44_100, "center"): render(renderer, workdir, 44_100, "center"),
        (96_000, "center"): render(renderer, workdir, 96_000, "center"),
    }
    legacy_raw = render_legacy_raw(renderer, workdir)

    failures: list[str] = []
    trimmed = SOURCE_BYTES[TRIM_START:TRIM_END]
    expected_sample_checksum = fnv1a(trimmed)
    expected_hold_checksum = expected_sample_checksum
    expected_center_checksum = fnv1a(trimmed + bytes((0x80,)))
    expected_step = SOURCE_RATE_HZ / (CLOCK_HZ / 144.0)

    for (host_rate, tail), data in runs.items():
        core = data.get("renderEndCoreState", data.get("coreState", {}))
        prefix = f"{host_rate} Hz {tail}"

        expected_top_level = {
            "opn2DacRateHz": SOURCE_RATE_HZ,
            "opn2DacRateProvided": True,
            "opn2DacRootNote": 48,
            "opn2DacTrimStart": TRIM_START,
            "opn2DacTrimEnd": TRIM_END,
            "opn2DacTailBehavior": tail,
        }
        for field, expected in expected_top_level.items():
            if data.get(field) != expected:
                failures.append(f"{prefix}: renderer {field} expected {expected!r}, got {data.get(field)!r}")

        expected_write_count = len(trimmed) + (tail == "center")
        expected_write_checksum = expected_center_checksum if tail == "center" else expected_hold_checksum
        expected_core = {
            "dacExternalSourceRateHz": SOURCE_RATE_HZ,
            "dacResolvedSourceRateHz": SOURCE_RATE_HZ,
            "dacRootNote": 48,
            "dacTrimStart": TRIM_START,
            "dacTrimEnd": TRIM_END,
            "dacTailBehavior": tail,
            "dacTriggerSampleBytes": len(trimmed),
            "dacTriggerSampleChecksum": expected_sample_checksum,
            "dacTriggerWriteCount": expected_write_count,
            "dacTriggerWriteChecksum": expected_write_checksum,
            "dacWriteCount": expected_write_count,
            "dacWriteChecksum": expected_write_checksum,
            "dacSampleSourceUser": 1,
            "dacCurrentSampleSourceUser": 0,
            "dacActive": 0,
        }
        for field, expected in expected_core.items():
            if core.get(field) != expected:
                failures.append(f"{prefix}: core {field} expected {expected!r}, got {core.get(field)!r}")

        if not math.isclose(float(core.get("dacEffectivePlaybackRateHz", 0.0)), SOURCE_RATE_HZ, rel_tol=1e-7):
            failures.append(f"{prefix}: effective playback rate expected {SOURCE_RATE_HZ}, got {core.get('dacEffectivePlaybackRateHz')!r}")
        if not math.isclose(float(core.get("dacPlaybackStep", 0.0)), expected_step, rel_tol=1e-5):
            failures.append(f"{prefix}: playback step expected {expected_step}, got {core.get('dacPlaybackStep')!r}")

        expected_last_value = 0x80 if tail == "center" else trimmed[-1]
        if core.get("dacRegister2A") != expected_last_value:
            failures.append(f"{prefix}: DAC register $2A expected {expected_last_value}, got {core.get('dacRegister2A')!r}")

    for tail in ("hold", "center"):
        slow_data = runs[(44_100, tail)]
        fast_data = runs[(96_000, tail)]
        slow_core = slow_data.get("renderEndCoreState", slow_data.get("coreState", {}))
        fast_core = fast_data.get("renderEndCoreState", fast_data.get("coreState", {}))
        for field in ("dacTriggerWriteCount", "dacTriggerWriteChecksum", "dacWriteCount", "dacWriteChecksum"):
            if slow_core.get(field) != fast_core.get(field):
                failures.append(
                    f"{tail}: host-rate invariant {field} differs: "
                    f"44.1 kHz={slow_core.get(field)!r}, 96 kHz={fast_core.get(field)!r}"
                )

    legacy_core = legacy_raw.get("renderEndCoreState", legacy_raw.get("coreState", {}))
    chip_rate = CLOCK_HZ / 144.0
    legacy_expectations = {
        "dacExternalSourceRateHz": 0,
        "dacResolvedSourceRateHz": chip_rate,
        "dacEffectivePlaybackRateHz": chip_rate,
        "dacPlaybackStep": 1,
        "dacTriggerSampleBytes": len(trimmed),
        "dacTriggerSampleChecksum": expected_sample_checksum,
        "dacTriggerWriteCount": len(trimmed),
        "dacTriggerWriteChecksum": expected_hold_checksum,
    }
    if legacy_raw.get("opn2DacRateHz") != 0 or legacy_raw.get("opn2DacRateProvided") is not False:
        failures.append("legacy raw input did not preserve the sourceRateHz=0 native-rate contract")
    for field, expected in legacy_expectations.items():
        actual = legacy_core.get(field)
        if isinstance(expected, float):
            if not math.isclose(float(actual or 0.0), expected, rel_tol=1e-5):
                failures.append(f"legacy raw: {field} expected {expected!r}, got {actual!r}")
        elif actual != expected:
            failures.append(f"legacy raw: {field} expected {expected!r}, got {actual!r}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
