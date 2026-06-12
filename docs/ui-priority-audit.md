# UI Priority Audit

This audit tracks layout and control-placement work that most directly improves Chipper as a usable, chip-aware instrument.

## Completed Pattern

- Game Boy / DMG now treats channel cards as the owner of channel-native register controls: Pulse 1 duty, Pulse 2 duty, Wave RAM shape/level, and Noise width mode.
- Embedded channel controls must suppress their old standalone labels and readouts; otherwise stale labels can reappear after chip switching or parameter refresh.
- HuC6280, Namco WSG, and Konami SCC now follow the same rule: per-channel wave shape selectors live inside a taller wavetable voice deck instead of a detached Wave/Mixer panel, with level strips reserved so wave controls cannot crowd out channel gain.
- Amiga Paula now follows the channel-local rule too: each of the four hard-panned sample channels exposes its own generated sample shape selector and optional loaded sample-bank slot selector inside a taller sampler-style channel card, matching Paula's independent DAC/sample playback model. The shared sample-bank panel stays focused on file/folder import, note mapping, and waveform preview; the old global Sample Shape segment stays hidden for Paula.
- SID already follows this pattern for per-voice waveform and pulse-width controls, with the global filter staying in the Filter panel.

## Highest-Value Next Fixes

1. SNES SPC700-style sample voices
   - Issue: each of the eight voices can become a selected/generated sample source, but the voice cards still mainly display state rather than offering per-voice sample assignment.
   - User value: high. Makes SNES mode feel like an eight-voice sample instrument rather than one global sample player.
   - Confidence: 7/10. Existing sample-bank and voice-card surfaces are usable foundations; engine voice assignment semantics need careful scoping.

2. Wavetable voice polish
   - Issue: HuC6280, Namco WSG, and SCC now place per-voice wave selectors and protected level strips in the cards, but the next pass should verify whether they also need per-lane pan/pitch controls in the same cards.
   - User value: high. These chips are fundamentally independent wavetable lanes, so users should shape each lane where they hear and enable it.
   - Confidence: 8/10. Layout and parameters are in place; remaining work is deeper per-lane editing and any engine gaps found while playing.

3. POKEY channel deck
   - Issue: channel cards now expose AUDC in the header and AUDF/AUDV in the per-channel strip, but AUDCTL pairing/filter paths still need a stronger visual relationship between linked channels.
   - User value: medium-high. Helps users understand why POKEY sounds chaotic and how channel pairing changes pitch behavior.
   - Confidence: 7/10. Register readouts are now straightforward; the remaining work is a clearer cross-channel pairing visual without crowding the deck.

4. Paula tracker import depth
   - Issue: channel-local generated shapes, loaded sample-slot pins, and matching renderer flags are in place, but MOD sample extraction and imported loop metadata remain planned.
   - User value: medium-high. Makes Paula feel more like a four-channel tracker sampler for real module-style workflows.
   - Confidence: 6/10. The UI and runtime bank model are usable; the remaining work needs file-format parsing and stricter validation.

5. FM operator editors
   - Issue: FM chips are playable, but the current UI is still too macro/operator-summary driven for serious FM editing.
   - User value: very high for FM users.
   - Confidence: 6/10. Needs a larger design slice: algorithm graph, operator grid, per-operator envelopes, and readable register values.

## Design Rule

When a control maps to a channel-local register, place it in or directly under that channel's card. Reserve shared panels for genuinely shared hardware: filters, output routing, sample banks, global clocks, echo, LFOs, and cross-channel pairing.
