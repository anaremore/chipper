# UI Priority Audit

This audit tracks layout and control-placement work that most directly improves Chipper as a usable, chip-aware instrument.

## Completed Pattern

- Game Boy / DMG now treats channel cards as the owner of channel-native register controls: Pulse 1 duty, Pulse 2 duty, Wave RAM shape/level, and Noise width mode.
- NES / RP2A03 should follow the same channel-ownership pattern: Pulse 1 duty lives under Pulse 1, Pulse 2 duty lives under Pulse 2, and Noise mode lives under Noise / DMC. The old Shape / Mixer panel should not be the primary place users edit these register-like choices.
- NES / RP2A03 now reserves more vertical room for those channel-owned controls and explicitly clears the old global Pulse 1 Duty slot in the Performance strip. If Pulse 2 appears to follow Pulse 1, verify that Pulse 2 Duty is not set to `Follow`; explicit 12.5/25/50/75 choices are independent register overrides.
- Embedded channel controls must suppress their old standalone labels and readouts; otherwise stale labels can reappear after chip switching or parameter refresh.
- HuC6280, Namco WSG, and Konami SCC now follow the same rule: per-channel wave shape selectors live inside a taller wavetable voice deck instead of a detached Wave/Mixer panel, with level strips reserved so wave controls cannot crowd out channel gain.
- Wavetable-family source decks now take priority over duplicate summary copy: HuC6280, Namco WSG, and SCC reserve more height for channel cards so per-lane wave selectors and level controls stay readable.
- Wavetable and sampler channel cards should stack controls in the same order users hear them: enable/header, waveform preview, per-voice wave or sample selector, then level. Dense channel cards now reserve a dedicated bottom level lane so wave/sample selectors cannot crowd out gain visibility.
- Amiga Paula now follows the channel-local rule too: each of the four hard-panned sample channels exposes its own generated sample shape selector and optional loaded sample-bank slot selector inside a taller sampler-style channel card, matching Paula's independent DAC/sample playback model. The shared sample-bank panel stays focused on file/folder import, note mapping, and waveform preview; the old global Sample Shape segment stays hidden for Paula.
- Channel-local wave/sample selectors must stay visible from the source-card surface, even when the old standalone Wave Shape segment is hidden. This prevents Paula, HuC6280, Namco WSG, SCC, and SPC700 from losing their per-voice controls during chip switches or visibility refreshes.
- SNES SPC700-style voices use the same source-card ownership rule for sample slots: each of the eight voice cards exposes a sample-source dropdown so loaded BRR/WAV/AIFF bank slots can be pinned per voice instead of relying only on one global manual slot.
- NES / RP2A03 DMC sample controls now prioritize the sample bank and waveform preview over the APU envelope helper. APU decay moved into Performance Macros so the DMC panel can use a two-column sample-editor layout with a larger waveform surface.
- SID already follows this pattern for per-voice waveform and pulse-width controls, with the global filter staying in the Filter panel.
- Shared helper controls must use neutral base wording and chip-specific resolved readouts. For example, a volume-gate helper on POKEY or Paula should never describe itself as a NES/APU envelope period.
- Visible preset-following states should say "Preset" rather than "Follow" when the control is using the selected preset recipe. "Shared Bank" is used for sample voices that inherit the global sample slot or note map.
- Envelope sections should use truthful chip taxonomy: **ADSR** for SID-style per-voice ADSR and S-DSP ADSR/GAIN work, **Operator EG** for FM operator envelopes, **APU/Volume/Hardware Envelope** for native non-ADSR chips, and **Amp Env/Volume Helper** when Chipper adds a musical helper over volume, attenuation, or sample playback.
- Chipper helper envelopes must name the underlying chip path when practical and must be honest about scope. POKEY should say **AUDV Gate** and Paula should say **Tracker Amp Env**. HuC6280, SCC, and Namco WSG currently expose one shared musical volume helper, so the visible control says **Global Amp Env** while per-channel shape/sample/level controls stay in the source cards. Avoid generic ADSR/Decay wording unless the chip really exposes that behavior.
- The code now exposes this as `EnvelopeModel` metadata via `envelopeModelFor()`, with descriptor-smoke coverage for native ADSR/GAIN, native Operator EG, native non-ADSR envelopes, Chipper helper envelopes, and the visible module titles for SPC700, FM, POKEY, Paula, HuC6280, Namco WSG, and SCC. Host/MIDI wording should use neutral names like **Envelope / EG** when one shared parameter spans multiple chips, while visible chip panels should use the chip-specific labels above.
- HuC6280, Namco WSG, and Konami SCC now have explicit helper-envelope modules instead of letting the decay helper appear under Motion/Wave copy. These modules must render a real slider, not just title/summary text, and the source cards remain the home for per-channel shape/sample/level editing.
- Embedded combo boxes and numeric inputs should use the standard control height wherever there is room. If a card cannot fit a standard-height dropdown plus label, increase the card or layout size instead of shrinking the dropdown into unreadable text.

## Highest-Value Next Fixes

1. Macro destination clarity
   - Issue: the bottom Performance Macros panel is now named correctly, but users still need clearer destination hints showing what each macro changes on the selected chip.
   - User value: very high. Users need to understand which controls are musical abstractions and which controls are chip-native.
   - Confidence: 8/10. Mostly tooltip, inline destination text, and control-label cleanup.

2. Preset browser and preset sharing
   - Issue: the top dropdown works, but 15 chips need filters by chip, role, engine, tag, favorites, and init patch. User presets should remain simple flat files that are easy to share.
   - User value: very high. Most musicians will browse for "arcade bass" or "Game Boy lead" before they know which chip engine they want.
   - Confidence: 7/10. Preset data already exists; browser UX and save/load polish are the main work.

3. Basic / Edit / Expert views
   - Issue: Play, edit, and register/audit information are currently shown together, which makes the instrument feel powerful but dense.
   - User value: very high. Keeps the default view musical while preserving honest chip detail for users who want it.
   - Confidence: 6/10. Needs careful visibility state and host-parameter behavior so no controls disappear from automation unexpectedly.

4. SNES SPC700-style sample voices
   - Issue: each voice can now pin a loaded sample slot in its card, but deeper work still needs per-voice loop/envelope/noise controls and clearer visual confirmation of voice-to-sample mapping.
   - User value: high. Makes SNES mode feel like an eight-voice sample instrument rather than one global sample player.
   - Confidence: 7/10. Existing sample-bank and voice-card surfaces are usable foundations; remaining engine voice assignment semantics need careful scoping.

5. Wavetable voice polish
   - Issue: HuC6280, Namco WSG, and SCC now place per-voice wave selectors and protected level strips in the cards, but the next pass should verify whether they also need per-lane pan/pitch controls in the same cards.
   - User value: high. These chips are fundamentally independent wavetable lanes, so users should shape each lane where they hear and enable it.
   - Confidence: 8/10. Layout and parameters are in place; remaining work is deeper per-lane editing and any engine gaps found while playing.

6. FM operator editors
   - Issue: FM chips are playable, but the current UI is still too macro/operator-summary driven for serious FM editing.
   - User value: very high for FM users.
   - Confidence: 6/10. Needs a larger design slice: algorithm graph, operator grid, per-operator envelopes, and readable register values.

7. POKEY channel deck
   - Issue: channel cards now expose AUDC in the header and AUDF/AUDV in the per-channel strip, but AUDCTL pairing/filter paths still need a stronger visual relationship between linked channels.
   - User value: medium-high. Helps users understand why POKEY sounds chaotic and how channel pairing changes pitch behavior.
   - Confidence: 7/10. Register readouts are now straightforward; the remaining work is a clearer cross-channel pairing visual without crowding the deck.

8. Paula tracker import depth
   - Issue: channel-local generated shapes, loaded sample-slot pins, and matching renderer flags are in place, but MOD sample extraction and imported loop metadata remain planned.
   - User value: medium-high. Makes Paula feel more like a four-channel tracker sampler for real module-style workflows.
   - Confidence: 6/10. The UI and runtime bank model are usable; the remaining work needs file-format parsing and stricter validation.

9. Behavior strictness and register text presentation
   - Issue: exact register readouts help prove honesty, but they compete with musical labels in the default view.
   - User value: high. Musicians should see what they hear first, while advanced users can still inspect register-level behavior.
   - Confidence: 7/10. Requires a consistent Expert/detail overlay and tooltip policy across chips.

## Design Rule

When a control maps to a channel-local register, place it in or directly under that channel's card. Reserve shared panels for genuinely shared hardware: filters, output routing, sample banks, global clocks, echo, LFOs, and cross-channel pairing.
