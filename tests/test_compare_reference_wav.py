import math
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_reference_wav import compare, compare_audio, read_wav  # noqa: E402


class ReferenceComparatorTests(unittest.TestCase):
    def test_identical_shape_with_level_change(self) -> None:
        reference = [math.sin(index * 0.1) for index in range(512)]
        metrics = compare(reference, [sample * 0.5 for sample in reference])
        self.assertAlmostEqual(metrics["correlation"], 1.0, places=12)
        self.assertLess(metrics["gainMatchedNormalizedRmse"], 1.0e-12)
        self.assertAlmostEqual(metrics["rmsRatio"], 0.5, places=12)
        self.assertEqual(metrics["lagSamples"], 0)

    def test_wrong_waveform_is_detected(self) -> None:
        reference = [math.sin(index * 0.1) for index in range(512)]
        candidate = [1.0 if sample >= 0.0 else -1.0 for sample in reference]
        metrics = compare(reference, candidate)
        self.assertLess(metrics["correlation"], 0.95)
        self.assertGreater(metrics["gainMatchedNormalizedRmse"], 0.25)

    def test_bounded_alignment_reports_capture_lag_and_length(self) -> None:
        reference = [
            math.sin(index * 0.131) + 0.31 * math.sin(index * 0.017) + (0.75 if index == 43 else 0.0)
            for index in range(512)
        ]
        candidate = [0.0] * 7 + reference + [0.0] * 3
        metrics = compare(reference, candidate, max_lag_samples=16, min_overlap_samples=256)
        self.assertEqual(metrics["lagSamples"], 7)
        self.assertEqual(metrics["lengthDifferenceSamples"], 10)
        self.assertAlmostEqual(metrics["correlation"], 1.0, places=12)

    def test_stereo_channel_swap_is_not_hidden_by_downmix(self) -> None:
        left = [math.sin(index * 0.071) for index in range(512)]
        right = [math.sin(index * 0.193 + 0.4) for index in range(512)]
        metrics = compare_audio([left, right], [right, left])
        correlations = [channel["correlation"] for channel in metrics["channels"]]
        self.assertLess(min(correlations), 0.25)

    def test_shared_alignment_maximizes_the_weakest_stereo_channel(self) -> None:
        left = [math.sin(index * 0.31) for index in range(512)]
        right = [math.sin(index * 0.23 + 0.7) for index in range(512)]
        candidate_left = [0.0] * 5 + left + [0.0] * 4
        candidate_right = [0.0] * 9 + right
        metrics = compare_audio(
            [left, right], [candidate_left, candidate_right],
            max_lag_samples=12, min_overlap_samples=400,
        )
        correlations = [channel["correlation"] for channel in metrics["channels"]]
        self.assertGreater(min(correlations), 0.75)
        self.assertIn(metrics["lagFrames"], range(5, 10))

    def test_bounded_per_channel_alignment_reports_slot_stagger(self) -> None:
        left = [math.sin(index * 0.31) for index in range(512)]
        right = [math.sin(index * 0.23 + 0.7) for index in range(512)]
        metrics = compare_audio(
            [left, right],
            [[0.0] * 5 + left + [0.0] * 4, [0.0] * 9 + right],
            max_lag_samples=12,
            min_overlap_samples=400,
            per_channel_alignment=True,
        )
        self.assertEqual(metrics["lagFramesPerChannel"], [5, 9])
        self.assertEqual(metrics["lagSpreadFrames"], 4)
        self.assertTrue(all(channel["correlation"] > 0.999 for channel in metrics["channels"]))

    def test_channel_count_mismatch_fails_explicitly(self) -> None:
        signal = [math.sin(index * 0.1) for index in range(128)]
        with self.assertRaisesRegex(ValueError, "channel-count mismatch"):
            compare_audio([signal], [signal, signal])

    def test_matching_silent_channels_are_supported(self) -> None:
        silence = [0.0] * 128
        metrics = compare_audio([silence], [silence])
        self.assertEqual(metrics["channels"][0]["correlation"], 1.0)
        self.assertEqual(metrics["channels"][0]["rmsRatio"], 1.0)

    def test_wav_reader_preserves_stereo_channels(self) -> None:
        interleaved = [-32768, 32767, -16384, 16384, 0, 8192]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stereo.wav"
            with wave.open(str(path), "wb") as output:
                output.setnchannels(2)
                output.setsampwidth(2)
                output.setframerate(48000)
                output.writeframes(struct.pack("<6h", *interleaved))

            rate, channels = read_wav(path)

        self.assertEqual(rate, 48000)
        self.assertEqual(len(channels), 2)
        self.assertEqual([len(channel) for channel in channels], [3, 3])
        self.assertAlmostEqual(channels[0][0], -1.0)
        self.assertAlmostEqual(channels[1][0], 32767 / 32768.0)


if __name__ == "__main__":
    unittest.main()
