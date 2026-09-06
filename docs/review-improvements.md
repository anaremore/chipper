# September review improvements

This implementation follows the [September 6 project review](reviews/2026-09-06-project-review.md). The review remains a historical snapshot. This pass focuses on everyday host reliability, a smaller build surface, and discovery within the existing 27 chips and 401 presets.

## Audio and project recall

- Mono output is the equal-weight average of the two native chip outputs before output limiting. Centered audio retains its level; a right-only source remains audible. Stereo routing is unchanged. `chipper_vst3_host_state_smoke` checks Both/Left/Right routing against the built VST3 at 64, 128, and 256 frames.
- Project saves capture parameters, registers, wave/motion state, and sample metadata under the callback lock. Sample snapshots share immutable byte storage. Checksums, XML, and base64 encoding run after releasing the callback lock. Existing sample formats, bounds, checksums, and fallback rules remain intact.
- `chipper_processor_state_concurrency_smoke` saves an 8 MiB bank while acquiring the same lock as the VST3 callback at 64/128/256 frames, verifies audio progress and finite output, checks payload sizes, and recalls each snapshot. Its relative lock-occupancy gate tolerates slower runners while detecting serialization that monopolizes the audio lock. Reported times are measurements, not a hard real-time guarantee.

## Native YM2149 Motion

In YM2149 Motion Lab each step has **Noise Period**: Preset, or native register values 0-31. Lower values make the shared noise generator run faster. Enable Noise and route it through the A/B/C source cards to hear the change. `ym-noise-hat` is a useful starting sound.

Period changes occur at the same sample boundaries as pitch/level/gate. Hold changes noise color without retriggering the envelope. Preset restores the current underlying period, including edits made while Motion was active. Source routing and levels remain in their owning cards. The destination is only visible on YM2149 and retains the existing Big Mono / Chip Poly bypass policy.

Schema 10 stores `yn0` through `yn7` in Motion patterns: 0 means Preset; 1-32 encode native period 0-31. Earlier states default to Preset. Out-of-range values or native fields on another chip fail before changing the processor. Host parameter IDs and MIDI CC assignments do not change. Transient noise overrides are excluded from exported base registers.

The native processor gate covers register mapping, exact timing, base-value restoration, no envelope retrigger, migration, round trip, and invalid-state rejection. The allocation gate crosses native Motion boundaries. Editor tests exercise the control at both widths and verify chip-local visibility.

## Discovery and readability

The header gives preset names more width, removes inactive Strictness controls, and retains the frozen parameter and recall attachment. The footer tooltip explains its reserved status. FM register readouts use a larger minimum text scale with full details available in tooltips.

Browse > Featured selects [16 existing sounds](featured-sounds.md), with playing tips. A dedicated gate renders each at three role-appropriate notes and two velocities, with 1.5 seconds held and 0.5 seconds released. It checks audibility/headroom and retains attack, held, and release RMS plus WAVs in `featured-auditions`. One-shot sounds may correctly have silent tails. This editorial bank has automated audition evidence; human musical listening remains a release activity.

## Structure and integration

- Concrete chip implementations live in `Source/Engine/Cores`; the public factory and mode parsing remain in `ChipCore.cpp`.
- Project serialization and immutable asset ownership live in `Source/State`. Compatibility parsing remains in the processor for a later bounded extraction.
- CMake registrations are divided into engine, renderer assertions, plugin, discovery/reference, and YM2149 family files. All 888 registered commands and properties were compared before and after the family extraction. Plugin tools share compiled processor/editor/JUCE code. An explicit dependency orders generated JUCE headers before test compilation.
- Build metadata is isolated in `BuildInfo.cpp`, so a timestamp update does not recompile the editor.
- Python 3.10+ is required with `CHIPPER_BUILD_TESTS=ON`. `scripts/verify-project.ps1` configures, builds all targets, and runs the full suite. CI uses that same command on Windows, Linux, and macOS. Sanitizers include state contention and native Motion.
- The CI aggregate **Required checks** only succeeds when every platform and sanitizer job passes. Work should use a short-lived PR branch and pass this check before merging into `main`.

## Independent evidence

Additional pinned Ayumi fixtures isolate YM2149 channels B and C. The generator reproduces the original A fixture byte-for-byte; B/C intentionally render the same waveform through different register/channel routes. They use the existing correlation/error/gain/lag limits without widening them. This expands independent channel-routing evidence, not noise-generator, envelope, analog, or hardware accuracy claims. Native noise timing has deterministic internal tests; independent noise/hardware comparison remains future accuracy work.

## Release boundary

Local verification on Windows used a fresh `build-review-release` directory and the shared verification script: **888/888 CTests passed**, including the 96 longer Featured auditions, in 52.99 seconds. Pluginval 1.0.4 passed strictness 5 with GUI tests skipped. Compact YM2149 Motion and YM2151 editor captures were visually checked. A subsequent 8 MiB save run measured maximum callback-lock waits of 0.06-0.10 ms; these values are observations from that machine.

The first macOS run built successfully and passed 887 gates. The save test's minimum callback-count assumption was replaced by an overlap check plus the existing lock-occupancy bound after the runner scheduled five callbacks during a save with negligible lock waiting. The corrected gate passes locally; every required platform job must pass before integration.

Automated host, renderer, UI, and sanitizer checks complement manual DAW use. Hardware captures, a complete screen-reader audit, and human musical listening are not claimed by this change. Independent FM channel patches and more chip families remain separate product work; the chosen feature here is native YM2149 noise Motion.
