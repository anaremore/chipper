# Chipper project review — 2026-09-06

Reviewed source: `a88b8a90ef` (`codex/chipper-ui-overhaul`). This is a review snapshot; the existing priority roadmap remains the owner of ongoing work.

## Overall assessment

Chipper has a strong foundation for an ambitious instrument: 27 chip modes, 401 factory presets, a coherent chip-aware interface, unusually explicit accuracy claims, and substantial automated verification. The next milestone should emphasize reliable everyday DAW use and depth in existing instruments. Another broad UI redesign or more chip modes would offer less value than closing the concrete reliability gaps below.

The existing suite passes, but an additional binary-host probe found a mono-output defect that the suite does not cover. A green suite is useful regression evidence, not proof that every supported host configuration works.

## What is working well

- **Product identity and layout.** Channels, operators, samples, and shared hardware have recognizable ownership. Fresh NES, Paula, and OPL3 captures show consistent colors, visible source levels, waveform previews, and a stable outer workflow. Wave Lab, Motion Lab, and the FM editor provide real sound-authoring depth.
- **Compatibility discipline.** Stable parameter IDs, the explicit bus contract, schema-v9 migrations, invalid-state rejection, and bounded/checksummed sample fallback protect user projects. The VST3 save/reopen test exercises the built plugin rather than only processor internals.
- **Regression infrastructure.** The renderer produces WAV and machine-readable state. Tests cover register behavior, held-note regressions, sample playback, presets, UI geometry, state recall, allocations, and callback deadlines. All 879 registered tests passed in this review.
- **Honest verification claims.** The project distinguishes partial implementations, same-engine regression checks, independent emulator references, and hardware evidence. Accepted Ayumi and YM2608-LLE fixtures include provenance and bounded comparison criteria.
- **Release groundwork.** CI describes Windows, Linux, macOS, sanitizer, preset, and Windows pluginval checks. Release packaging includes checksums and deliberate publishing. The remaining issue is consistently exercising and enforcing those safeguards.

## Findings and recommendations

### 1. High priority: mono output discards right-routed audio

`Source/PluginProcessor.cpp:3410` accepts mono and stereo buses. In `renderRange` at line 3477, channel 0 receives only `frame.left`; `frame.right` is written only when a second output channel exists. A sound routed entirely right therefore disappears in an accepted mono layout.

An independent, temporary JUCE host probe loaded the freshly built VST3, selected Game Boy / DMG, rendered MIDI note 69 at velocity 0.8 for 64 blocks of 256 samples at 48 kHz, and negotiated each output layout before preparing playback:

| Route | Output layout | Left/mono peak | Right peak |
| --- | --- | ---: | ---: |
| Both | Stereo | 0.111019 | 0.111019 |
| Both | Mono | 0.111019 | — |
| Left | Stereo | 0.111019 | 0 |
| Left | Mono | 0.111019 | — |
| Right | Stereo | 0 | 0.111019 |
| Right | Mono | 0 | — |

**Recommendation:** define the mono mix explicitly, combine both chip outputs with a documented headroom policy, and add binary-host regressions for left-only, right-only, and centered sounds. Preserve current stereo behavior. The existing processor test checks whether mono is accepted but does not test the resulting mono audio.

### 2. High priority: shorten the audio lock held during project saves

`createStateXml` takes `getCallbackLock()` at `Source/PluginProcessor.cpp:5025` and retains it throughout state construction. Embedded sample serialization invokes `addEmbeddedSamplePayload` at line 1062, which copies bytes, computes a checksum, and encodes base64 while that lock remains held. The project permits up to 16 MiB of embedded sample data.

The JUCE VST3 wrapper takes that same lock around processing. Large project saves can therefore block playback. This is a source-confirmed contention path; dropout frequency and worst-case lock duration were not measured in this review. JUCE's [AudioProcessor documentation](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html) explicitly cautions that long callback-lock holds can cause stuttering.

**Recommendation:** capture a consistent snapshot with a short lock, retain immutable sample buffers safely, then serialize and encode outside the callback lock. Add a concurrent save/playback stress test with loaded banks, state recalls, and 64/128/256-sample buffers. The existing performance test measures direct `processBlock` calls at 48 kHz / 256 samples after warming the chip pool; it does not measure time blocked entering the VST3 callback during a save.

### 3. Medium priority: reduce the cost and scope of routine changes

Physical line counts at the reviewed revision:

| File | Lines |
| --- | ---: |
| `Source/PluginEditor.cpp` | 17,757 |
| `Source/Engine/ChipCore.cpp` | 17,056 |
| `Source/Presets.cpp` | 8,788 |
| `Source/Engine/ChipDescriptors.cpp` | 7,353 |
| `Source/PluginProcessor.cpp` | 6,406 |
| `tests/editor_size_smoke.cpp` | 5,835 |
| `CMakeLists.txt` | 4,174 |
| `tests/processor_midi_cc_smoke.cpp` | 4,174 |

These files concentrate unrelated chip behavior and make small edits expensive to understand. Existing `Source/UI`, `Source/State`, codec, bank, motion, and wavetable modules are a good extraction pattern. Several test targets compile the entire plugin source list separately. `cmake/WriteBuildInfo.cmake` also rewrites a timestamp-bearing header every build, and this review's incremental build recompiled `PluginEditor.cpp` repeatedly across targets.

**Recommendation:** extract one chip-family layout or asset-state service at a time under the existing tests. Split the chip implementations and test registration by family. Isolate build metadata in a small translation unit, and share compatible compiled objects where compile definitions permit. Avoid a wholesale rewrite.

### 4. Medium priority: turn release infrastructure into a dependable integration gate

At review start, `main` was 60 commits behind the working branch, there were no open pull requests, `main` was unprotected, and GitHub reported zero check runs for `a88b8a9`. The workflow runs on pull requests, pushes to `main`, and manual dispatches; long-lived branch pushes without a PR therefore bypass it. The available successful remote runs were for older revisions.

**Recommendation:** use short-lived branches with PR checks, require the relevant checks on `main`, and merge smaller changes regularly. Retain the deliberate release workflow. Do not describe configured cross-platform jobs as verified for a revision until their runs complete successfully. Branch-protection settings were inspected, not changed, during this review.

### 5. Medium priority: make clean-checkout verification reproducible

Python is optional in `CMakeLists.txt:10`; a large assertion block is conditional at line 1331. Without Python, CTest can register a smaller suite that omits important validation. Python is also absent from the README's build requirements. The README and local release instructions build only `Chipper_VST3` and `chipper_render` before asking users to run the full CTest suite, which does not build its missing test executables.

**Recommendation:** require Python when tests are enabled, document it, and provide one clean-checkout configure/build/test path that builds all test targets. Use a real test-build option and split the long CMake test definitions. Check the documented commands in CI so existing build artifacts cannot conceal missing steps.

### 6. Product polish: prioritize sound discovery and deeper editing

The compact captures preserve the main source controls, but the header truncates preset names and spends space on a reserved Strictness control. Some operator metadata is very small. These are focused polish opportunities, not evidence that every chip page needs redesigning.

**Recommendation:** simplify the header while retaining the frozen host parameter and recall behavior; expose inactive Strictness information unobtrusively; improve preset-name and operator-detail legibility. Audition and curate a small featured bank before increasing preset count. Automated 50 ms renders establish basic audibility, not musical quality across notes, velocities, and sustained playback.

For the next feature, choose one bounded existing-family improvement: native duty/wave/noise Motion destinations, or independent FM channel patches. Both build on delivered editors. Keep new chip families and multi-output routing behind compatibility and host-test work. Expand independent references for the chosen family instead of treating more internal assertions as hardware-accuracy evidence.

## Suggested order

1. Fix mono routing and add the missing binary-host coverage.
2. Measure and remove long save-time callback locks; expand low-buffer concurrency coverage.
3. Establish required PR checks and a reliable clean-checkout build/test command.
4. Make one targeted structural extraction while implementing a useful existing-chip improvement.
5. Curate presets and expand independent reference evidence before the next release candidate.

## Verification performed

- Full Windows **RelWithDebInfo** build: `cmake --build build-codex --config RelWithDebInfo --parallel 2` — passed.
- Full CTest suite: `ctest --test-dir build-codex -C RelWithDebInfo --output-on-failure --parallel 1` — **879/879 passed**, 94.69 seconds.
- The suite included the binary VST3 state test, real-time performance test, editor geometry and snapshot tests, independent-reference gates, and **401/401 factory-preset audibility** checks across **27 modes**.
- Additional NES screenshots at 1180 and 1240 px and Paula at 1180 px were generated. NES/Paula compact captures and the fresh OPL3 four-operator compact capture were visually inspected.
- Additional compiled-VST3 mono probe — reproduced the defect above. Probe code and outputs are retained under ignored `build-codex/review-*` paths; production code and existing tests were not altered.
- Git branch ancestry, clean working tree, remote branches, open PRs, branch protection, and available check runs were inspected.

Not performed here: a fresh Release-configuration build, a new pluginval run, Linux/macOS builds, sanitizer execution, manual DAW sessions, hardware captures, a complete screen-reader audit, or musical listening tests. Prior documented results are not substituted for these checks.
