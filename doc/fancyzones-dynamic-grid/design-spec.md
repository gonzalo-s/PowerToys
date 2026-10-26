# FancyZones dynamic grid resize: design

Date: 2026-10-24
Target: local fork of microsoft/PowerToys (`PowerToys/src/modules/fancyzones`), not a companion app.

## Goal

Start from a base FancyZones grid layout. When the user resizes a snapped window by dragging its edge, the shared divider moves and every other zone and snapped window adjusts so that all windows stay snapped, with no overlaps and no gaps. The user can drag the divider back to restore the original geometry, helped by a visual cue showing the original gridlines.

## Scope clarification

"Resize" always means direct manipulation of a snapped application window's edge or corner. Nothing changes in the FancyZones editor, the layout picker, or the zone overlay UI, apart from the new gridline cue.

## Decisions (agreed with the user)

| Topic | Decision |
|---|---|
| Delivery | Modify PowerToys FancyZones itself (no companion app) |
| Divider semantics | Whole divider line moves (grid-style, as today's grid layouts work) |
| Reset gesture | Drag the divider back by hand; snap to the base line when within a few pixels; show base gridlines as a visual cue |
| Update timing | Live while dragging (throttled) |
| Persistence | In-memory only, per work area (monitor + virtual desktop). Survives hibernate/resume, lost when PowerToys exits |
| Layout types | Grid-based only (Columns, Rows, Grid, PriorityGrid, custom Grid). Focus and Canvas are unchanged |

## Findings in the existing code

- Window events already reach `FancyZones::HandleWinHookEvent` (`MOVESIZESTART`, `MOVESIZEEND`, `LOCATIONCHANGE`).
- `WindowMouseSnap::Create` returns `nullptr` for resize-cursor drags, so resizes are ignored today. A resized window stays snapped in memory.
- Zones are computed, not stored: `Layout::Init` -> `LayoutConfigurator::CalculateGridZones` from `rowsPercents`, `columnsPercents`, `cellChildMap` (units of 1/10000 of the work area) plus spacing.
- `applied-layouts.json` stores no geometry (uuid, type, spacing, zone count per work area).
- `WorkArea::UpdateWindowPositions()` already moves all snapped windows to their zone rectangles.
- The drag overlay and keyboard snapping read `m_layout->Zones()`, so they follow a changed layout automatically.
- Work areas can be recreated on display change (`OnDisplayChange`, `ShouldWorkAreasBeRecreated`).

## Design

### Working layout store

A singleton `WorkingLayouts` store (pattern of `AppliedLayouts`), in memory only. Keyed by work area id (monitor + virtual desktop). Each entry holds the base layout uuid, the base grid shape (rows, columns, `cellChildMap`) and the modified `rowsPercents` / `columnsPercents`. Spacing and `cellChildMap` are never overridden.

- `WorkArea::CalculateZoneSet` builds the layout from the working percentages when an entry exists, otherwise from the base layout. Because the store outlives `WorkArea` objects, recreated work areas pick their entry back up.
- Template layouts are converted to `GridLayoutInfo` on demand so all grid types use one path.
- An entry is discarded when the applied layout uuid changes or the base grid shape no longer matches (layout edited in the editor).
- The base layout and the saved JSON files are never modified.

### Resize flow

1. `MOVESIZESTART` of a snapped window with a resize cursor: remember its visible rectangle (`DWMWA_EXTENDED_FRAME_BOUNDS`) and its zone set. This path currently bails out early.
2. `LOCATIONCHANGE` (throttled, about 30 to 60 Hz): compare the visible rectangle with the remembered one to find which edges moved. Map each moved edge to a row or column boundary index from the window's zones. Edges on the work-area border are ignored.
3. Compute the new boundary percentage, clamped so no zone falls below a minimum size. Snap it to the base value when within a few pixels.
4. Rebuild zones and call `UpdateWindowPositions()` for the other snapped windows. A re-entrancy flag ignores the `LOCATIONCHANGE` events caused by our own repositioning.
5. `MOVESIZEEND`: final pass using the window's actual rectangle (apps may refuse part of the resize). If every boundary equals its base value, delete the entry.

### Reset

Dragging a divider back to a base gridline restores it (magnetic snap). When all dividers match the base, the working layout is removed.

### Base gridline cue

While an edge is being dragged, `ZonesOverlay` draws the base gridlines across the work area, and highlights the dragged divider when it is within snapping distance. While any divider differs from its base, the base lines may stay faintly visible (optional setting, off by default).

### Edge cases

- Minimum size: each zone is clamped to a floor (about 10% of the work area, final value to be tuned).
- Non-resizable windows: cannot start a resize, but are moved by neighbor dividers and keep their size.
- Multi-zone windows: can only move dividers on their outer sides.
- Multiple monitors and DPI: percentages are per work area, so monitors are independent and DPI changes re-flow.
- Closed or unsnapped windows: the working layout stays; zones do not depend on windows.
- Win+arrow and drag-to-zone use the working layout, so they stay consistent.
- Apps that enforce their own minimum size: the final pass uses the real rectangle and clamps the divider.

## Status after implementation (2026-10-26, wave 5)

Implemented on branch `feature/dynamic-grid-resize` of the local PowerToys clone (HEAD c84e8c8f9 after wave 5). Unit tests pass (per-class counts: ZoneRectModel 27, WorkArea filter 70 incl. WorkAreaEdgeResize 25, LayoutUnitTests 16, LayoutConfiguratorGrid 23, WorkingLayouts 9, AppliedLayouts 25, WindowKeyboardSnap 53 + 1 known pre-existing failure). Real-window behaviour is verified only by the manual checklist in `fancyzones-test-build\README.md` (test build `PowerToys.FancyZones.wave5.exe`).

How it works now (supersedes parts of the design above):
- Segment-only edges (rectangle model) replaced the whole-gridline divider model: the layout is kept as independent zone rectangles (`ZoneRectModel`, units of 1/10000), and dragging an inner edge moves only the wall segment the dragged window touches (plus whatever keeps the tiling consistent, e.g. a wide window spanning below). The originally out-of-scope "segment-only dividers" are therefore implemented.
- The original grid lines never change. The layout used by the Shift-drag overlay, drop targets, keyboard snapping and `ZonesFromPoint` stays the applied (original) layout. A resize only changes separate working zones that position the snapped windows. Example: two windows 50/50, drag the left one to 75: the right window follows to 25, the overlay still shows 50/50.
- A user-initiated snap (mouse drop after Shift-drag, Win+Arrow / keyboard snap, extend) resets the working layout of that work area: the window lands on its original zone, and every other snapped window that is elsewhere returns to its original zone. Internal re-snaps (startup, display / DPI / virtual-desktop changes, a new window placed into its last zone) keep the working geometry.
- Magnet: a dragged edge within 20 px of its original line snaps to it (green cue line; blue = original gridline). Releasing inside the magnet zone puts the edge exactly on the original line, and when no edge differs any more the working layout is dropped.
- Minimum zone size: 50 px (capped per zone by its current and base size). Edges on the work-area border keep the natural FancyZones resize (nothing else moves, no cue).
- Release position: pixel edges are converted to units with a ceiling division, so the rebuilt zone edge equals the released pixel edge; the dragged window is repositioned on release only when a dragged divider edge differs from the release position by more than 1 px (magnet snap or minimum-size clamp).
- Constants: minimum zone size 50 px; snap radius 20 px; throttle 16 ms; moved-edge tolerance 2 px.
- Wave 6 (commits 8a258ab95, 347cb7bbe): the magnet snaps to the nearest of ALL original gridlines on the axis within 20 px (tie: own line, then lower), before the clamp; the cue draws dedicated 1 px guide lines for the original gridline segments (blue; with spacing through the centre of the gap), the snapped line green when it is the edge's own line and white otherwise. Release rule: the edge ends exactly where released, exactly on an original gridline, or at a clamp limit (test build `PowerToys.FancyZones.wave6.exe`).
- Wave 9 (commits 601532978, a099ed5b6; supersedes the wave 5 reset bullet above): a user-initiated snap no longer resets the whole working layout. `WorkArea::PrepareUserSnap` / `ZoneRectModel::ResolveSnap` put the target zones back on their original rects plus only the windows that conflict (current rect overlaps, with positive area, the original rect of a reset zone; repeated to a fixpoint; a multi-zone window as one unit); every other window keeps its resized size, and the entry is dropped when nothing differs any more (test build `PowerToys.FancyZones.wave9.exe`).
- Wave 10 (commit 674c135): multi-zone windows are squeezed as a whole. `ZoneRectModel::MoveEdge` takes the snapped windows' zone sets (plus the dragged block) as units; a squeezed zone at its minimum pushes the zones of its own unit right behind it (each keeping min(50 px, current, base)), so a window's inner gridlines no longer stop the edge (user report: 6x9 custom layout, edge stopped at 2448 / 1138 px); every zone of a pushed window past the new wall is pushed too (no bounding-rect overlap). A window's minimum is therefore 50 px per zone column/row it spans. `WorkArea` logs, once per drag, a neighbour whose actual rect stays larger than the requested one (app minimum size); the edge is not held there (test build `PowerToys.FancyZones.wave10.exe`).

Resolved items (unchanged from earlier waves):
- The live path uses `WorkArea::RepositionSnappedWindows` (async SetWindowPos, no history writes) and skips minimized, maximized, hidden and cloaked windows; only windows whose zone rect changed are moved.
- Working layouts are keyed by `WorkAreaId::toString()` and survive work-area recreation (hibernate/resume still to confirm by hand).

Deviations from this design:
- Columns/Rows templates derive their base rectangles from the template geometry (work-area dependent), so the working layout is discarded after a resolution change for those templates.
- The cue is drawn only while dragging an inner edge, from the original zones, as 1 px outlines (blue; green inside the magnet zone). No persistent cue.
- Win+Arrow and drag-to-zone use the original layout (not the working layout, as this design first said) and reset the working layout.
- No setting to turn the feature off; working layouts are in memory only.

Open items: real-app testing with the wave 5 build (checklist in `fancyzones-test-build\README.md`), mixed-DPI check, hibernate/resume, behaviour with elevated neighbours (cannot be moved), apps that enforce their own minimum size (the window may not match its working zone), multi-monitor keyboard moves (only the target work area is reset).

## Out of scope

Segment-only dividers (tiling-window-manager style), Focus and Canvas layouts, persistence across PowerToys restarts, changes to the editor or settings UI (except an optional cue setting).

## Build and distribution notes

Requires Visual Studio and a self-built, unsigned runner; the installed PowerToys must be stopped to test. Check that this is allowed on the managed machine. If upstreaming, `AGENTS.md` requires a human-written PR description.
