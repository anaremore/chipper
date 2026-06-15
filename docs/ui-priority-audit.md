# UI Priority Audit

This audit tracks layout and control-placement work that most directly improves Chipper as a usable, chip-aware instrument.

## Current Audit Focus

The current UI baseline is source-card ownership plus readable standard controls. New screenshot feedback should be turned into durable rules here, then pruned from the active queue once the rule is implemented and verified. Fixed audio bugs such as FM held-tail and NES DMC loop-off only belong in this audit when a UI change could reintroduce them. Broad product backlog items belong in `product-gap-roadmap.md`; this file is the visual and control-placement checklist.

Review status: synced on 2026-06-14 after the fixed-regression and helper-envelope cleanup. The focused release gate passed 26/26 on 2026-06-14, so this audit treats FM held-tail, NES DMC loop-off, and helper-envelope honesty as protected release gates. Active UI work focuses on chip-owned controls, readable source cards, sample/wave/operator editors, and current screenshot regressions. If a fixed audio issue or misleading label is suspected again, verify the gate first; do not turn it into a UI todo unless the current screen gives users a misleading or inaccessible control path.

Command-line release gates live in [release-builds.md](release-builds.md). This audit should keep only the visual rules that prevent users from misunderstanding or losing access to chip behavior. When an audio bug is fixed and tested, keep the UI implication here only if it affects what users can see, choose, or trust.

## Planning Hygiene

This file should stay visual. If a bug is fixed by a renderer or processor gate, keep only the UI rule that prevents confusing the user. Do not keep old audio bugs in the active UI queue unless a UI change can reintroduce them or the current screenshot shows the same class of visual failure.

For every new screenshot issue, reduce it to one of these reusable questions before editing:

- Is the control owned by the channel, lane, voice, operator, or shared chip block that actually controls the sound?
- Is the control standard height and readable at the default editor size?
- Does the visible wording tell the truth about native hardware versus Chipper helper behavior?
- Does the panel have enough room for all controls without hiding level, sample, wave, loop, or envelope state?
- If the issue is fixed, is there a descriptor, renderer, screenshot, or checklist gate that makes it less likely to regress?

## Current Visual Gate

Before a UI slice is considered done, inspect the changed chip at the default editor size and one neighboring chip from a different layout family. The pass should confirm:

- no hidden source-level lanes
- no shrunken dropdowns or numeric boxes where standard-height controls should fit
- no overlap between sample-bank editors, waveform previews, and Performance Macros
- source-owned controls stay in the source cards
- footer verification wording remains visible and truthful
- the Performance Macros strip never draws over chip-owned editors such as sample banks, waveform previews, operator grids, or source cards

## Non-Regression Checklist

- Do not number sections. Hidden or chip-specific modules make numbering misleading.
- Keep the header focused on preset, chip mode, Strictness, and Play Mode. Verification strength belongs in the footer.
- Use standard-height dropdowns, buttons, and numeric inputs. If a control cannot fit at standard size, grow the card or layout.
- Never let labels, readouts, dropdowns, sliders, or waveform views overlap. Readability beats compactness.
- Channel-local register controls live in their channel card: duty under pulse channels, sample/wave selectors under sample/wavetable voices, and noise controls under noise channels.
- Shared modules are only for shared hardware or shared workflow: filters, echo, sample banks, output routing, global clocks, and cross-channel pairing.
- Sample and wavetable chips must reserve a real waveform/sample workspace. Performance macros may be compact, but they must not overlap sample-bank controls.
- When a chip has per-voice or per-lane ownership, the source card should show the playable path in one place: waveform or sample selector, key/loop/noise mode where applicable, level, and any visible helper envelope state. If those controls cannot fit, the layout grows instead of hiding them.
- Footer verification language must remain truthful: **Verified partial** is acceptable; **Cycle accurate** is not allowed without accepted tests or hardware/reference proof.
- Chip themes may differ, but contrast, focus, hit targets, and readable text size must stay consistent.
- Logo underline decorations should align to the full logo width so the header feels intentional on every theme.
- Screenshot feedback should become durable layout rules in this audit. Avoid preserving one-off visual bug notes after the underlying rule is captured and verified.
- Fixed audio regressions should only appear here when they constrain UI changes. The active UI queue should focus on visible chip-aware controls, readable source cards, sample/wave/operator editing, and screenshot regressions that still reproduce.
- If a visual problem has already been fixed, keep only the reusable rule that would prevent it from returning. If a chip still violates that rule in screenshots, track the chip-specific fix under Highest-Value Next Fixes.
- If a screenshot issue is fixed in code, prefer adding or tightening a descriptor/render/checklist gate over preserving the screenshot as a permanent todo.
- If a non-visual audio bug is fixed, keep the UI implication here only when it affects what users can see or choose. The actual audio memory belongs in the regression gate table, not in every visual backlog.

## Completed Pattern

- Game Boy / DMG now treats channel cards as the owner of channel-native register controls: Pulse 1 duty, Pulse 2 duty, Wave RAM shape/level, and Noise width mode.
- NES / RP2A03 should follow the same channel-ownership pattern: Pulse 1 duty lives under Pulse 1, Pulse 2 duty lives under Pulse 2, and Noise mode lives under Noise / DMC. The old Shape / Mixer panel should not be the primary place users edit these register-like choices.
- NES / RP2A03 now reserves more vertical room for those channel-owned controls and explicitly clears the old global Pulse 1 Duty slot in the Performance strip. If Pulse 2 appears to follow Pulse 1, verify whether Pulse 2 Duty is set to `Preset`; explicit 12.5/25/50/75 choices are independent `$4004` duty overrides.
- Game Boy / DMG follows the same rule: Pulse 2's `Preset` state intentionally follows or offsets Pulse 1 according to the selected recipe/play mode, while explicit 12.5/25/50/75 choices write the `NR21` duty bits independently.
- Embedded channel controls must suppress their old standalone labels and readouts; otherwise stale labels can reappear after chip switching or parameter refresh.
- HuC6280, Namco WSG, and Konami SCC now follow the same rule: per-channel wave shape selectors live inside a taller four-column wavetable voice deck instead of a detached Wave/Mixer panel, with level strips reserved so wave controls cannot crowd out channel gain or stretch each card into an awkward wide strip.
- Wavetable-family source decks now take priority over duplicate summary copy: HuC6280, Namco WSG, and SCC reserve more height for channel cards so per-lane wave selectors and level controls stay readable.
- Wavetable and sampler channel cards should stack controls in the same order users hear them: enable/header, waveform preview, per-voice wave or sample selector, then level. Dense channel cards now keep the level lane directly in the reading flow so wave/sample selectors cannot crowd out gain visibility or strand the slider at the bottom of a tall card.
- Amiga Paula now follows the channel-local rule too: each of the four hard-panned sample channels exposes its own generated sample shape selector and optional loaded sample-bank slot selector inside a taller sampler-style channel card, matching Paula's independent DAC/sample playback model. The shared sample-bank panel stays focused on file/folder import, note mapping, and waveform preview; the old global Sample Shape segment stays hidden for Paula.
- Paula channel cards place the generated shape and sample-bank selector side by side at standard dropdown height where space allows. This keeps both per-channel sample decisions readable without pushing the protected level lane out of view.
- Channel-local wave/sample selectors must stay visible from the source-card surface, even when the old standalone Wave Shape segment is hidden. This prevents Paula, HuC6280, Namco WSG, SCC, and SPC700 from losing their per-voice controls during chip switches or visibility refreshes.
- HuC6280, Namco WSG, and SCC wavetable cards should stay compact but complete: waveform preview, per-lane wave selector, and a visible level lane. Use a horizontal selector row and reserve the level lane from the bottom so empty preview/selector space cannot stretch the cards or push gain out of view.
- Paula's shared sample-bank workspace has priority over the Performance Macros strip. The macro strip should shrink before the waveform preview, file/folder/bank controls, playback selector, or per-channel sample pins overlap.
- SNES SPC700-style voices use the same source-card ownership rule for sample slots: each of the eight voice cards exposes a sample-source dropdown so loaded BRR/WAV/AIFF bank slots can be pinned per voice instead of relying only on one global manual slot.
- SPC700/SNES reserves enough vertical space for three different concepts to remain separately readable: the top sample/envelope controls, the eight per-voice sample cards, and the shared sample-bank waveform editor. The playback lifetime control is a checkbox-style toggle with chip-specific wording (`Loop While Held` when enabled, `One Shot` when disabled), not a dropdown-style row, because it is a binary S-DSP sample-lifetime choice.
- SPC700/SNES envelope shaping is not decorative: ADSR/GAIN-style speed controls must remain visible beside the envelope shape buttons because this is one of the chip's native musical behaviors.
- SPC700/SNES top envelope controls use a two-column layout at normal editor width so envelope shape and ADSR/GAIN speed are both readable. If the editor is narrowed, the controls may stack, but the speed slider still keeps a standard-height readout lane.
- SPC700/SNES voice cards must reserve a readable level lane after the sample dropdown. Each S-DSP voice is a playable source with its own output level, so hidden level sliders are a usability regression.
- SPC700/SNES gives voice-card height priority over secondary macro height. The eight voice cards must fit header, waveform preview, sample dropdown, and Level lane before the Performance Macros strip expands.
- NES / RP2A03 DMC sample controls now prioritize the sample bank and waveform preview over the APU envelope helper. APU decay moved into Performance Macros so the DMC panel can use a two-column sample-editor layout with a larger waveform surface.
- NES / RP2A03 DMC loop wording should distinguish true sample looping from the hardware DAC hold. The `Loop Sample` checkbox maps to the `$4010` loop bit; when it is off, DPCM bit stepping stops at the final bit and the DMC DAC holds its terminal level until the next trigger. `processor_midi_cc_smoke` is the quick regression check for manual-slot and note-map one-shot behavior, loop-on behavior, debug playback state, and waveform-preview loop markers.
- FM modes should not be allowed to regress into sustained-note fade-out. Renderer `tailRms` held-tail assertions are in place for YM2612/OPN2, OPL2/OPL3, YM2151/OPM, YM2413/OPLL, and key factory presets; any FM UI/control pass touching operator envelopes, source levels, or key-on handling should keep that CTest subset green.
- Because FM held-tail behavior is currently fixed, FM planning should focus on user-visible operator editing, algorithm clarity, and envelope terminology. Do not keep "fix FM fade" as an active UI item unless a current build reproduces it and the held-tail gate fails.
- SID already follows this pattern for per-voice waveform and pulse-width controls, with the global filter staying in the Filter panel.
- Shared helper controls must use neutral base wording and chip-specific resolved readouts. For example, a volume-gate helper on POKEY or Paula should never describe itself as a NES/APU envelope period.
- Visible preset-following states should say "Preset" rather than "Follow" when the control is using the selected preset recipe. "Shared Bank" is used for sample voices that inherit the global sample slot or note map.
- Envelope sections should use truthful chip taxonomy: **ADSR** for SID-style per-voice ADSR and S-DSP ADSR/GAIN work, **Operator EG** for FM operator envelopes, **APU/Volume/Hardware Envelope** for native non-ADSR chips, and **Amp Env/Volume Helper** when Chipper adds a musical helper over volume, attenuation, or sample playback.
- Chipper helper envelopes must name the underlying chip path when practical and must be honest about scope. POKEY should say **AUDV Gate** and Paula should say **Tracker Amp Env**. HuC6280, SCC, and Namco WSG currently expose one shared musical volume helper, so the visible control says **Global Amp Env** while per-channel shape/sample/level controls stay in the source cards. Avoid generic ADSR/Decay wording unless the chip really exposes that behavior.
- Helper-envelope parameter help is part of the UI contract too: non-native helper chips must not reuse NES copy or native ADSR wording, and their help should disclose the helper/gate layer so users understand what is authentic chip state versus a Chipper playability layer.
- Helper envelope modules must not be empty. If the engine only has a shared helper, show one standard-height shared control with an honest readout. If a future pass adds per-channel helper envelopes, move them into the channel cards so users can see which lane is being shaped.
- The code now exposes this as `EnvelopeModel` metadata via `envelopeModelFor()`, with descriptor-smoke coverage for native ADSR/GAIN, native Operator EG, native non-ADSR envelopes, Chipper helper envelopes, and the visible module titles for SPC700, FM, POKEY, Paula, HuC6280, Namco WSG, and SCC. Host/MIDI wording should use neutral names like **Envelope / EG** when one shared parameter spans multiple chips, while visible chip panels should use the chip-specific labels above.
- HuC6280, Namco WSG, and Konami SCC now expose the shared helper envelope as a visible `Global Amp Env` module with enough height for a normal slider/readout instead of a dead-looking title-only placeholder. The source cards remain the home for per-channel shape/sample/level editing; future per-channel gate helpers should move into those cards if the engine supports them.
- Embedded combo boxes and numeric inputs should use the standard control height wherever there is room. If a card cannot fit a standard-height dropdown plus label, increase the card or layout size instead of shrinking the dropdown into unreadable text. Dense wavetable and sampler source cards now reserve standard-height selectors and a clearer level lane.
- Grouped slider readouts must not depend on leftover slivers of space. If a panel is compact, place the readout in the header row; otherwise reserve a standard-height readout below the slider.
- Source-card panels should size from the visible card rows and the controls they actually contain, not stale percentage minima. The card should be tall enough for waveform preview, standard selectors, and a visible level lane, while extra vertical budget goes to sample-bank waveform editors, output scopes, or macro controls instead of empty source-card space.
- Source-card height budgets must include the module title/summary chrome. SPC700 voice cards need enough post-header height for sample selector plus level, and wavetable cards should stay compact by trimming unused body space rather than hiding level lanes.
- Wavetable source decks should use a tight control-derived budget: header reserve plus compact card rows. HuC6280, Namco WSG, and SCC need enough room for waveform preview, a standard-height wave selector, and a visible Level lane, but should not stretch each source card into tall empty panels. Current source cards are top-aligned and capped near the visible-control height so extra editor height does not turn each lane into an oversized blank panel.
- Wavetable decks should choose columns from chip voice count, not a generic three-column fallback. HuC6280 and SCC use one compact row at the default width, while Namco WSG stays four-by-two, so waveform previews stay readable without turning each card into a tall empty panel.
- Wavetable decks should not spend extra vertical budget on empty card bodies. HuC6280, Namco WSG, and SCC source rows now cap close to the actual control stack: header, waveform, standard wave selector, and visible Level lane. Extra height should go to meaningful editors or macros, not blank space inside source cards.
- The editor default canvas now gives chip-specific layouts more room. This is intentional: Chipper should favor readable channel-local controls and waveform/sample previews over forcing every chip into the earliest compact prototype size. Sampler modes are baseline size drivers because Paula and SPC700 need visible per-channel sample selectors plus a real sample-bank waveform surface without colliding with Performance Macros.
- Chip-specific layout heights should stay explicit. NES, SID, SPC700, Paula, wavetable, and FM/PSG fallback screens have different space budgets; avoid generic sample fallbacks that silently change sampler behavior when one chip needs a special case.
- Paula reserves three separate workspaces: channel-local sample cards, a protected Sample Bank waveform/editor row, and a secondary Performance Macro strip. The sample editor and channel cards are the layout drivers; macro controls must grow or move before they overlap file/import controls or waveform preview.
- Sample-focused layouts should not reuse the Sample Bank module as a generic Output panel. Paula and SPC700 route output controls through Performance Macros so the shared file/folder/sample waveform editor keeps a protected rectangle.
- Dense sampler and wavetable cards reserve their level lane in the main top-down control flow after optional shape/sample selectors. This keeps per-channel gain readable for Paula, SPC700, HuC6280, Namco WSG, and SCC even when those cards grow more chip-aware controls. Treat the reserved level lane as the non-negotiable part of the card; if future controls do not fit, grow the card instead of shrinking level controls. Descriptor smoke coverage now also requires every visible source-level spec to keep a Level-named label, help text, APVTS parameter, and MIDI CC mapping so a level lane cannot silently degrade into an unlabeled or unhelpful control.
- SPC700 per-voice sample slot metadata is now a descriptor-smoke contract for all eight voices: each voice keeps a standard menu surface in the Sample group, supports preset plus 32 bank slots, explains loaded sample-bank pinning, and remains backed by APVTS plus MIDI CC mapping.
- Wavetable-family source decks should stay compact enough to scan. HuC6280, Namco WSG, and SCC should budget each lane card for the visible waveform, standard-height wave selector, and Level lane in a top-down flow, without stretching into tall empty cards. Grow only when new visible per-lane controls require it.
- For HuC6280, Namco WSG, and SCC, the source deck owns the vertical budget before helper-envelope and output rows. Those secondary rows may be shorter so every visible wavetable lane keeps its waveform preview, wave selector, and Level lane readable in the card.
- Sample-family source decks should follow the same control-derived height rule. SPC700 and Paula source cards need enough room for waveform preview, standard sample/shape selectors, and Level lanes, but extra vertical budget should go to the shared sample-bank waveform editor or compact Performance Macros instead of tall empty voice cards.
- SPC700 and Paula sample-bank panels now require enough height before switching into the two-column editor layout. Pitch, playback, root, loop, and file/folder/bank controls keep standard control height; if the sample bank is too short, the layout falls back instead of letting controls cover the waveform or Performance Macros. Current guardrails: SPC700 needs at least 400 px of sample-bank height for two-column layout, Paula needs at least 360 px, with Paula's module budget protecting roughly 386 px or more for the shared Sample Bank at default size.
- Paula sample-bank layout has priority over macro-strip height. The four channel cards still need readable per-channel shape/sample/level controls, but the shared sample-bank waveform/editor row must stay tall enough to avoid collisions with Performance Macros; shrink the macro strip, cap the waveform preview inside the Sample Bank module, or rebalance channel-card height before shrinking the sample editor.
- Paula sampler layout should balance three visible layers without overlap: per-channel sample cards, the shared sample-bank waveform/editor, and compact Performance Macros. The sample bank is the protected editor; macros are secondary, and channel cards should stay only tall enough for waveform, shape/sample selectors, and level.
- Paula uses a compact Performance Macros strip so the shared Sample Bank editor remains the layout driver. The layout gives Paula a deliberately smaller macro budget and a flexible sample-bank minimum so per-channel sample cards, file/folder/bank controls, and the waveform preview do not collide at the default editor size. If future Paula macros need more room, grow the editor or add a dedicated secondary view instead of taking height back from file/folder/bank controls or the waveform preview.
- SPC700 and Paula sampler layouts now reserve extra source-card height for standard sample dropdowns and visible level lanes, while their Performance Macros strips stay slightly shorter than non-sampler pages. This is intentional: sampler usability depends on seeing channel sample assignment, level, and the shared waveform editor before secondary macro controls.
- Sampler source-card height must be calculated from the full visible control stack, not from a generic row percentage. SPC700 voice cards need room for header, waveform, per-voice sample selector, and level; Paula cards need header, waveform, per-channel generated shape, per-channel sample slot, and level. If these cannot fit, grow the sampler page rather than hiding the dropdown or level lane.
- Binary sample lifetime controls should read like binary controls. SPC700 loop lifetime is a checkbox-style `Loop While Held` control in the sample bank; one-shot playback is the unchecked state, while detailed playback/register notes stay in the status/readout text.
- Sample-bank panels are primary editors, not status widgets. NES DMC, SPC700, and Paula should show a useful waveform preview, readable file/folder/bank controls, root/playback/loop state, and missing-sample feedback without overlapping Performance Macros. If a helper envelope or macro competes for space with sample import and waveform inspection, move the helper into Performance Macros or grow the layout.
- Sampler and wavetable chips may require a taller default editor. Protect per-channel shape/sample selectors, level sliders, and waveform previews before compressing the page. A slightly larger fixed canvas is preferable to hiding controls inside cramped source cards.
- Performance Macros now show chip-aware destination hints in their resolved readout lines, such as `$400E noise`, `NR43 noise`, `$D415 cutoff`, `AUDV gate`, `Wave RAM`, and `$B0 feedback`. These hints make it clear which macros are musical gestures over native chip paths rather than generic unlabeled sliders.
- The preset browser now groups factory sounds by chip-local musical category and shows counts in the section headers, while user presets remain a separate flat-file bank. A chip-local Init Patch entry sits above both banks for a neutral reset point that preserves the current Strictness and supported Play Mode. This keeps browsing useful without changing the simple `.chipperpreset` sharing model.
- Descriptor smoke tests now lock the public chip catalog to named hardware families only, assert that `Arcade Hybrid` and `Custom` do not return as undefined selector modes, and require each exposed chip to have factory presets available in the UI.
- Descriptor smoke tests also act as the first UI contract for chip-owned controls: NES/DMG pulse duty and noise choices, SPC700/Paula sample slots, HuC6280/Namco/SCC wave selectors, source enable toggles, and source level trims must remain tied to source-card surfaces or explicitly documented shared modules.
- Shared APVTS/MIDI slots should use generic host-facing names when their chip meaning changes by mode. CC94 remains the stable parameter behind DMG Stereo Route, SID Model, SPC700/Paula loop behavior, POKEY AUDCTL pairing, HuC6280 LFO mode, FM pan/routing, and some final wavetable lane choices, so the host/default map calls it `Chip Choice / Route` while each chip panel shows the descriptor-specific label.
- Active chip panels should describe what the current build can do, while verification gaps and future work belong in the footer tooltip, accuracy docs, and roadmaps. Avoid dead-looking rows such as `Reference tests needed`, `Custom patch planned`, or `Drive planned` inside playable chip modules; use current-state labels such as `Verified partial`, `Operator EG readout`, `Model color`, or the exact register path instead.
- Retired or unknown chip descriptors are migration fallbacks only. If an old session somehow reaches that surface, it should say `Unavailable` / `Migration only` and point the user toward named hardware-family modes rather than presenting planned modules or half-productized generic engines.

## Highest-Value Next Fixes

1. Preset browser and preset sharing
   - Issue: the top dropdown now groups the current chip's presets by category and includes a chip-local Init Patch, but 15 chips still need richer search/filtering by role, engine, tag, and favorites. User presets should remain simple flat files that are easy to share.
   - User value: very high. Most musicians will browse for "arcade bass" or "Game Boy lead" before they know which chip engine they want.
   - Confidence: 7/10. Preset data already exists; browser UX and save/load polish are the main work. Confidence improves with a preset QA pass that checks audibility, loudness, visible-control recall, and clean provenance for every factory preset.

2. Basic / Edit / Expert views
   - Issue: Play, edit, and register/audit information are currently shown together, which makes the instrument feel powerful but dense.
   - User value: very high. Keeps the default view musical while preserving honest chip detail for users who want it.
   - Confidence: 6/10. Needs careful visibility state and host-parameter behavior so no controls disappear from automation unexpectedly.

3. SNES SPC700-style sample voices
   - Issue: each voice can now pin a loaded sample slot in its card, but deeper work still needs per-voice loop/envelope/noise controls and clearer visual confirmation of voice-to-sample mapping.
   - User value: high. Makes SNES mode feel like an eight-voice sample instrument rather than one global sample player.
   - Confidence: 7/10. Existing sample-bank and voice-card surfaces are usable foundations; remaining engine voice assignment semantics need careful scoping.

4. Wavetable voice polish
   - Issue: HuC6280, Namco WSG, and SCC now place per-voice wave selectors and protected level strips in the cards, but the next pass should verify whether they also need per-lane pan/pitch controls in the same cards.
   - User value: high. These chips are fundamentally independent wavetable lanes, so users should shape each lane where they hear and enable it.
   - Confidence: 8/10. Layout and parameters are in place; remaining work is deeper per-lane editing and any engine gaps found while playing.

5. FM operator editors
   - Issue: FM chips are playable, but the current UI is still too macro/operator-summary driven for serious FM editing.
   - User value: very high for FM users.
   - Confidence: 6/10. Needs a larger design slice: algorithm graph, operator grid, per-operator envelopes, and readable register values.

6. POKEY channel deck
   - Issue: channel cards now expose AUDC in the header and AUDF/AUDV in the per-channel strip, but AUDCTL pairing/filter paths still need a stronger visual relationship between linked channels.
   - User value: medium-high. Helps users understand why POKEY sounds chaotic and how channel pairing changes pitch behavior.
   - Confidence: 7/10. Register readouts are now straightforward; the remaining work is a clearer cross-channel pairing visual without crowding the deck.

7. Paula tracker import depth
   - Issue: channel-local generated shapes, loaded sample-slot pins, matching renderer flags, a protected Sample Bank waveform, and non-overlapping macro/sample layout are in place. MOD sample extraction, imported loop metadata, 14-bit/channel-pair options, and clearer tracker-style retrigger workflows remain planned.
   - User value: medium-high. Makes Paula feel more like a four-channel tracker sampler for real module-style workflows.
   - Confidence: 6/10. The UI and runtime bank model are usable; the remaining work needs file-format parsing and stricter validation.

8. Behavior strictness and register text presentation
   - Issue: exact register readouts help prove honesty, but they compete with musical labels in the default view.
   - User value: high. Musicians should see what they hear first, while advanced users can still inspect register-level behavior.
   - Confidence: 7/10. Requires a consistent Expert/detail overlay and tooltip policy across chips.

9. All-chip screenshot audit
   - Issue: each bespoke chip layout can regress independently, especially when a source card grows per-voice controls.
   - User value: high. Users should never need to guess whether a control is hidden, clipped, or decorative.
   - Confidence: 8/10. The latest screenshots make the remaining problems visible; a repeatable screenshot checklist will catch most regressions before release.

10. Expand audio non-regression smoke checks
   - Issue: FM held-tail assertions and NES DMC loop-off assertions are now release gates, not open UI bugs. The next coverage value is extending that same confidence to sample, loop, helper-envelope, and source-card changes that can look correct while regressing key-on/key-off, one-shot/loop behavior, or sustained output.
   - User value: high. A playable instrument must hold notes predictably before deeper editor polish matters.
   - Confidence: 8/10. Existing renderer and processor smoke tests already expose source levels, key-on state, sample loop state, `tailRms`, and debug JSON; the next value is expanding the same assertion style beyond FM.

## Design Rule

When a control maps to a channel-local register, place it in or directly under that channel's card. Reserve shared panels for genuinely shared hardware: filters, output routing, sample banks, global clocks, echo, LFOs, and cross-channel pairing.

When a chip needs more detail than the current card can hold, grow the chip layout or add a chip-specific editor surface. Do not hide the control, shrink it below the standard control height, or move it into a vague global macro area just to preserve the old grid.

## Cleanup Rule

After a screenshot-driven fix lands, keep the durable rule and remove the one-off complaint from active planning language. The audit should answer "what class of UI regression are we preventing now?" rather than preserve a chronological bug diary.
