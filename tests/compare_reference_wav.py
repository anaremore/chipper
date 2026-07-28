"""Compare a Chipper render with an independently sourced reference WAV.

The comparator preserves channels, optionally aligns a bounded capture offset,
and reports duration drift separately from gain-matched waveform error. A render
cannot hide swapped stereo channels, a truncated tail, or a bad waveform behind
mono downmixing or normalization. The accepted lag and length bounds remain
explicit release-gate choices.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import wave
from pathlib import Path


def _decode_pcm(raw: bytes, sample_width: int) -> list[float]:
    if sample_width == 1:
        return [(value - 128) / 128.0 for value in raw]
    if sample_width == 2:
        return [value / 32768.0 for value in struct.unpack(f"<{len(raw) // 2}h", raw)]
    if sample_width == 3:
        values: list[float] = []
        for offset in range(0, len(raw), 3):
            packed = int.from_bytes(raw[offset : offset + 3], "little", signed=False)
            if packed & 0x800000:
                packed -= 0x1000000
            values.append(packed / 8388608.0)
        return values
    if sample_width == 4:
        return [value / 2147483648.0 for value in struct.unpack(f"<{len(raw) // 4}i", raw)]
    raise ValueError(f"unsupported PCM sample width: {sample_width * 8} bits")


def read_wav(path: Path) -> tuple[int, list[list[float]]]:
    with wave.open(str(path), "rb") as source:
        if source.getcomptype() != "NONE":
            raise ValueError(f"{path}: compressed WAV is not supported")
        channel_count = source.getnchannels()
        rate = source.getframerate()
        values = _decode_pcm(source.readframes(source.getnframes()), source.getsampwidth())

    channels = [values[channel::channel_count] for channel in range(channel_count)]
    return rate, channels


def _compare_at_lag(reference: list[float], candidate: list[float], lag_samples: int) -> dict[str, float | int]:
    reference_start = max(0, -lag_samples)
    candidate_start = max(0, lag_samples)
    sample_count = min(len(reference) - reference_start, len(candidate) - candidate_start)
    if sample_count <= 0:
        raise ValueError("reference and candidate do not have overlapping audio samples")
    aligned_reference = reference[reference_start : reference_start + sample_count]
    aligned_candidate = candidate[candidate_start : candidate_start + sample_count]

    reference_energy = sum(value * value for value in aligned_reference)
    candidate_energy = sum(value * value for value in aligned_candidate)
    reference_is_silent = reference_energy <= 1.0e-18
    candidate_is_silent = candidate_energy <= 1.0e-18
    if reference_is_silent or candidate_is_silent:
        if reference_is_silent and candidate_is_silent:
            return {
                "sampleCount": sample_count,
                "correlation": 1.0,
                "gainMatchedNormalizedRmse": 0.0,
                "candidateGainForBestMatch": 1.0,
                "referenceRms": 0.0,
                "candidateRms": 0.0,
                "rmsRatio": 1.0,
            }
        raise ValueError("reference and candidate must have matching silent/non-silent channels")

    dot = sum(left * right for left, right in zip(aligned_reference, aligned_candidate))
    correlation = dot / math.sqrt(reference_energy * candidate_energy)
    gain = dot / candidate_energy
    matched_error = sum((left - gain * right) ** 2 for left, right in zip(aligned_reference, aligned_candidate))
    normalized_rmse = math.sqrt(matched_error / reference_energy)
    reference_rms = math.sqrt(reference_energy / sample_count)
    candidate_rms = math.sqrt(candidate_energy / sample_count)

    return {
        "sampleCount": sample_count,
        "correlation": max(-1.0, min(1.0, correlation)),
        "gainMatchedNormalizedRmse": normalized_rmse,
        "candidateGainForBestMatch": gain,
        "referenceRms": reference_rms,
        "candidateRms": candidate_rms,
        "rmsRatio": candidate_rms / reference_rms,
    }


def _validate_channels(channels: list[list[float]], label: str) -> int:
    if not channels:
        raise ValueError(f"{label} must contain at least one audio channel")
    frame_count = len(channels[0])
    if frame_count == 0:
        raise ValueError(f"{label} must contain audio frames")
    if any(len(channel) != frame_count for channel in channels):
        raise ValueError(f"{label} channels must have equal frame counts")
    return frame_count


def _best_alignment(
    reference_channels: list[list[float]],
    candidate_channels: list[list[float]],
    max_lag_samples: int,
    min_overlap_samples: int,
) -> tuple[int, list[dict[str, float | int]]]:
    if max_lag_samples < 0:
        raise ValueError("max_lag_samples must be non-negative")
    if min_overlap_samples < 1:
        raise ValueError("min_overlap_samples must be positive")

    best: tuple[
        tuple[float, float, int, int],
        int,
        list[dict[str, float | int]],
    ] | None = None
    for lag in range(-max_lag_samples, max_lag_samples + 1):
        try:
            channel_metrics = [
                _compare_at_lag(reference, candidate, lag)
                for reference, candidate in zip(reference_channels, candidate_channels)
            ]
        except ValueError:
            continue
        if min(metric["sampleCount"] for metric in channel_metrics) < min_overlap_samples:
            continue
        correlations = [float(metric["correlation"]) for metric in channel_metrics]
        mean_correlation = sum(correlations) / len(correlations)
        ranking = (min(correlations), mean_correlation, -abs(lag), -lag)
        if best is None or ranking > best[0]:
            best = (ranking, lag, channel_metrics)

    if best is None:
        raise ValueError("no valid alignment exists inside the requested lag and overlap bounds")
    return best[1], best[2]


def compare(
    reference: list[float],
    candidate: list[float],
    *,
    max_lag_samples: int = 0,
    min_overlap_samples: int = 1,
) -> dict[str, float | int]:
    lag, channel_metrics = _best_alignment(
        [reference], [candidate], max_lag_samples=max_lag_samples, min_overlap_samples=min_overlap_samples
    )
    metrics = dict(channel_metrics[0])
    metrics.update(
        {
            "lagSamples": lag,
            "referenceSampleCount": len(reference),
            "candidateSampleCount": len(candidate),
            "lengthDifferenceSamples": len(candidate) - len(reference),
        }
    )
    return metrics


def compare_audio(
    reference_channels: list[list[float]],
    candidate_channels: list[list[float]],
    *,
    max_lag_samples: int = 0,
    min_overlap_samples: int = 1,
    per_channel_alignment: bool = False,
) -> dict[str, object]:
    reference_frames = _validate_channels(reference_channels, "reference")
    candidate_frames = _validate_channels(candidate_channels, "candidate")
    if len(reference_channels) != len(candidate_channels):
        raise ValueError(
            f"channel-count mismatch: reference={len(reference_channels)}, candidate={len(candidate_channels)}"
        )
    if per_channel_alignment:
        lags: list[int] = []
        channel_metrics: list[dict[str, float | int]] = []
        for reference, candidate in zip(reference_channels, candidate_channels):
            lag, metrics = _best_alignment(
                [reference],
                [candidate],
                max_lag_samples=max_lag_samples,
                min_overlap_samples=min_overlap_samples,
            )
            lags.append(lag)
            channel_metrics.append(metrics[0])
    else:
        lag, channel_metrics = _best_alignment(
            reference_channels,
            candidate_channels,
            max_lag_samples=max_lag_samples,
            min_overlap_samples=min_overlap_samples,
        )
        lags = [lag] * len(channel_metrics)

    result = {
        "channelCount": len(reference_channels),
        "referenceFrameCount": reference_frames,
        "candidateFrameCount": candidate_frames,
        "lengthDifferenceFrames": candidate_frames - reference_frames,
        "lagFrames": max(lags, key=abs),
        "lagFramesPerChannel": lags,
        "lagSpreadFrames": max(lags) - min(lags),
        "alignmentMode": "per-channel" if per_channel_alignment else "shared",
        "channels": channel_metrics,
    }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare a render against an independent reference WAV")
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--min-correlation", type=float, default=0.995)
    parser.add_argument("--max-normalized-rmse", type=float, default=0.10)
    parser.add_argument("--min-rms-ratio", type=float, default=0.5)
    parser.add_argument("--max-rms-ratio", type=float, default=2.0)
    parser.add_argument("--alignment-window-frames", type=int, default=256)
    parser.add_argument("--max-accepted-lag-frames", type=int, default=256)
    parser.add_argument("--max-length-difference-frames", type=int, default=0)
    parser.add_argument("--min-overlap-frames", type=int, default=64)
    parser.add_argument("--per-channel-alignment", action="store_true")
    parser.add_argument("--max-channel-lag-spread-frames", type=int, default=0)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    try:
        reference_rate, reference_channels = read_wav(args.reference)
        candidate_rate, candidate_channels = read_wav(args.candidate)
        if reference_rate != candidate_rate:
            raise ValueError(f"sample-rate mismatch: reference={reference_rate}, candidate={candidate_rate}")
        metrics = compare_audio(
            reference_channels,
            candidate_channels,
            max_lag_samples=args.alignment_window_frames,
            min_overlap_samples=args.min_overlap_frames,
            per_channel_alignment=args.per_channel_alignment,
        )
    except (ValueError, wave.Error, OSError) as error:
        print(f"reference comparison failed: {error}")
        return 2

    metrics["sampleRate"] = reference_rate
    encoded = json.dumps(metrics, indent=2, sort_keys=True)
    print(encoded)
    if args.json is not None:
        args.json.write_text(encoded + "\n", encoding="utf-8")

    failures: list[str] = []
    if abs(int(metrics["lagFrames"])) > args.max_accepted_lag_frames:
        failures.append(f"absolute lag {abs(int(metrics['lagFrames']))} > {args.max_accepted_lag_frames} frames")
    if int(metrics["lagSpreadFrames"]) > args.max_channel_lag_spread_frames:
        failures.append(
            f"channel lag spread {metrics['lagSpreadFrames']} > "
            f"{args.max_channel_lag_spread_frames} frames"
        )
    if abs(int(metrics["lengthDifferenceFrames"])) > args.max_length_difference_frames:
        failures.append(
            f"absolute length difference {abs(int(metrics['lengthDifferenceFrames']))} "
            f"> {args.max_length_difference_frames} frames"
        )
    for channel_index, channel in enumerate(metrics["channels"], start=1):
        if channel["correlation"] < args.min_correlation:
            failures.append(
                f"channel {channel_index} correlation {channel['correlation']:.6f} < {args.min_correlation:.6f}"
            )
        if channel["gainMatchedNormalizedRmse"] > args.max_normalized_rmse:
            failures.append(
                f"channel {channel_index} gain-matched normalized RMSE "
                f"{channel['gainMatchedNormalizedRmse']:.6f} > {args.max_normalized_rmse:.6f}"
            )
        if not args.min_rms_ratio <= channel["rmsRatio"] <= args.max_rms_ratio:
            failures.append(
                f"channel {channel_index} RMS ratio {channel['rmsRatio']:.6f} outside "
                f"[{args.min_rms_ratio:.6f}, {args.max_rms_ratio:.6f}]"
            )
    if failures:
        print("reference comparison failed: " + "; ".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
