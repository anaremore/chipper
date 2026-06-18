---
name: chipper-juce-ui
description: Use when modifying Chipper's JUCE plugin editor, LookAndFeel, chip-specific layouts, APVTS parameter attachments, custom controls, preset-visible UI state, or UI tests. Applies to chip-aware VST UI work where controls must reflect authentic or truthfully partial chip behavior rather than generic web/frontend patterns.
---

# Chipper JUCE UI

## Working Rules

- Keep `PluginEditor` thin. Prefer reusable helpers, small Components, descriptor metadata, and LookAndFeel tokens over growing one-off layout branches.
- Bind visible controls through APVTS parameters and attachments. Do not mutate DSP state directly from UI code.
- Keep expensive work out of `paint()`. Precompute labels, paths, wave previews, and sample summaries when state changes.
- Preserve chip ownership. Put pulse duty under pulse voices, noise mode under noise voices, sample/wave selection under sampler or wavetable voices, operator controls under FM operators, and shared hardware in shared modules.
- Use continuous controls only for continuous behavior or deliberate performance macros. Use segmented or stepped controls for hardware choices, register enums, bitfields, algorithms, waveforms, duty cycles, rhythm modes, and clock/divider choices.
- Keep UI honest. Strictness is a behavior preference; verification strength belongs in footer/status/docs. Do not label a mode cycle-accurate without test evidence.
- Preserve readability over decorative retro styling. The app can look like a tracker, but labels, dropdowns, numeric boxes, level lanes, and waveform previews must remain readable at the default editor size.
- Keep visual theme values tokenized by chip. Prefer theme structs, palette helpers, and shared drawing primitives over hard-coded colors scattered through layout code.
- Keep HiDPI and host restore behavior in mind. Default to the 1240 x 820 DAW-fit contract unless a deliberate fixed-size redesign changes the smoke tests and docs together.

## Chipper-Specific Control Rules

- NES/RP2A03: Pulse duty is four-position register behavior, not a continuous pulse-width slider. DMC controls are sample/bank/rate/loop controls, not oscillator controls.
- Game Boy/DMG: Pulse 1 and Pulse 2 duty are independent four-position controls in their source cards. Wave RAM shape/level belongs in the Wave source card. Noise width/mode belongs in the Noise source card.
- SID/C64: Each voice owns waveform, pulse width, level, and ADSR. Filter cutoff, resonance, mode, and voice routing stay together in the Filter module. Sync and ring are explicit oscillator-interaction choices.
- PSG chips: Tone/noise and attenuation choices should read as hardware mixer/register behavior, with modern helpers labeled as helpers.
- FM chips: Do not present YM2612/OPL/YM2151 as subtractive synths. Show algorithms, operators, feedback, envelopes, rhythm/DAC modes, and routing as FM structure.
- Sampler/wavetable chips: Paula, SPC700, HuC6280, Namco WSG, SCC, and DMG Wave need source-card sample/wave selectors, visible levels, waveform previews where practical, and clear missing-asset states.

## Layout Review Checklist

- Identify controls that are confusing, clipped, overlapping, hidden, or visually detached from the sound source they affect.
- Identify fake-continuous controls that should be segmented or stepped.
- Identify source cards that are too tall, too short, or have dead space between selector and level controls.
- Identify places where a roadmap placeholder is visible but not engine-backed.
- Identify chip-specific controls that should move from shared panels into source cards.
- Prefer one shippable vertical slice: change the affected chip layout, add or tighten a smoke-test guard, update the owning doc, build/test/install when code changes, then commit and push.

## Verification

- Run `git diff --check` for every change.
- For UI code changes, build `Chipper_VST3` and run `chipper_editor_size_smoke`, `chipper_processor_midi_cc_smoke`, and `chipper_parameter_midi_cc_smoke`.
- When touching samples, verify loop vs one-shot behavior and missing-file UI states.
- When touching FM behavior, run the held-tail/preset sustain checks before claiming no regression.
- Install from the same build root after code changes so the VST3 footer/marker reflects the committed build.
