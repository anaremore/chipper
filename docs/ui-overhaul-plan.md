# UI Overhaul Execution Plan

This plan turns the current all-controls-at-once editor into a scalable instrument workflow while preserving APVTS parameter identity, MIDI CC mappings, chip-owned control placement, preset recall, and truthful verification language.

## Completion Contract

The overhaul is complete only when all of the following are implemented and verified:

1. Play, Edit, and Info workspaces provide progressive disclosure without changing sound or automation state.
2. The stable shell and chip-family workspaces replace repeated per-chip layout policy where behavior is shared.
3. Every public chip is covered by repeatable compact/default UI captures plus a machine-readable component manifest.
4. Preset and chip discovery use a dedicated role-first browser rather than a cramped header-only workflow.
5. High-source-count chips use a master-detail source workflow while keeping source enable, identity, and level visible in each card.
6. Four-operator FM chips share a real algorithm/operator editor with chip-specific controls and honest register detail.
7. Sampler and wavetable chips share clear assignment, waveform, loop, mapping, and missing-asset recovery conventions.
8. Cross-channel relationships and native-versus-Chipper helper behavior are visible and understandable.
9. Keyboard traversal, visible focus, accessible names, contrast, and interaction states are verified across workspaces.
10. Musician workflow tools include A/B comparison, undo/redo, section initialization, copy/paste, and bounded chip-safe variation where supported.

## Progress

- Complete: repeatable PNG capture plus a portable structural manifest for every chip, workspace, and supported width.
- Complete: stable header/summary/footer layout extracted into `ChipperEditorShell`.
- Complete: Play, Edit, and Info workspace foundation with APVTS-safe switching, UI-only persistence, all-chip layout coverage, and parameter-mutation guards.
- Complete: intent-specific workspace hierarchy: Play leads with large performance controls and compact chip-aware sources, Edit keeps the authoritative chip-native construction surface, and Info replaces developer-first text boxes with authenticity, capability, limitation, and collapsible evidence views.
- Complete: low-density Play source cards use 2x2 performance decks, descriptor-backed quick controls, source-state indicators, and waveform identity glyphs; sampler, wavetable, and high-source-count modes retain master-detail precision editing.
- Complete: actionable sample empty states begin with NES DMC, whose waveform surface now offers a direct `Load a .dmc sample` recovery action.
- Complete: shared chip-family classification, browser grouping, density policy, and centralized Edit-layout height rules.
- Complete: selected-source master-detail editing for every chip with seven or more visible lanes while retaining card-level identity, enable, and level controls.
- Complete: dedicated global sound browser with grouped chip navigation, role and text filtering, favorites, recents, recursive user-bank discovery, detail copy, and explicit cross-chip loading.
- Complete: shared four-operator FM editor with a 2x2 carrier/modulator grid, per-operator level/multiplier/envelope editing, resolved register readouts, and shared algorithm visualization.
- Complete: unified sampler/wavetable selected-asset workflow with per-source assignment, loaded/missing status, Wave RAM shape state, precision level, and direct Edit recovery actions.
- Complete: chip-aware relationship schematics with distinct Native and Chipper affordances for routing, modulation, shared resources, and musical helpers.
- Complete: shared high-contrast focus outline, explicit cross-surface focus order, accessible control names, browser keyboard behavior, and workspace/global shortcuts.
- Complete: stable-shell musician workflow bar with grouped APVTS undo/redo, per-chip A/B audition slots, guarded whole-sound copy/paste, section initialization, and bounded macro/source-level variation.
- Complete: final audit across 27 chips, Play/Edit/Info, both supported widths, and the dedicated browser (164 retained PNGs plus manifests); all 826 tests pass for the current implementation.

## Delivery Order

### Foundation

- Add PNG capture tooling for human visual review at 1180 and 1240 px.
- Export a structural component manifest so CI can detect hierarchy, visibility, focusability, and bounds regressions without relying on platform-identical pixels.
- Add a workspace deck whose switches are presentation-only.
- Extract the stable shell and family layout policies before adding more one-off chip branches.

### Discovery And Density

- Add the global preset/chip browser with role, chip family, engine, tag, favorites, recents, user banks, and explicit cross-chip loading.
- Convert dense source decks to compact cards plus a selected-source detail editor.

### Deep Editors

- Build the shared FM algorithm/operator editor.
- Build shared sampler and wavetable editors with clear asset and mapping state.
- Add relationship diagrams for pairing, modulation, routing, and shared resources.

### Quality And Workflow

- Complete the accessibility and interaction-state pass.
- Add musician workflow accelerators and final visual polish.
- Audit every chip at both supported widths, run the full relevant test suite, install the exact build, and verify the footer marker.

## UI Capture Workflow

Generate PNGs and `manifest.json` for every chip, workspace, and supported width:

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

PNG comparisons are reviewed per operating system because font rasterization and graphics backends differ. The JSON manifest is the portable CI evidence for component hierarchy, bounds, visibility, enabled state, and keyboard focusability.
