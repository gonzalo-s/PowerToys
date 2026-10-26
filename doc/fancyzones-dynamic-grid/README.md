# FancyZones dynamic grid resize (experimental fork)

Experimental change to PowerToys FancyZones: dragging the edge of a snapped window moves the windows that touch that edge, while the layout's original grid lines stay as the guide for snapping back. Not an official PowerToys feature.

Branch: `feature/dynamic-grid-resize`, on top of upstream commit `a5e83e5`.

## Behaviour
- Only windows touching the dragged edge follow it (rectangle model, segment-only). Empty zones never block a resize.
- The overlay and snap zones always show the original layout. Windows follow a separate in-memory working geometry (lost when FancyZones restarts).
- Magnet: a dragged edge snaps to the nearest original gridline within 20 px. Cue lines: blue = original gridlines, green = the edge's own original line, white = another original line it will snap to.
- Minimum zone size 50 px; an edge can reach the work-area border; outer edges resize naturally.
- Snapping a new window puts it on its original zone and moves back only the windows that overlap it.
- Windows spanning several zones are squeezed as one block.
- Resize drags are logged to the FancyZones log (`%LOCALAPPDATA%\Microsoft\PowerToys\FancyZones\Logs`).

## Where the code is
`src/modules/fancyzones/FancyZonesLib`: `ZoneRectModel`, `WorkingLayouts`, `WindowResizeHandler`, plus changes in `WorkArea`, `Layout`, `LayoutConfigurator` and `FancyZones.cpp`. Unit tests in `src/modules/fancyzones/FancyZonesTests/UnitTests`.

## Building and testing
Needs Visual Studio 2022 (C++ desktop workload) and Windows long paths enabled. Build the unit-test project with `tools\build\build.cmd` from `src\modules\fancyzones\FancyZonesTests\UnitTests` (NuGet restore for that project needs `/p:RestorePackagesConfig=true /p:SolutionDir=<repo root>\`). Run the tests with `vstest.console.exe` using a `/TestCaseFilter` per class: a whole-DLL run crashes at the end (a known problem that was there before these changes) and `WindowKeyboardSnap_MoveAcrossMonitors_ByPosition_UnitTests::Snap_Down` already failed before these changes.

## Docs
- `design-spec.md`: the design and its status; `implementation-plan.md`: the original plan (the design changed later, see the status section of the spec).
- `dynamic-grid-demo.html`: an interactive HTML model of the first version of the idea.
