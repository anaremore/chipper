# UI Priority Audit

This audit tracks layout and control-placement work that most directly improves Chipper as a usable, chip-aware instrument.

## Non-Regression Checklist

- Do not number sections. Hidden or chip-specific modules make numbering misleading.
- Keep the header focused on preset, chip mode, Strictness, and Play Mode. Verification strength belongs in the footer.
- Use standard-height dropdowns, buttons, and numeric inputs. If a control cannot fit at standard size, grow the card or layout.
- Never let labels, readouts, dropdowns, sliders, or waveform views overlap. Readability beats compactness.
- Channel-local register controls live in their channel card: duty under pulse channels, sample/wave selectors under sample/wavetable voices, and noise controls under noise channels.
- Shared modules are only for shared hardware or shared workflow: filters, echo, sample banks, output routing, global clocks, and cross-channel pairing.
- Sample and wavetable chips must reserve a real waveform/sample workspace. Performance macros may be compact, but they must not overlap sample-bank controls.
- Footer verification language must remain truthful: **Verified partial** is acceptable; **Cycle accurate** is not allowed without accepted tests or hardware/reference proof.
- Chip themes may differ, but contrast, focus, hit targets, and readable text size must stay consistent.
- Logo underline decorations should align to the full logo width so the header feels intentional on every theme.

## Completed Pattern

- Game Boy / DMG now treats channel cards as the owner of channel-native register controls: Pulse 1 duty, Pulse 2 duty, Wave RAM shape/level, and Noise width mode.
- NES / RP2A03 should follow the same channel-ownership pattern: Pulse 1 duty lives under Pulse 1, Pulse 2 duty lives under Pulse 2, and Noise mode lives under Noise / DMC. The old Shape / Mixer panel should not be the primary place users edit these register-like choices.
- NES / RP2A03 now reserves more vertical room for those channel-owned controls and explicitly clears the old global Pulse 1 Duty slot in the Performance strip. If Pulse 2 appears to follow Pulse 1, verify whether Pulse 2 Duty is set to `Preset`; explicit 12.5/25/50/75 choices are independent `$4004` duty overrides.
- Game Boy / DMG follows the same rule: Pulse 2's `Preset` state intentionally follows or offsets Pulse 1 according to the selected recipe/play mode, while explicit 12.5/25/50/75 choices write the `NR21` duty bits independently.
- Embedded channel controls must suppress their old standalone labels and readouts; otherwise stale labels can reappear after chip switching or parameter refresh.
- HuC6280, Namco WSG, and Konami SCC now follow the same rule: per-channel wave shape selectors live inside a taller wavetable voice deck instead of a detached Wave/Mixer panel, with level strips reserved so wave controls cannot crowd out channel gain.
- Wavetable-family source decks now take priority over duplicate summary copy: HuC6280, Namco WSG, and SCC reserve more height for channel cards so per-lane wave selectors and level controls stay readable.
- Wavetable and sampler channel cards should stack controls in the same order users hear them: enable/header, waveform preview, per-voice wave or sample selector, then level. Dense channel cards now reserve a dedicated bottom level lane so wave/sample selectors cannot crowd out gain visibility.
- Amiga Paula now follows the channel-local rule too: each of the four hard-panned sample channels exposes its own generated sample shape selector and optional loaded sample-bank slot selector inside a taller sampler-style channel card, matching Paula's independent DAC/sample playback model. The shared sample-bank panel stays focused on file/folder import, note mapping, and waveform preview; the old global Sample Shape segment stays hidden for Paula.
- Channel-local wave/sample selectors must stay visible from the source-card surface, even when the old standalone Wave Shape segment is hidden. This prevents Paula, HuC6280, Namco WSG, SCC, and SPC700 from losing their per-voice controls during chip switches or visibility refreshes.
- SNES SPC700-style voices use the same source-card ownership rule for sample slots: each of the eight voice cards exposes a sample-source dropdown so loaded BRR/WAV/AIFF bank slots can be pinned per voice instead of relying only on one global manual slot.
- SPC700/SNES reserves enough vertical space for three different concepts to remain separately readable: the top sample/envelope controls, the eight per-voice sample cards, and the shared sample-bank waveform editor. The Loop While Held setting is a checkbox-style toggle, not a dropdown-style row, because it is a binary playback-lifetime choice.
- SPC700/SNES envelope shaping is not decorative: ADSR/GAIN-style speed controls must remain visible beside the envelope shape buttons because this is one of the chip's native musical behaviors.
- SPC700/SNES voice cards must reserve a bottom level lane before placing sample dropdowns. Each S-DSP voice is a playable source with its own output level, so hidden level sliders are a usability regression.
- NES / RP2A03 DMC sample controls now prioritize the sample bank and waveform preview over the APU envelope helper. APU decay moved into Performance Macros so the DMC panel can use a two-column sample-editor layout with a larger waveform surface.
- NES / RP2A03 DMC loop wording should distinguish true sample looping from the hardware DAC hold. The `Loop Sample` checkbox maps to the `$4010` loop bit; when it is off, DPCM bit stepping stops at the final bit and the DMC DAC holds its terminal level until the next trigger.
- FM modes should not be allowed to regress into sustained-note fade-out. Renderer `tailRms` held-tail assertions are in place for YM2612/OPN2, OPL2/OPL3, YM2151/OPM, YM2413/OPLL, and key factory presets; any FM UI/control pass touching operator envelopes, source levels, or key-on handling should keep that CTest subset green.
- SID already follows this pattern for per-voice waveform and pulse-width controls, with the global filter staying in the Filter panel.
- Shared helper controls must use neutral base wording and chip-specific resolved readouts. For example, a volume-gate helper on POKEY or Paula should never describe itself as a NES/APU envelope period.
- Visible preset-following states should say "Preset" rather than "Follow" when the control is using the selected preset recipe. "Shared Bank" is used for sample voices that inherit the global sample slot or note map.
- Envelope sections should use truthful chip taxonomy: **ADSR** for SID-style per-voice ADSR and S-DSP ADSR/GAIN work, **Operator EG** for FM operator envelopes, **APU/Volume/Hardware Envelope** for native non-ADSR chips, and **Amp Env/Volume Helper** when Chipper adds a musical helper over volume, attenuation, or sample playback.
- Chipper helper envelopes must name the underlying chip path when practical and must be honest about scope. POKEY should say **AUDV Gate** and Paula should say **Tracker Amp Env**. HuC6280, SCC, and Namco WSG currently expose one shared musical volume helper, so the visible control says **Global Amp Env** while per-channel shape/sample/level controls stay in the source cards. Avoid generic ADSR/Decay wording unless the chip really exposes that behavior.
- The code now exposes this as `EnvelopeModel` metadata via `envelopeModelFor()`, with descriptor-smoke coverage for native ADSR/GAIN, native Operator EG, native non-ADSR envelopes, Chipper helper envelopes, and the visible module titles for SPC700, FM, POKEY, Paula, HuC6280, Namco WSG, and SCC. Host/MIDI wording should use neutral names like **Envelope / EG** when one shared parameter spans multiple chips, while visible chip panels should use the chip-specific labels above.
- HuC6280, Namco WSG, and Konami SCC now expose the shared helper envelope as a visible `Global Amp Env` module with enough height for a normal slider/readout instead of a dead-looking title-only placeholder. The source cards remain the home for per-channel shape/sample/level editing; future per-channel gate helpers should move into those cards if the engine supports them.
- Embedded combo boxes and numeric inputs should use the standard control height wherever there is room. If a card cannot fit a standard-height dropdown plus label, increase the card or layout size instead of shrinking the dropdown into unreadable text. Dense wavetable and sampler source cards now reserve standard-height selectors and a clearer bottom level lane.
- The editor default canvas now gives chip-specific layouts more room. This is intentional: Chipper should favor readable channel-local controls and waveform/sample previews over forcing every chip into the earliest compact prototype size. Sampler modes are baseline size drivers because Paula and SPC700 need visible per-channel sample selectors plus a real sample-bank waveform surface without colliding with Performance Macros.
- Paula now reserves a protected Sample Bank row and a separate Performance Macro strip. The sample-bank waveform, playback controls, and per-channel sample cards take priority because Paula is fundamentally a four-channel sample instrument; macro controls should never visually overlap the shared sample editor.
- Paula's compact macro strip is intentionally shorter than its sampler workspace. If the editor is resized, give height to the four channel cards and sample waveform before expanding macros.
- Sample-focused layouts should not reuse the Sample Bank module as a generic Output panel. Paula and SPC700 route output controls through Performance Macros so the shared file/folder/sample waveform editor keeps a protected rectangle.
- Dense sampler and wavetable cards reserve their bottom level lane before placing optional shape/sample selectors. This keeps per-channel gain readable for Paula, SPC700, HuC6280, Namco WSG, and SCC even when those cards grow more chip-aware controls. Treat the reserved level lane as the non-negotiable part of the card; if future controls do not fit above it, grow the card instead of shrinking level controls.
- SPC700 and Paula sample-bank panels now require enough height before switching into the two-column editor layout. Pitch, playback, root, loop, and file/folder/bank controls keep standard control height; if the sample bank is too short, the layout falls back instead of letting controls cover the waveform or Performance Macros.
- Performance Macros now show chip-aware destination hints in their resolved readout lines, such as `$400E noise`, `NR43 noise`, `$D415 cutoff`, `AUDV gate`, `Wave RAM`, and `$B0 feedback`. These hints make it clear which macros are musical gestures over native chip paths rather than generic unlabeled sliders.
- The preset browser now groups factory sounds by chip-local musical category and shows counts in the section headers, while user presets remain a separate flat-file bank. A chip-local Init Patch entry sits above both banks for a neutral reset point that preserves the current Strictness and supported Play Mode. This keeps browsing useful without changing the simple `.chipperpreset` sharing model.
- Descriptor smoke tests now lock the public chip catalog to named hardware families only, assert that `Arcade Hybrid` and `Custom` do not return as undefined selector modes, and require each exposed chip to have factory presets available in the UI.

## Highest-Value Next Fixes

1. Preset browser and preset sharing
   - Issue: the top dropdown now groups the current chip's presets by category and includes a chip-local Init Patch, but 15 chips still need richer search/filtering by role, engine, tag, and favorites. User presets should remain simple flat files that are easy to share.
   - User value: very high. Most musicians will browse for "arcade bass" or "Game Boy lead" before they know which chip engine they want.
   - Confidence: 7/10. Preset data already exists; browser UX and save/load polish are the main work.

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
   - Issue: channel-local generated shapes, loaded sample-slot pins, and matching renderer flags are in place, but MOD sample extraction and imported loop metadata remain planned.
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

10. Audio non-regression smoke checks
   - Issue: FM held-tail assertions are now in place, but sample, loop, and helper-envelope fixes can still look correct while regressing key-on/key-off, one-shot/loop behavior, or sustained output.
   - User value: high. A playable instrument must hold notes predictably before deeper editor polish matters.
   - Confidence: 8/10. Existing renderer and processor smoke tests already expose source levels, key-on state, sample loop state, `tailRms`, and debug JSON; the next value is expanding the same assertion style beyond FM.

## Design Rule

When a control maps to a channel-local register, place it in or directly under that channel's card. Reserve shared panels for genuinely shared hardware: filters, output routing, sample banks, global clocks, echo, LFOs, and cross-channel pairing.
