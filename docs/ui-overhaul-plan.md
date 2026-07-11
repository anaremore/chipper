# UI Overhaul Execution Plan

This plan turns Chipper into one coherent multi-chip instrument while preserving APVTS parameter identity, MIDI CC mappings, preset recall, chip-owned controls, and truthful verification language.

## Completion Contract

The overhaul is complete only when all of the following are implemented and verified:

1. The editor presents one unified chip surface; Play/Edit/Info are not navigation destinations.
2. The entire active signal path remains legible, while focus and density can emphasize the channel being edited.
3. Every public chip has repeatable compact/default editor captures plus a machine-readable component manifest.
4. Preset and chip discovery use the dedicated sound browser, the only separate overlay.
5. Every visible channel retains identity, enable/activity, and an honest level or native amplitude state.
6. Channel-local controls live with their owner; shared sections contain only genuinely cross-channel behavior.
7. Four-operator FM chips share a real algorithm/operator editor with chip-specific controls and honest register detail.
8. Sampler and wavetable chips share clear assignment, waveform, loop, mapping, and missing-asset recovery conventions.
9. Keyboard traversal, visible focus, accessible names, contrast, and interaction states are verified across the editor and browser.
10. Musician workflow tools include A/B comparison, undo/redo, section initialization, copy/paste, and bounded chip-safe variation where supported.

## Progress

- Complete: stable header, summary, workflow, footer, and browser layout extracted into reusable shell components.
- Complete: Play/Edit/Info navigation removed; obsolete programmatic requests and Ctrl/Cmd+1–3 cannot leave the unified editor.
- Complete: dedicated global sound browser with grouped chip navigation, role/text filtering, favorites, recents, recursive user-bank discovery, detail copy, and explicit cross-chip loading.
- Complete: Browse remains open through periodic UI refresh and is kept above the editor until explicitly closed, loaded, or dismissed with Escape.
- Complete: NES exposes five truthful lanes—Pulse 1, Pulse 2, Triangle, Noise, and independently gated DMC. Noise owns mode/period; DMC owns its native DAC/rate/sample path and has no fake conventional level trim.
- Complete: Game Boy / DMG exposes four native lanes with Pulse 1 sweep and Noise clock inside their owning cards, Wave RAM shape/NR32 level inside Wave, shared NRx2 envelope helpers in one truthful module, explicit NR51 routing, and an explicit three-pitched-lane Chip Poly contract.
- Complete: SN76489 / Sega PSG presents three compact tone lanes beside a deeper Noise lane. Tone Stack/Pitch Motion stay spatially under the tone group; Noise owns mode, preset-only bias, native attenuation, and modern trim; Chip Poly explicitly allocates only the three tone lanes.
- Complete: YM2151 / OPM presents all eight four-operator channels in a two-row bank, keeps `$0F` operator-4 noise inside channel 8, exposes Algorithm/Feedback/Operator Tone/FM Level as one shared patch beside the editable operator matrix, and groups native LFO PM/AM depth with per-lane pan routing before a clock/output-only footer.
- Complete: YM2413 / OPLL presents all nine lanes in a stable 3x3 bank, keeps Instrument and `$0E` melodic/rhythm topology together, gives the one shared programmable User0 slot a two-card Mod/Carrier editor, and makes rhythm-only note allocation visible without hiding preserved melodic-lane settings.
- Complete: YM2203 / OPN presents three FM lanes directly above their three paired SSG lanes, keeps each SSG Tone/Noise mix inside its owning lane, separates the shared FM patch/operator matrix from the shared SSG envelope generator, and leaves only the intentional FM/SSG tone and level bridges beside clock/output.
- Complete: YM2608 / OPNA presents six FM and three SSG lanes in a stable 3x3 bank, discloses the six FM-lane controls' additional BD/SD/cymbal/hat/tom/rim ownership in Drum/Hit, separates the shared FM patch/operator matrix and SSG generator from conditional ADPCM memory, and keeps the FM/SSG bridge, native pan, modern width, clock, and output together in one compact footer.
- Complete: actionable sample empty states begin with NES DMC, whose waveform surface offers a direct `Load a .dmc sample` recovery action.
- Complete: shared chip-family classification, browser grouping, density policy, and centralized editor-height rules.
- Complete: shared four-operator FM editor with carrier/modulator grid, per-operator editing, resolved register readouts, and algorithm visualization.
- Complete: sampler/wavetable asset workflows with per-source assignment, loaded/missing status, Wave RAM shape state, loop controls, and standard-size selectors.
- Complete: stable-shell workflow bar with APVTS undo/redo, per-chip A/B slots, guarded whole-sound copy/paste, section initialization, and bounded variation.
- Complete: snapshot tooling now names the primary surface `editor`; `browser` is captured explicitly rather than multiplying obsolete workspace variants.
- In progress: chip-by-chip refinement continues after NES and DMG; each remaining chip must complete its own source-truth audit, implementation, two-width visual review, behavioral coverage, and product-gap record before the final all-chip release audit.

## Delivery Order

### Unified Surface

- Keep the complete active signal path visible.
- Use spatial hierarchy and inline focus instead of top-level editor tabs.
- Move hardware-owned state into the source/operator/block it affects.
- Keep Strictness, note allocation, authenticity, and output as distinct concepts.

### Discovery And Density

- Preserve Browse as the only overlay and give it explicit Close/Escape behavior.
- Use bounded inline master-detail organization for dense chips without hiding the rest of the instrument.

### Deep Editors

- Continue enriching shared FM, sampler, and wavetable components only when engine mappings, automation, preset recall, and renderer coverage exist.
- Add relationship diagrams only where they materially clarify shared hardware resources or modulation routing.

### Quality And Workflow

- Verify focus order, names, contrast, empty states, enabled/disabled states, and overlay z-order.
- Audit every chip at both supported widths.
- Run the full test suite, install the exact clean build, and verify the footer marker.

## UI Capture Workflow

Generate unified-editor PNGs and `manifest.json` for every chip at both supported widths:

```powershell
.\scripts\capture-ui.ps1
```

Generate only the platform-independent structural manifest:

```powershell
.\scripts\capture-ui.ps1 -ManifestOnly
```

Capture the dedicated global sound browser at both supported widths:

```powershell
.\scripts\capture-ui.ps1 -Chip nes -Workspace browser
```

PNG comparisons are reviewed per operating system because font rasterization and graphics backends differ. The JSON manifest is portable CI evidence for hierarchy, bounds, visibility, enabled state, and keyboard focusability.
