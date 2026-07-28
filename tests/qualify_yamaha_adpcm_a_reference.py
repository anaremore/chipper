"""Prove the Yamaha ADPCM-A reference gates reject known-bad mutations."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


COMMON_COMPARE_ARGS = [
    "--per-channel-alignment",
    "--alignment-window-frames",
    "32",
    "--max-accepted-lag-frames",
    "16",
    "--max-length-difference-frames",
    "0",
]


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def expect_rejected(command: list[str], label: str) -> None:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 1:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        raise RuntimeError(
            f"{label}: comparator should reject with threshold status 1, got {result.returncode}"
        )


def render_base_args(renderer: Path, source: Path, chip: str, clock: str, events: Path) -> list[str]:
    return [
        str(renderer),
        "--chip",
        chip,
        "--accuracy",
        "authentic",
        "--macro",
        "manual",
        "--clock",
        clock,
        "--rate",
        "48000",
        "--seconds",
        "0.2",
        "--events",
        str(events),
        "--source1",
        "1",
        "--source2",
        "0",
        "--source3",
        "0",
        "--source4",
        "0",
        "--source5",
        "0",
        "--source6",
        "0",
        "--source7",
        "0",
        "--source8",
        "0",
        "--source9",
        "0",
        "--stereo-spread",
        "0",
        "--output-db",
        "0",
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("renderer", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()

    renderer = args.renderer.resolve()
    source = args.source.resolve()
    binary = args.binary.resolve()
    binary.mkdir(parents=True, exist_ok=True)

    references = source / "tests" / "references"
    events = source / "tests" / "events"
    comparator = source / "tests" / "compare_reference_wav.py"

    opna_bank = (references / "yamaha-adpcm-a-synthetic-opna.bin").read_bytes()
    reversed_nibbles = bytes(((value & 0x0F) << 4) | (value >> 4) for value in opna_bank)
    mutated_opna_bank = binary / "yamaha-adpcm-a-opna-nibble-reversed.bin"
    mutated_opna_bank.write_bytes(reversed_nibbles)
    mutated_opna_wav = binary / "yamaha-adpcm-a-opna-nibble-reversed.wav"
    mutated_opna_json = binary / "yamaha-adpcm-a-opna-nibble-reversed.json"
    run(
        render_base_args(
            renderer,
            source,
            "ym2608",
            "7987200",
            events / "ym2608-adpcm-a-reference-events.txt",
        )
        + [
            "--opna-rhythm-rom",
            str(mutated_opna_bank),
            "--out",
            str(mutated_opna_wav),
            "--debug",
            str(mutated_opna_json),
        ]
    )
    expect_rejected(
        [
            sys.executable,
            str(comparator),
            str(references / "yamaha-adpcm-a-opna-lle.wav"),
            str(mutated_opna_wav),
            "--min-correlation",
            "0.80",
            "--max-normalized-rmse",
            "0.58",
            "--min-rms-ratio",
            "0.97",
            "--max-rms-ratio",
            "1.03",
            "--max-channel-lag-spread-frames",
            "4",
            *COMMON_COMPARE_ARGS,
        ],
        "OPNA reversed-nibble mutation",
    )

    opnb_event_text = (events / "ym2610-adpcm-a-reference-events.txt").read_text(
        encoding="utf-8"
    )
    opnb_event_text = opnb_event_text.replace("write 1 0x115 0x02", "write 1 0x115 0x01")
    opnb_event_text = opnb_event_text.replace("write 1 0x125 0x02", "write 1 0x125 0x01")
    mutated_opnb_events = binary / "ym2610-adpcm-a-page-mutated-events.txt"
    mutated_opnb_events.write_text(opnb_event_text, encoding="utf-8")
    mutated_opnb_wav = binary / "yamaha-adpcm-a-opnb-page-mutated.wav"
    mutated_opnb_json = binary / "yamaha-adpcm-a-opnb-page-mutated.json"
    run(
        render_base_args(renderer, source, "ym2610", "8000000", mutated_opnb_events)
        + [
            "--opnb-adpcm-a-sample",
            str(references / "yamaha-adpcm-a-synthetic-opnb.bin"),
            "--out",
            str(mutated_opnb_wav),
            "--debug",
            str(mutated_opnb_json),
        ]
    )
    expect_rejected(
        [
            sys.executable,
            str(comparator),
            str(references / "yamaha-adpcm-a-opnb-lle.wav"),
            str(mutated_opnb_wav),
            "--min-correlation",
            "0.88",
            "--max-normalized-rmse",
            "0.45",
            "--min-rms-ratio",
            "0.97",
            "--max-rms-ratio",
            "1.04",
            "--max-channel-lag-spread-frames",
            "2",
            *COMMON_COMPARE_ARGS,
        ],
        "OPNB page-register mutation",
    )

    print("YAMAHA_ADPCM_A_REFERENCE_QUALIFIED mutations=2")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
