# UI Priority Audit

This audit tracks layout and control-placement work that most directly improves Chipper as a usable, chip-aware instrument.

## Completed Pattern

- Game Boy / DMG now treats channel cards as the owner of channel-native register controls: Pulse 1 duty, Pulse 2 duty, Wave RAM shape/level, and Noise width mode.
- Embedded channel controls must suppress their old standalone labels and readouts; otherwise stale labels can reappear after chip switching or parameter refresh.
- HuC6280, Namco WSG, and Konami SCC now follow the same rule: per-channel wave shape selectors live inside their wavetable voice cards instead of a detached Wave/Mixer panel.
- SID already follows this pattern for per-voice waveform and pulse-width controls, with the global filter staying in the Filter panel.

## Highest-Value Next Fixes

1. SNES SPC700-style sample voices
   - Issue: each of the eight voices can become a selected/generated sample source, but the voice cards still mainly display state rather than offering per-voice sample assignment.
   - User value: high. Makes SNES mode feel like an eight-voice sample instrument rather than one global sample player.
   - Confidence: 7/10. Existing sample-bank and voice-card surfaces are usable foundations; engine voice assignment semantics need careful scoping.

2. Wavetable voice polish
   - Issue: HuC6280, Namco WSG, and SCC now place per-voice wave selectors in the cards, but the next pass should verify spacing, labels, and per-chip terminology after real DAW use.
   - User value: high. These chips are fundamentally independent wavetable lanes, so users should shape each lane where they hear and enable it.
   - Confidence: 8/10. Layout and parameters are in place; remaining work is visual QA and any engine gaps found while playing.

3. POKEY channel deck
   - Issue: distortion/noise and AUDCTL pairing are readable, but individual channel AUDF/AUDC/AUDV relationships could be clearer in the four channel cards.
   - User value: medium-high. Helps users understand why POKEY sounds chaotic and how channels pair.
   - Confidence: 7/10. Requires careful UX so AUDCTL cross-channel behavior is visible without crowding.

4. Paula sample channels
   - Issue: sample import is strong, but per-channel sample assignment and hard-pan identity could become more explicit.
   - User value: medium-high. Makes Paula feel like a four-channel tracker sampler.
   - Confidence: 7/10. Similar to SNES, but four channels make the UI easier.

5. FM operator editors
   - Issue: FM chips are playable, but the current UI is still too macro/operator-summary driven for serious FM editing.
   - User value: very high for FM users.
   - Confidence: 6/10. Needs a larger design slice: algorithm graph, operator grid, per-operator envelopes, and readable register values.

## Design Rule

When a control maps to a channel-local register, place it in or directly under that channel's card. Reserve shared panels for genuinely shared hardware: filters, output routing, sample banks, global clocks, echo, LFOs, and cross-channel pairing.
