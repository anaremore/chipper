import argparse
import json
import math
import sys
from pathlib import Path


def load(path: str) -> dict:
    with Path(path).open("r", encoding="utf-8") as handle:
        return json.load(handle)


def expect(failures: list[str], condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assert OPN2 DAC renderer metadata and generated preset differentiation."
    )
    parser.add_argument("--sample", required=True)
    parser.add_argument("--aiff", required=True)
    parser.add_argument("--kick", required=True)
    parser.add_argument("--snare", required=True)
    parser.add_argument("--chord", required=True)
    args = parser.parse_args()

    failures: list[str] = []
    sample = load(args.sample)
    sample_core = sample.get("renderEndCoreState", sample.get("coreState", {}))

    expect(failures, math.isclose(float(sample.get("opn2DacRateHz", 0.0)), 8000.0), "WAV source rate was not retained")
    expect(failures, sample.get("opn2DacRateProvided") is False, "decoded WAV rate should not be marked as a CLI override")
    expect(failures, sample.get("opn2DacRootNote") == 48, "renderer root note metadata mismatch")
    expect(failures, sample.get("opn2DacTrimStart") == 2, "renderer trim start metadata mismatch")
    expect(failures, sample.get("opn2DacTrimEnd") == 14, "renderer trim end metadata mismatch")
    expect(failures, sample.get("opn2DacTailBehavior") == "hold", "renderer tail behavior metadata mismatch")

    expected_core = {
        "dacExternalSampleLoaded": 1,
        "dacExternalSampleBytes": 16,
        "dacExternalSourceRateHz": 8000,
        "dacResolvedSourceRateHz": 8000,
        "dacRootNote": 48,
        "dacTrimStart": 2,
        "dacTrimEnd": 14,
        "dacTriggerSampleBytes": 12,
        "dacSampleSourceUser": 1,
        "dacCurrentSampleSourceUser": 0,
        "dacActive": 0,
        "dacTriggerWriteCount": 12,
        "dacWriteCount": 12,
    }
    for field, expected in expected_core.items():
        expect(failures, sample_core.get(field) == expected, f"sample core {field} expected {expected}, got {sample_core.get(field)!r}")

    expect(failures, sample_core.get("dacTailBehavior") == "hold", f"sample core tail behavior expected hold, got {sample_core.get('dacTailBehavior')!r}")
    expect(
        failures,
        math.isclose(float(sample_core.get("dacEffectivePlaybackRateHz", 0.0)), 8000.0, rel_tol=1e-6),
        f"effective playback rate expected 8000 Hz, got {sample_core.get('dacEffectivePlaybackRateHz')!r}",
    )
    expect(
        failures,
        math.isclose(float(sample_core.get("dacPlaybackStep", 0.0)), 8000.0 / (7670454.0 / 144.0), rel_tol=1e-4),
        f"playback step does not reflect retained 8 kHz source rate: {sample_core.get('dacPlaybackStep')!r}",
    )
    expect(failures, sample_core.get("dacRegister2A") != 128, "hold tail should leave the final trimmed sample value in DAC register $2A")
    expect(
        failures,
        sample_core.get("dacTriggerSampleChecksum") == sample_core.get("dacTriggerWriteChecksum"),
        "trimmed source checksum should match emitted-byte checksum at full velocity",
    )

    aiff = load(args.aiff)
    aiff_core = aiff.get("renderEndCoreState", aiff.get("coreState", {}))
    expect(failures, math.isclose(float(aiff.get("opn2DacRateHz", 0.0)), 8000.0), "AIFF source rate was not retained")
    expect(failures, aiff_core.get("dacExternalSampleBytes") == 16, f"AIFF decoded frame count expected 16, got {aiff_core.get('dacExternalSampleBytes')!r}")
    expect(
        failures,
        aiff_core.get("dacTriggerSampleChecksum") == sample_core.get("dacTriggerSampleChecksum"),
        "equivalent WAV and AIFF fixtures decoded to different trimmed PCM bytes",
    )
    expect(
        failures,
        aiff_core.get("dacTriggerWriteChecksum") == sample_core.get("dacTriggerWriteChecksum"),
        "equivalent WAV and AIFF fixtures emitted different DAC bytes",
    )

    variants = {
        "kick": (load(args.kick), 32, 512),
        "snare": (load(args.snare), 33, 384),
        "chord": (load(args.chord), 47, 448),
    }
    trigger_checksums: set[int] = set()
    write_checksums: set[int] = set()
    for name, (data, key_mask, sample_bytes) in variants.items():
        core = data.get("renderEndCoreState", data.get("coreState", {}))
        expect(failures, core.get("keyOnMask") == key_mask, f"{name} key mask expected {key_mask}, got {core.get('keyOnMask')!r}")
        expect(failures, core.get("dacTriggerSampleBytes") == sample_bytes, f"{name} generated sample length expected {sample_bytes}, got {core.get('dacTriggerSampleBytes')!r}")
        expect(failures, int(core.get("dacTriggerWriteCount", 0)) > 0, f"{name} emitted no DAC writes")
        trigger_checksums.add(int(core.get("dacTriggerSampleChecksum", 0)))
        write_checksums.add(int(core.get("dacTriggerWriteChecksum", 0)))

    expect(failures, len(trigger_checksums) == 3 and 0 not in trigger_checksums, f"generated DAC source checksums are not pairwise distinct: {trigger_checksums}")
    expect(failures, len(write_checksums) == 3 and 0 not in write_checksums, f"generated DAC emitted checksums are not pairwise distinct: {write_checksums}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
