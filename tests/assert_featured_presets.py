"""Render the curated bank across notes, velocities, sustain, and release.

This checks gross audibility/headroom and reports tail metrics. It is not a
substitute for musical listening. One-shot presets may correctly have no tail.
"""
import argparse
import array
import json
import math
from pathlib import Path
import subprocess
import sys
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer', required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    catalog = args.work_dir / 'catalog.json'
    subprocess.run([args.renderer, '--list-descriptors', '--debug', str(catalog)], check=True, capture_output=True)
    presets = [p for p in json.loads(catalog.read_text())['presets'] if p['featured']]
    assert len(presets) == 16 and len({p['id'] for p in presets}) == 16, 'Featured bank contract changed'
    assert len({p['role'] for p in presets}) >= 5, 'Featured bank lost role diversity'
    reports, failures = [], []
    for preset in presets:
        # Match the register-aware musical range to the intended role.
        notes = (36, 48, 60) if preset['role'] == 'Bass' else (48, 60, 72)
        for note in notes:
            for velocity in (0.35, 1.0):
                name = f"{preset['id']}-{note}-{int(velocity * 100)}"
                events = args.work_dir / f'{name}.txt'
                events.write_text(f'note 0 {note} {velocity} 72000\n')
                wav = args.work_dir / f'{name}.wav'
                debug = args.work_dir / f'{name}.json'
                subprocess.run([args.renderer, '--preset', preset['id'], '--events', str(events),
                                '--rate', '48000', '--seconds', '2', '--out', str(wav), '--debug', str(debug)],
                               check=True, capture_output=True)
                data = json.loads(debug.read_text())
                with wave.open(str(wav), 'rb') as source:
                    assert source.getsampwidth() == 2 and source.getnframes() == 96000
                    samples = array.array('h', source.readframes(96000))
                    if sys.byteorder != 'little': samples.byteswap()
                    channels = source.getnchannels()
                rms = lambda start, end: math.sqrt(sum((x / 32768) ** 2 for x in samples[start*channels:end*channels]) / ((end-start)*channels))
                peak = data['peak']
                if not math.isfinite(peak) or not 0.0005 < peak < 0.98:
                    failures.append(f'{name}: peak {peak} outside audition bounds')
                if not math.isfinite(data['rms']) or data['rms'] <= 0:
                    failures.append(f'{name}: invalid RMS')
                reports.append(dict(id=preset['id'], note=note, velocity=velocity, peak=peak,
                                    attackRms=rms(0, 4800), heldRms=rms(48000, 72000), releaseRms=rms(72000, 96000)))
    (args.work_dir / 'report.json').write_text(json.dumps(reports, indent=2) + '\n')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'{len(presets)} featured sounds passed {len(reports)} two-second note/velocity auditions; tail metrics and WAVs retained.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
