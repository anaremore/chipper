"""Validate provenance metadata and integrity for an audio reference fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import wave
from pathlib import Path


def fail(message: str) -> int:
    print(f"reference metadata validation failed: {message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metadata", type=Path)
    args = parser.parse_args()

    try:
        metadata_path = args.metadata.resolve(strict=True)
        data = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return fail(str(error))

    if data.get("schemaVersion") != 1:
        return fail("schemaVersion must be 1")

    for section in ("fixture", "reference", "trace", "candidate", "comparison", "scope"):
        if not isinstance(data.get(section), dict):
            return fail(f"missing object section: {section}")

    fixture = data["fixture"]
    fixture_name = fixture.get("file")
    if not isinstance(fixture_name, str) or not fixture_name:
        return fail("fixture.file must be a non-empty relative path")

    fixture_path = (metadata_path.parent / fixture_name).resolve()
    try:
        fixture_path.relative_to(metadata_path.parent)
    except ValueError:
        return fail("fixture.file must remain inside the metadata directory")
    if not fixture_path.is_file():
        return fail(f"fixture does not exist: {fixture_path}")

    expected_sha256 = fixture.get("sha256")
    if not isinstance(expected_sha256, str) or re.fullmatch(r"[0-9a-f]{64}", expected_sha256) is None:
        return fail("fixture.sha256 must be a lowercase SHA-256 digest")
    actual_sha256 = hashlib.sha256(fixture_path.read_bytes()).hexdigest()
    if actual_sha256 != expected_sha256:
        return fail(f"SHA-256 mismatch: expected {expected_sha256}, got {actual_sha256}")

    try:
        with wave.open(str(fixture_path), "rb") as source:
            observed = {
                "sampleRate": source.getframerate(),
                "channelCount": source.getnchannels(),
                "frameCount": source.getnframes(),
                "sampleWidthBits": source.getsampwidth() * 8,
                "compression": source.getcomptype(),
            }
    except (OSError, wave.Error) as error:
        return fail(f"invalid WAV fixture: {error}")

    expected_audio = {
        "sampleRate": fixture.get("sampleRate"),
        "channelCount": fixture.get("channelCount"),
        "frameCount": fixture.get("frameCount"),
        "sampleWidthBits": 16 if fixture.get("encoding") == "PCM16LE" else None,
        "compression": "NONE",
    }
    if observed != expected_audio:
        return fail(f"WAV metadata mismatch: expected {expected_audio}, got {observed}")

    duration = fixture.get("durationSeconds")
    if not isinstance(duration, (int, float)) or isinstance(duration, bool):
        return fail("fixture.durationSeconds must be numeric")
    expected_duration = observed["frameCount"] / observed["sampleRate"]
    if abs(float(duration) - expected_duration) > 1.0e-9:
        return fail(
            f"fixture.durationSeconds mismatch: expected {expected_duration}, got {duration}"
        )

    reference = data["reference"]
    if reference.get("kind") not in {"hardware-capture", "trusted-emulator"}:
        return fail("reference.kind must identify hardware-capture or trusted-emulator provenance")
    if not all(isinstance(reference.get(field), str) and reference[field] for field in ("engine", "repository", "commit", "license", "generator")):
        return fail("reference provenance must include engine, repository, commit, license, and generator")
    if re.fullmatch(r"[0-9a-f]{40}", reference["commit"]) is None:
        return fail("reference.commit must be a full lowercase Git commit")
    if reference.get("generatedFromCopyrightedMusicOrGameData") is not False:
        return fail("reference must explicitly state that no copyrighted music or game data was used")

    generator_path = (metadata_path.parent / reference["generator"]).resolve()
    try:
        generator_path.relative_to(metadata_path.parent)
    except ValueError:
        return fail("reference.generator must remain inside the reference directory")
    if not generator_path.is_file():
        return fail(f"reference generator does not exist: {generator_path}")

    trace = data["trace"]
    event_file = trace.get("eventFile")
    if not isinstance(event_file, str) or not event_file:
        return fail("trace.eventFile must be a non-empty relative path")
    event_path = (metadata_path.parent / event_file).resolve()
    try:
        event_path.relative_to(metadata_path.parent.parent)
    except ValueError:
        return fail("trace.eventFile must remain inside the tests directory")
    if not event_path.is_file():
        return fail(f"reference event trace does not exist: {event_path}")

    if not isinstance(data["candidate"].get("command"), str) or not data["candidate"]["command"]:
        return fail("candidate.command must record the reproduction command")
    if not isinstance(data["comparison"].get("command"), str) or not data["comparison"]["command"]:
        return fail("comparison.command must record the accepted comparison command")

    thresholds = data["comparison"].get("thresholds")
    if not isinstance(thresholds, dict):
        return fail("comparison.thresholds must be an object")
    accepted_lag = thresholds.get("maxAcceptedLagFrames")
    alignment_window = thresholds.get("alignmentWindowFrames")
    if not isinstance(accepted_lag, int) or accepted_lag < 0:
        return fail("maxAcceptedLagFrames must be a non-negative integer")
    if not isinstance(alignment_window, int) or alignment_window < accepted_lag:
        return fail("alignmentWindowFrames must cover maxAcceptedLagFrames")
    if not -1.0 <= float(thresholds.get("minCorrelation", -2.0)) <= 1.0:
        return fail("minCorrelation must be inside [-1, 1]")
    if float(thresholds.get("maxGainMatchedNormalizedRmse", -1.0)) < 0.0:
        return fail("maxGainMatchedNormalizedRmse must be non-negative")
    minimum_rms = float(thresholds.get("minRmsRatio", -1.0))
    maximum_rms = float(thresholds.get("maxRmsRatio", -1.0))
    if minimum_rms < 0.0 or maximum_rms < minimum_rms:
        return fail("RMS ratio bounds are invalid")
    if int(thresholds.get("maxLengthDifferenceFrames", -1)) < 0:
        return fail("maxLengthDifferenceFrames must be non-negative")

    baseline = data["comparison"].get("acceptanceBaseline")
    if not isinstance(baseline, dict):
        return fail("comparison.acceptanceBaseline must be an object")
    baseline_checks = (
        (
            float(baseline.get("correlationPerChannel", -2.0))
            >= float(thresholds["minCorrelation"]),
            "baseline correlation is outside the accepted threshold",
        ),
        (
            float(baseline.get("gainMatchedNormalizedRmsePerChannel", float("inf")))
            <= float(thresholds["maxGainMatchedNormalizedRmse"]),
            "baseline normalized RMSE is outside the accepted threshold",
        ),
        (
            minimum_rms
            <= float(baseline.get("candidateToReferenceRmsRatioPerChannel", -1.0))
            <= maximum_rms,
            "baseline RMS ratio is outside the accepted threshold",
        ),
        (
            abs(int(baseline.get("lagFrames", accepted_lag + 1))) <= accepted_lag,
            "baseline lag is outside the accepted threshold",
        ),
        (
            abs(int(baseline.get("lengthDifferenceFrames", -1)))
            <= int(thresholds["maxLengthDifferenceFrames"]),
            "baseline length difference is outside the accepted threshold",
        ),
    )
    for condition, message in baseline_checks:
        if not condition:
            return fail(message)

    scope = data["scope"]
    if not isinstance(scope.get("covers"), list) or not scope["covers"]:
        return fail("scope.covers must state at least one covered behavior")
    if not isinstance(scope.get("doesNotClaim"), list) or not scope["doesNotClaim"]:
        return fail("scope.doesNotClaim must state the accuracy boundary")

    print(
        "REFERENCE_METADATA"
        f" fixture={fixture_path.name}"
        f" sha256={actual_sha256}"
        f" frames={observed['frameCount']}"
        f" rate={observed['sampleRate']}"
        f" channels={observed['channelCount']}"
        f" source={reference['engine']}@{reference['commit']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
