# Chip UI component convergence

The September UI review calls for shared interaction rules and modules that match real hardware blocks. A chip page composes those blocks; its native meanings, register mappings, and routing remain specific to that chip.

## Completion criteria

- Inherited choices show their effective values and preserve existing host parameters and state.
- SSG channels use one implementation across YM2149 and the embedded OPN-family SSG sections.
- Reusable controls own their widgets and parameter attachments, with the processor retaining audio and saved-state ownership.
- FM controls use readable labels and useful hit areas within the existing editor heights.
- Shared generators identify their affected channels; helper controls remain distinguishable from native hardware behavior.
- Every chip is audited at 1180 and 1240 pixels. Native source controls and level lanes remain visible.
- UI interaction, parameter/state, audio regression, clean build, and required platform checks pass before integration.

## Implementation sequence

1. Add a bound inherited-choice component with compact and segmented presentations.
2. Extract the SSG channel UI and expose resolved mixer routing consistently.
3. Consolidate FM control ownership and improve operator/mixer space allocation.
4. Audit and refine the remaining chip layouts, common control states, and labels.
5. Verify, integrate through the required checks, and install the committed build.

## Component boundaries

| Component | Shared responsibility | Chip-specific responsibility |
| --- | --- | --- |
| `ChipperChoiceControl` | Parameter attachment, complete host gestures, undo transactions, automation/restore updates, inherited value display, menu/segment layout | Parameter ID, native labels and resolved value supplied by the chip adapter |
| `ChipperSsgChannel` | AY-compatible tone/noise routing choices and effective register readout | AY macro mixer versus embedded OPN preset mixer; surrounding source card placement and levels |
| `ChipperFmEditor` | Owned operator widgets, level and multiplier bindings, readable operator layouts, grouped native envelope menus | OPN/OPM rates versus OPL/OPLL levels/ranges, native detune/flags, carrier roles and register readouts |

Reuse a whole control when its musical meaning and interaction match. Reuse a frame when only the geometry matches. Keep the processor responsible for audio and saved state; UI components observe and edit existing APVTS parameters. Native envelope/detune menu dispatch stays in the chip presentation adapter while its widgets live in the FM component.

## Visible refinements

- NES and DMG Pulse 2 separate the inherited duty from four readable native choices. Short expansion cards use a full-width menu with the resolved duty.
- NES expansion cards remove redundant captions and fit their previews around the native selector and visible level lane. Tests require a usable level slider for every expansion source at both widths, including controls with empty bounds that a generic visibility scan could miss.
- AY and all four embedded OPN SSG pages display `Follow → Tone/Noise/Both/Off`. The AY shared noise control identifies routed channels and calls its post-mixer helper a trim.
- FM multipliers show the effective inherited multiple. Four-operator levels have a separate row; envelope controls use readable names and four focused submenus. OPL/OPLL menus expose their 4-bit rate/level range without changing host parameter IDs or stored values.
- OPL gives the operator panel more width and arranges patch macros in two columns. OPM retains its channel and LFO space and places native detune beside the other operator controls.
- Paula source previews are taller. Obsolete sample-import readouts no longer overlap SPC700/Paula source cards.

## Verification record

The editor size suite checks all 27 chips at 1180 and 1240 pixels, including restored layouts and source ownership. `chipper_ui_choice_smoke` covers complete gestures, read-only inherited display, visible menu refresh, automation, state restore, undo/redo, rebinding/destruction, AY/OPN routing and native envelope menu ranges/dispatch. Release and platform verification results are recorded with the integration commit/PR.
