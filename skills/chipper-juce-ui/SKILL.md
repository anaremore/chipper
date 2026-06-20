---
name: chipper-juce-ui
description: Use when modifying Chipper's JUCE plugin editor, LookAndFeel, chip-specific layouts, APVTS parameter attachments, custom controls, preset-visible UI state, or UI tests. Applies to chip-aware VST UI work where controls must reflect authentic or truthfully partial chip behavior rather than generic web/frontend patterns. Use this as the authority for Chipper UI decisions; use generic design/frontend skills only as optional inspiration.
---

# Chipper JUCE UI

## Scope And Design Source Of Truth

- Treat this skill as the decision-making authority for Chipper plugin UI work.
- Use general UI/design skills only for inspiration: spacing, hierarchy, accessibility, contrast, typography, and interaction clarity.
- Resolve implementation decisions through Chipper's actual constraints: JUCE Components, APVTS attachments, host automation, MIDI CC mapping, LookAndFeel tokens, fixed/default editor size, and chip-native behavior.
- Do not translate web patterns directly into the plugin. React-style layout assumptions, DOM-like state, CSS overflow fixes, and web animation habits are advisory at most.
- Prefer a custom Chipper component or descriptor rule when a generic UI rule conflicts with chip ownership, register behavior, DAW host behavior, or audio-thread safety.
- The final question for any control is: does this make the chip easier to play and understand without lying about what the engine is doing?

## Using Other Design Skills

- Load broad design/frontend skills only when visual inspiration or critique is useful.
- Translate any useful advice back into JUCE terms: `Component` bounds, `LookAndFeel` drawing, APVTS attachments, parameter metadata, smoke tests, and host automation behavior.
- Reject web-first fixes that depend on scrolling pages, CSS overflow, DOM reflow, browser font metrics, or client-side state patterns that do not map cleanly to a DAW plugin editor.
- Prefer a small Chipper-specific component over a generic abstraction when the control represents real chip behavior: duty bits, waveform masks, operator algorithms, sample slots, loop points, or mixer routing.

## Working Rules

- Keep `PluginEditor` thin. Prefer reusable helpers, small Components, descriptor metadata, and LookAndFeel tokens over growing one-off layout branches.
- Bind visible controls through APVTS parameters and attachments. Do not mutate DSP state directly from UI code.
- Keep expensive work out of `paint()`. Precompute labels, paths, wave previews, and sample summaries when state changes.
- Preserve chip ownership. Put pulse duty under pulse voices, noise mode under noise voices, sample/wave selection under sampler or wavetable voices, operator controls under FM operators, and shared hardware in shared modules.
- Use continuous controls only for continuous behavior or deliberate performance macros. Use segmented or stepped controls for hardware choices, register enums, bitfields, algorithms, waveforms, duty cycles, rhythm modes, and clock/divider choices.
- Keep UI honest. Strictness is a behavior preference; verification strength belongs in footer/status/docs. Do not label a mode cycle-accurate without test evidence.
- Preserve readability over decorative retro styling. The app can look like a tracker, but labels, dropdowns, numeric boxes, level lanes, and waveform previews must remain readable at the default editor size.
- Keep visual theme values tokenized by chip. Prefer theme structs, palette helpers, and shared drawing primitives over hard-coded colors scattered through layout code.
- Keep HiDPI and host restore behavior in mind. Default to the 1240 x 820 DAW-fit contract and the 1180 px compact-width floor unless a deliberate fixed-size redesign changes the smoke tests and docs together.
- Do not solve cramped panels by making the editor taller. Rebalance rows, shorten copy, move secondary details into tooltips/status text, or split source cards into denser reusable controls.
- Standardize input sizes. ComboBoxes, TextEditors, and meaningful buttons should use the shared default height; only tiny segmented buttons may go smaller, and only when their labels remain readable.
- Keep source-card level controls visible. If a card owns a sound source, its waveform/sample selector, active state, and level lane must be visible without relying on clipped text or hidden rows. Sampler and wavetable cards should treat a readable Level lane as non-negotiable, not as leftover space.
- Keep performance macros secondary. They should never overlap source panels, sample banks, native envelopes, or output scopes. SID ADSR in particular must remain clear of Performance Macros at default and compact widths.

## Chipper-Specific Control Rules

- NES/RP2A03: Pulse duty is four-position register behavior, not a continuous pulse-width slider. DMC controls are sample/bank/rate/loop controls, not oscillator controls.
- Game Boy/DMG: Pulse 1 and Pulse 2 duty are independent four-position controls in their source cards. Wave RAM shape/level belongs in the Wave source card. Noise width/mode belongs in the Noise source card.
- YM2149/AY: A/B/C tone-noise mixer overrides are per-channel register choices and belong in the A/B/C source cards. Shared noise pitch and hardware-envelope timing may stay in global/performance areas.
- SID/C64: Each voice owns waveform, pulse width, level, and ADSR. Filter cutoff, resonance, mode, and voice routing stay together in the Filter module. Sync and ring are explicit oscillator-interaction choices.
- PSG chips: Tone/noise and attenuation choices should read as hardware mixer/register behavior, with modern helpers labeled as helpers.
- FM chips: Do not present YM2612/OPL/YM2151 as subtractive synths. Show algorithms, operators, feedback, envelopes, rhythm/DAC modes, and routing as FM structure.
- Sampler/wavetable chips: Paula, SPC700, HuC6280, Namco WSG, SCC, and DMG Wave need source-card sample/wave selectors, visible levels, waveform previews where practical, and clear missing-asset states.
- Native envelope terminology matters. SID, SPC700, YM2612/OPN2, YM2151/OPM, OPL2/OPL3, and YM2413/OPLL may expose native envelope controls. Paula, Namco WSG, SCC, HuC6280, POKEY, SN76489, NES, and DMG helper volume shaping must be labeled as gates, decay helpers, or amp helpers unless the modeled chip has that native envelope behavior.
- Sample chips need waveform views where useful. NES DMC, SPC700, and Paula sample banks should show selected sample waveforms and loop/root state; loop mode can be a checkbox when the chip page only needs one-shot vs loop.

## Layout Review Checklist

- Identify controls that are confusing, clipped, overlapping, hidden, or visually detached from the sound source they affect.
- Identify fake-continuous controls that should be segmented or stepped.
- Identify source cards that are too tall, too short, or have dead space between selector and level controls.
- Identify source cards where the selector is visible but the level lane, active state, or sample/wave assignment is clipped.
- Identify sampler or wavetable cards where the Level lane is technically present but too short, too narrow, overlapping the selector, or stranded far away from the sample/wave selector.
- Identify standard dropdowns or numeric boxes rendered below the default usable height.
- Identify places where a roadmap placeholder is visible but not engine-backed.
- Identify chip-specific controls that should move from shared panels into source cards.
- Identify shared sections that are mostly informational. Remove, collapse, or replace them with chip-owned controls unless the information directly helps the musician act.
- Identify excessive vertical whitespace. Compact the panel while preserving control height and label readability; do not bury controls at the bottom of oversized cards.
- Check both default width and compact width. A chip page that only works at 1240 px but clips at 1180 px is still a layout regression.
- Identify generic visual polish that would reduce usability, shrink standard inputs, or detach controls from their chip source.
- Prefer one shippable vertical slice: change the affected chip layout, add or tighten a smoke-test guard, update the owning doc, build/test/install when code changes, then commit and push.

## Verification

- Run `git diff --check` for every change.
- For UI code changes, build `Chipper_VST3` and run `chipper_editor_size_smoke`, `chipper_processor_midi_cc_smoke`, and `chipper_parameter_midi_cc_smoke`.
- When touching samples, verify loop vs one-shot behavior and missing-file UI states.
- When touching FM behavior, run the held-tail/preset sustain checks before claiming no regression.
- Install from the same build root after code changes so the VST3 footer/marker reflects the committed build.
- For docs-only or skill-only changes, do not rebuild/install unnecessarily; verify the installed build marker if the user may be checking Ableton against a previous binary.
