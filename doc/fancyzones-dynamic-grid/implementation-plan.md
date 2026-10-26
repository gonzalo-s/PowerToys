# FancyZones Dynamic Grid Resize Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the user drags the edge of a snapped window, the shared grid divider moves, every other zone and snapped window follows live with no overlaps, and the original gridlines are shown as a cue while dragging.

**Architecture:** A pure `DividerModel` holds a base `GridLayoutInfo` plus a modified copy and moves whole divider lines (clamped, with magnetic snap to the base line). An in-memory `WorkingLayouts` singleton, keyed by work area, stores one model per work area. `WorkArea::CalculateZoneSet` builds zones from the working percentages when an entry exists, so the overlay, keyboard snapping and drag-to-zone follow automatically. A `WindowResizeHandler` owned by `FancyZones` turns resize-cursor drags (`MOVESIZESTART` / `LOCATIONCHANGE` / `MOVESIZEEND`) into divider moves and calls `WorkArea::UpdateWindowPositions`.

**Tech Stack:** C++20, Win32 (DWM frame bounds, `SetWindowPlacement`), Direct2D overlay (`ZonesOverlay`), Microsoft CppUnitTest, PowerToys build scripts (`tools\build\build.cmd`).

## Global Constraints

Copied from the spec `docs/superpowers/specs/2026-10-24-fancyzones-dynamic-grid-resize-design.md`:

- Modify PowerToys FancyZones itself (no companion app). Code lives in `PowerToys/src/modules/fancyzones`.
- Whole divider line moves (grid-style, as today's grid layouts work). Segment-only dividers are out of scope.
- Grid-based layouts only: Columns, Rows, Grid, PriorityGrid, custom Grid. Focus and Canvas are unchanged.
- Working layout is in memory only, per work area (monitor + virtual desktop). It survives hibernate/resume and is lost when PowerToys exits. No new JSON file.
- The base layout and the saved JSON files (`applied-layouts.json`, `custom-layouts.json`) are never modified.
- Resize updates are live while dragging, throttled.
- Reset is manual: drag the divider back; it snaps to the base line within a few pixels; base gridlines are drawn as a cue while dragging.
- Edges on the work-area border do nothing. A window spanning several zones can only move dividers on its outer sides.
- Zones are clamped to a minimum size so no zone is smaller than the minimum (this plan fixes it at 90 px, tunable in Task 7).
- An entry is discarded when the applied layout id changes or the base grid (rows, columns, percents, `cellChildMap`) no longer matches.
- Every change touching `src/modules/fancyzones` follows `PowerToys/AGENTS.md`: build with `tools\build\build.cmd`, exit code 0 means success, add or adjust tests when behavior changes. If this is ever upstreamed, the visible PR description must be written by a human.

**Deviation from the spec (deliberate, YAGNI):** the optional "faint base lines stay visible while modified" setting is not in this plan. The cue is shown only while an edge is being dragged, drawn as 1 px solid zone outlines (blue; green when the dragged divider is at its base position) because `ZonesOverlay` already draws zone borders and has no dashed-line support.

## File Structure

All paths relative to `PowerToys/src/modules/fancyzones/`.

| File | Action | Responsibility |
|---|---|---|
| `FancyZonesLib/DividerModel.h` / `.cpp` | Create | Pure divider logic: spans, moved-edge detection, `Move` with clamp and snap. No Win32 state. |
| `FancyZonesLib/WorkingLayouts.h` / `.cpp` | Create | In-memory singleton: one `DividerModel` per work area, stale-entry discard. |
| `FancyZonesLib/WindowResizeHandler.h` / `.cpp` | Create | Tracks one resize drag: reads DWM frame, accumulates moved edges, throttles, calls `WorkArea`. |
| `FancyZonesLib/LayoutConfigurator.h` / `.cpp` | Modify | Add `GridInfo(LayoutData)` and `GridZones(...)`; extract Grid/Columns/Rows info builders. |
| `FancyZonesLib/Layout.h` / `.cpp` | Modify | Add `InitFromGrid(...)` and `Spacing()`. |
| `FancyZonesLib/WorkArea.h` / `.cpp` | Modify | Use working layout in `CalculateZoneSet`; add `ApplyEdgeResize`, `FinishEdgeResize`, `ShowResizeCue`; `UpdateWindowPositions(skip)`. |
| `FancyZonesLib/FancyZones.cpp` | Modify | Own a `WindowResizeHandler`; call it from move/size start, update, end, abort, destroy. |
| `FancyZonesLib/FancyZonesLib.vcxproj` | Modify | Register new files. |
| `FancyZonesTests/UnitTests/DividerModel.Spec.cpp` | Create | Tests for `DividerModel`. |
| `FancyZonesTests/UnitTests/WorkingLayouts.Spec.cpp` | Create | Tests for `WorkingLayouts`. |
| `FancyZonesTests/UnitTests/LayoutConfigurator.Spec.cpp` | Create | Tests for `GridInfo` / `GridZones`. |
| `FancyZonesTests/UnitTests/Layout.Spec.cpp` | Modify | Test `InitFromGrid`. |
| `FancyZonesTests/UnitTests/WorkArea.Spec.cpp` | Modify | Test `ApplyEdgeResize` / `FinishEdgeResize`. |
| `FancyZonesTests/UnitTests/UnitTests.vcxproj` | Modify | Register new test files. |

Unit-test build/run commands used throughout (PowerShell, from the `PowerToys` repo root):

```powershell
# build the unit test project (exit code 0 = success)
Push-Location src\modules\fancyzones\FancyZonesTests\UnitTests
..\..\..\..\..\tools\build\build.cmd
$build = $LASTEXITCODE
Pop-Location
$build

# run tests whose name contains a filter
$vstest = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find "**\vstest.console.exe" | Select-Object -First 1
& $vstest x64\Debug\tests\UnitTests\UnitTests.dll /TestCaseFilter:"FullyQualifiedName~DividerModel"
```

If `UnitTests.dll` is not at that path, locate it with `Get-ChildItem x64 -Recurse -Filter UnitTests.dll`.

---

### Task 0: Full checkout, branch and baseline

The current clone is a sparse checkout containing only `src/modules/fancyzones` and `src/common`. A real build needs the whole repo.

**Files:** none.

- [ ] **Step 1: Expand the checkout and create the branch**

```powershell
cd <workspace>\PowerToys
git sparse-checkout disable
git submodule update --init --recursive
git checkout -b feature/dynamic-grid-resize
git status --short
```

Expected: `git status --short` prints nothing (clean tree) and the branch is `feature/dynamic-grid-resize`.

- [ ] **Step 2: Restore NuGet packages and build the baseline unit tests**

```powershell
tools\build\build-essentials.cmd
Push-Location src\modules\fancyzones\FancyZonesTests\UnitTests
..\..\..\..\..\tools\build\build.cmd
$LASTEXITCODE
Pop-Location
```

Expected: `0`. On failure read `artifacts\logs\UnitTests\build.Debug.x64.errors.log` (path pattern from `AGENTS.md`). Do not continue until the baseline builds.

- [ ] **Step 3: Run the baseline tests**

```powershell
$vstest = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find "**\vstest.console.exe" | Select-Object -First 1
& $vstest x64\Debug\tests\UnitTests\UnitTests.dll
```

Expected: all existing tests pass (note any pre-existing failures so they are not mistaken for regressions later).

---

### Task 1: DividerModel (pure divider logic)

**Files:**
- Create: `FancyZonesLib/DividerModel.h`
- Create: `FancyZonesLib/DividerModel.cpp`
- Modify: `FancyZonesLib/FancyZonesLib.vcxproj` (add `<ClInclude Include="DividerModel.h" />` next to `DraggingState.h`, and `<ClCompile Include="DividerModel.cpp" />` next to `DraggingState.cpp`)
- Create: `FancyZonesTests/UnitTests/DividerModel.Spec.cpp`
- Modify: `FancyZonesTests/UnitTests/UnitTests.vcxproj` (add `<ClCompile Include="DividerModel.Spec.cpp" />` next to `Layout.Spec.cpp`)

**Interfaces:**
- Produces (used by Tasks 2 to 6):
  - `namespace FancyZonesDividers`
  - `constexpr int TotalUnits = 10000;`
  - `enum class Axis { Column, Row };`
  - `struct Divider { Axis axis; int index; };` (`index` is the boundary between track `index - 1` and track `index`, valid range `1..count-1`)
  - `struct Span { int firstRow; int lastRow; int firstCol; int lastCol; };`
  - `struct MovedEdges { bool left, top, right, bottom; bool any() const; MovedEdges& operator|=(const MovedEdges&); };`
  - `MovedEdges DetectMovedEdges(const RECT& before, const RECT& after, long tolerance);`
  - `class DividerModel` with: `explicit DividerModel(const GridLayoutInfo& base)`, `const GridLayoutInfo& Base() const`, `const GridLayoutInfo& Current() const`, `bool SameBase(const GridLayoutInfo&) const`, `std::optional<Span> SpanOf(const ZoneIndexSet&) const`, `std::vector<Divider> DividersFor(const Span&, const MovedEdges&) const`, `int Position(Divider) const`, `int BasePosition(Divider) const`, `bool IsModified(Divider) const`, `bool IsModified() const`, `bool Move(Divider, int position, int minTrack, int snapRadius)`. Positions and sizes are in units of 1/10000 of the work area.

- [ ] **Step 1: Write the failing tests**

Create `FancyZonesTests/UnitTests/DividerModel.Spec.cpp`:

```cpp
#include "pch.h"

#include <FancyZonesLib/DividerModel.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesDividers;
using FancyZonesDataTypes::GridLayoutInfo;

namespace FancyZonesUnitTests
{
    namespace
    {
        GridLayoutInfo MakeGrid(const std::vector<int>& rows, const std::vector<int>& cols, const std::vector<std::vector<int>>& map)
        {
            return GridLayoutInfo(GridLayoutInfo::Full{
                .rows = static_cast<int>(rows.size()),
                .columns = static_cast<int>(cols.size()),
                .rowsPercents = rows,
                .columnsPercents = cols,
                .cellChildMap = map,
                .showSpacing = false,
                .spacing = 0,
                .sensitivityRadius = 0 });
        }

        // 2x2 grid, zones 0 1 / 2 3
        GridLayoutInfo Grid2x2() { return MakeGrid({ 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } }); }

        // 2 rows x 3 columns, zone 1 spans both rows in the middle column
        GridLayoutInfo Priority5() { return MakeGrid({ 5000, 5000 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 3, 1, 4 } }); }
    }

    TEST_CLASS (DividerModelUnitTests)
    {
    public:
        TEST_METHOD (SpanOfSingleZone)
        {
            DividerModel model(Grid2x2());
            const auto span = model.SpanOf({ 3 });
            Assert::IsTrue(span.has_value());
            Assert::AreEqual(1, span->firstRow);
            Assert::AreEqual(1, span->lastRow);
            Assert::AreEqual(1, span->firstCol);
            Assert::AreEqual(1, span->lastCol);
        }

        TEST_METHOD (SpanOfMergedZone)
        {
            DividerModel model(Priority5());
            const auto span = model.SpanOf({ 1 });
            Assert::IsTrue(span.has_value());
            Assert::AreEqual(0, span->firstRow);
            Assert::AreEqual(1, span->lastRow);
            Assert::AreEqual(1, span->firstCol);
            Assert::AreEqual(1, span->lastCol);
        }

        TEST_METHOD (SpanOfUnknownZone)
        {
            DividerModel model(Grid2x2());
            Assert::IsFalse(model.SpanOf({ 42 }).has_value());
        }

        TEST_METHOD (OuterEdgesHaveNoDivider)
        {
            DividerModel model(Grid2x2());
            const auto span = model.SpanOf({ 0 }).value();
            MovedEdges moved;
            moved.left = true;
            moved.top = true;
            Assert::AreEqual(size_t{ 0 }, model.DividersFor(span, moved).size());
        }

        TEST_METHOD (InnerEdgesMapToDividers)
        {
            DividerModel model(Grid2x2());
            const auto span = model.SpanOf({ 0 }).value();
            MovedEdges moved;
            moved.right = true;
            moved.bottom = true;
            const auto dividers = model.DividersFor(span, moved);
            Assert::AreEqual(size_t{ 2 }, dividers.size());
            Assert::IsTrue(dividers[0].axis == Axis::Column);
            Assert::AreEqual(1, dividers[0].index);
            Assert::IsTrue(dividers[1].axis == Axis::Row);
            Assert::AreEqual(1, dividers[1].index);
        }

        TEST_METHOD (MergedZoneUsesItsOuterDividers)
        {
            DividerModel model(Priority5());
            const auto span = model.SpanOf({ 1 }).value();
            MovedEdges moved;
            moved.left = true;
            moved.right = true;
            const auto dividers = model.DividersFor(span, moved);
            Assert::AreEqual(size_t{ 2 }, dividers.size());
            Assert::AreEqual(1, dividers[0].index); // left edge of column 1
            Assert::AreEqual(2, dividers[1].index); // right edge of column 1
        }

        TEST_METHOD (MoveMovesWholeLine)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 6000, 500, 0));
            Assert::AreEqual(6000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(4000, model.Current().columnsPercents()[1]);
            // rows untouched
            Assert::AreEqual(5000, model.Current().rowsPercents()[0]);
            Assert::AreEqual(5000, model.Current().rowsPercents()[1]);
            Assert::AreEqual(6000, model.Position({ Axis::Column, 1 }));
        }

        TEST_METHOD (MoveClampsToMinimumTrack)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 9900, 1000, 0));
            Assert::AreEqual(9000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(1000, model.Current().columnsPercents()[1]);

            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 10, 1000, 0));
            Assert::AreEqual(1000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(9000, model.Current().columnsPercents()[1]);
        }

        TEST_METHOD (MoveInThreeColumnsKeepsOtherTracks)
        {
            DividerModel model(Priority5());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 3000, 500, 0));
            Assert::AreEqual(3000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(4500, model.Current().columnsPercents()[1]);
            Assert::AreEqual(2500, model.Current().columnsPercents()[2]);
        }

        TEST_METHOD (MoveSnapsToBaseWithinRadius)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 6000, 500, 50));
            Assert::IsTrue(model.IsModified());
            Assert::IsTrue(model.IsModified({ Axis::Column, 1 }));

            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 5030, 500, 50));
            Assert::AreEqual(5000, model.Position({ Axis::Column, 1 }));
            Assert::IsFalse(model.IsModified());
            Assert::IsFalse(model.IsModified({ Axis::Column, 1 }));
        }

        TEST_METHOD (MoveReportsNoChange)
        {
            DividerModel model(Grid2x2());
            Assert::IsFalse(model.Move({ Axis::Column, 1 }, 5000, 500, 0));
            Assert::IsFalse(model.Move({ Axis::Column, 1 }, 5010, 500, 50)); // snaps to the same base value
        }

        TEST_METHOD (MoveRejectsInvalidDivider)
        {
            DividerModel model(Grid2x2());
            Assert::IsFalse(model.Move({ Axis::Column, 0 }, 100, 500, 0));
            Assert::IsFalse(model.Move({ Axis::Column, 2 }, 100, 500, 0));
            Assert::IsFalse(model.Move({ Axis::Row, 5 }, 100, 500, 0));
        }

        TEST_METHOD (SameBaseDetectsChanges)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.SameBase(Grid2x2()));
            Assert::IsFalse(model.SameBase(Priority5()));
            Assert::IsFalse(model.SameBase(MakeGrid({ 5000, 5000 }, { 4000, 6000 }, { { 0, 1 }, { 2, 3 } })));
            Assert::IsFalse(model.SameBase(MakeGrid({ 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 0, 3 } })));
        }

        TEST_METHOD (DetectMovedEdgesUsesTolerance)
        {
            const RECT before{ 100, 100, 500, 400 };
            const RECT after{ 101, 90, 560, 400 };
            const auto moved = DetectMovedEdges(before, after, 2);
            Assert::IsFalse(moved.left);
            Assert::IsTrue(moved.top);
            Assert::IsTrue(moved.right);
            Assert::IsFalse(moved.bottom);
            Assert::IsTrue(moved.any());
            Assert::IsFalse(DetectMovedEdges(before, before, 2).any());
        }

        TEST_METHOD (MovedEdgesAccumulate)
        {
            MovedEdges a;
            a.left = true;
            MovedEdges b;
            b.bottom = true;
            a |= b;
            Assert::IsTrue(a.left);
            Assert::IsTrue(a.bottom);
            Assert::IsFalse(a.right);
            Assert::IsFalse(a.top);
        }
    };
}
```

Add `<ClCompile Include="DividerModel.Spec.cpp" />` to `UnitTests.vcxproj`.

- [ ] **Step 2: Run the build to verify it fails**

Run the unit-test build command from the File Structure section.
Expected: build FAILS with `Cannot open include file: 'FancyZonesLib/DividerModel.h'`.

- [ ] **Step 3: Write the header**

Create `FancyZonesLib/DividerModel.h`:

```cpp
#pragma once

#include <optional>
#include <vector>

#include <windef.h>

#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/Zone.h>

namespace FancyZonesDividers
{
    // Positions and track sizes are expressed in units of 1/10000 of the work area, like GridLayoutInfo percents.
    constexpr int TotalUnits = 10000;

    enum class Axis
    {
        Column, // vertical divider, positioned on the x axis
        Row, // horizontal divider, positioned on the y axis
    };

    // Boundary between track (index - 1) and track (index) on the given axis. Valid range: 1..trackCount-1.
    struct Divider
    {
        Axis axis;
        int index;
    };

    // Inclusive range of rows/columns covered by a set of zones.
    struct Span
    {
        int firstRow;
        int lastRow;
        int firstCol;
        int lastCol;
    };

    struct MovedEdges
    {
        bool left = false;
        bool top = false;
        bool right = false;
        bool bottom = false;

        bool any() const noexcept { return left || top || right || bottom; }

        MovedEdges& operator|=(const MovedEdges& other) noexcept
        {
            left |= other.left;
            top |= other.top;
            right |= other.right;
            bottom |= other.bottom;
            return *this;
        }
    };

    MovedEdges DetectMovedEdges(const RECT& before, const RECT& after, long tolerance) noexcept;

    class DividerModel
    {
    public:
        explicit DividerModel(const FancyZonesDataTypes::GridLayoutInfo& base);

        const FancyZonesDataTypes::GridLayoutInfo& Base() const noexcept { return m_base; }
        const FancyZonesDataTypes::GridLayoutInfo& Current() const noexcept { return m_current; }

        // True when other has the same rows, columns, percents and cellChildMap as the base grid.
        bool SameBase(const FancyZonesDataTypes::GridLayoutInfo& other) const noexcept;

        // Bounding rows/columns of the given zones; nullopt when none of them is in the grid.
        std::optional<Span> SpanOf(const ZoneIndexSet& zones) const noexcept;

        // Dividers on the inner sides of span that correspond to the moved edges. Outer (work area) sides are skipped.
        std::vector<Divider> DividersFor(const Span& span, const MovedEdges& moved) const;

        int Position(Divider divider) const noexcept;
        int BasePosition(Divider divider) const noexcept;
        bool IsModified(Divider divider) const noexcept;
        bool IsModified() const noexcept;

        // Moves the whole divider line to position (clamped so both neighbouring tracks stay >= minTrack,
        // snapped to the base position when within snapRadius). Returns true when the grid changed.
        bool Move(Divider divider, int position, int minTrack, int snapRadius);

    private:
        FancyZonesDataTypes::GridLayoutInfo m_base;
        FancyZonesDataTypes::GridLayoutInfo m_current;
    };
}
```

- [ ] **Step 4: Write the implementation**

Create `FancyZonesLib/DividerModel.cpp`:

```cpp
#include "pch.h"
#include "DividerModel.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>

using FancyZonesDataTypes::GridLayoutInfo;

namespace FancyZonesDividers
{
    namespace
    {
        const std::vector<int>& Tracks(const GridLayoutInfo& info, Axis axis) noexcept
        {
            return axis == Axis::Column ? info.columnsPercents() : info.rowsPercents();
        }

        std::vector<int>& Tracks(GridLayoutInfo& info, Axis axis) noexcept
        {
            return axis == Axis::Column ? info.columnsPercents() : info.rowsPercents();
        }

        // Sum of the first `index` tracks = position of the boundary before track `index`.
        int EdgeOf(const std::vector<int>& tracks, int index) noexcept
        {
            return std::accumulate(tracks.begin(), tracks.begin() + index, 0);
        }

        bool IsValid(const GridLayoutInfo& info, Divider divider) noexcept
        {
            const int count = static_cast<int>(Tracks(info, divider.axis).size());
            return divider.index >= 1 && divider.index < count;
        }
    }

    MovedEdges DetectMovedEdges(const RECT& before, const RECT& after, long tolerance) noexcept
    {
        MovedEdges moved;
        moved.left = std::labs(after.left - before.left) > tolerance;
        moved.top = std::labs(after.top - before.top) > tolerance;
        moved.right = std::labs(after.right - before.right) > tolerance;
        moved.bottom = std::labs(after.bottom - before.bottom) > tolerance;
        return moved;
    }

    DividerModel::DividerModel(const GridLayoutInfo& base) :
        m_base(base),
        m_current(base)
    {
    }

    bool DividerModel::SameBase(const GridLayoutInfo& other) const noexcept
    {
        return m_base.rows() == other.rows() &&
               m_base.columns() == other.columns() &&
               m_base.rowsPercents() == other.rowsPercents() &&
               m_base.columnsPercents() == other.columnsPercents() &&
               m_base.cellChildMap() == other.cellChildMap();
    }

    std::optional<Span> DividerModel::SpanOf(const ZoneIndexSet& zones) const noexcept
    {
        std::optional<Span> span;
        const auto& map = m_base.cellChildMap();
        for (int row = 0; row < m_base.rows(); ++row)
        {
            for (int col = 0; col < m_base.columns(); ++col)
            {
                const ZoneIndex id = map[row][col];
                if (std::find(zones.begin(), zones.end(), id) == zones.end())
                {
                    continue;
                }

                if (!span)
                {
                    span = Span{ row, row, col, col };
                }
                else
                {
                    span->firstRow = std::min(span->firstRow, row);
                    span->lastRow = std::max(span->lastRow, row);
                    span->firstCol = std::min(span->firstCol, col);
                    span->lastCol = std::max(span->lastCol, col);
                }
            }
        }

        return span;
    }

    std::vector<Divider> DividerModel::DividersFor(const Span& span, const MovedEdges& moved) const
    {
        std::vector<Divider> dividers;
        if (moved.left && span.firstCol > 0)
        {
            dividers.push_back({ Axis::Column, span.firstCol });
        }
        if (moved.right && span.lastCol + 1 < m_base.columns())
        {
            dividers.push_back({ Axis::Column, span.lastCol + 1 });
        }
        if (moved.top && span.firstRow > 0)
        {
            dividers.push_back({ Axis::Row, span.firstRow });
        }
        if (moved.bottom && span.lastRow + 1 < m_base.rows())
        {
            dividers.push_back({ Axis::Row, span.lastRow + 1 });
        }

        return dividers;
    }

    int DividerModel::Position(Divider divider) const noexcept
    {
        return EdgeOf(Tracks(m_current, divider.axis), divider.index);
    }

    int DividerModel::BasePosition(Divider divider) const noexcept
    {
        return EdgeOf(Tracks(m_base, divider.axis), divider.index);
    }

    bool DividerModel::IsModified(Divider divider) const noexcept
    {
        return IsValid(m_current, divider) && Position(divider) != BasePosition(divider);
    }

    bool DividerModel::IsModified() const noexcept
    {
        return m_current.rowsPercents() != m_base.rowsPercents() || m_current.columnsPercents() != m_base.columnsPercents();
    }

    bool DividerModel::Move(Divider divider, int position, int minTrack, int snapRadius)
    {
        if (!IsValid(m_current, divider))
        {
            return false;
        }

        auto& tracks = Tracks(m_current, divider.axis);
        const int previous = EdgeOf(tracks, divider.index - 1);
        const int next = EdgeOf(tracks, divider.index + 1);

        const int basePosition = BasePosition(divider);
        if (std::abs(position - basePosition) <= snapRadius)
        {
            position = basePosition;
        }

        const int low = previous + minTrack;
        const int high = next - minTrack;
        if (low > high)
        {
            return false;
        }

        position = std::clamp(position, low, high);

        const int before = position - previous;
        const int after = next - position;
        if (tracks[divider.index - 1] == before && tracks[divider.index] == after)
        {
            return false;
        }

        tracks[divider.index - 1] = before;
        tracks[divider.index] = after;
        return true;
    }
}
```

Add to `FancyZonesLib.vcxproj`: `<ClInclude Include="DividerModel.h" />` and `<ClCompile Include="DividerModel.cpp" />`.

- [ ] **Step 5: Build and run the tests**

Run the build command, then the test command with filter `FullyQualifiedName~DividerModel`.
Expected: build exit code `0`; all `DividerModelUnitTests` pass.

- [ ] **Step 6: Commit**

```powershell
git add src/modules/fancyzones/FancyZonesLib/DividerModel.h src/modules/fancyzones/FancyZonesLib/DividerModel.cpp src/modules/fancyzones/FancyZonesLib/FancyZonesLib.vcxproj src/modules/fancyzones/FancyZonesTests/UnitTests/DividerModel.Spec.cpp src/modules/fancyzones/FancyZonesTests/UnitTests/UnitTests.vcxproj
git commit -m "feat(fancyzones): add DividerModel for whole-line grid divider moves"
```

---

### Task 2: LayoutConfigurator grid helpers

Expose any grid-based layout as a `GridLayoutInfo`, and expose grid zone calculation, without changing existing zone output.

**Files:**
- Modify: `FancyZonesLib/LayoutConfigurator.h`
- Modify: `FancyZonesLib/LayoutConfigurator.cpp` (functions `Grid`, plus new helpers; lines 327-382 and 400-448 at time of writing)
- Create: `FancyZonesTests/UnitTests/LayoutConfigurator.Spec.cpp`
- Modify: `FancyZonesTests/UnitTests/UnitTests.vcxproj` (add `<ClCompile Include="LayoutConfigurator.Spec.cpp" />`)

**Interfaces:**
- Consumes: `LayoutData` (`FancyZonesLib/FancyZonesData/LayoutData.h`), `CustomLayouts::instance().GetCustomLayoutData(const GUID&)`.
- Produces:
  - `static std::optional<FancyZonesDataTypes::GridLayoutInfo> LayoutConfigurator::GridInfo(const LayoutData& data) noexcept;` returns nullopt for Blank, Focus and Canvas layouts.
  - `static ZonesMap LayoutConfigurator::GridZones(FancyZonesUtils::Rect workArea, const FancyZonesDataTypes::GridLayoutInfo& info, int spacing) noexcept;`

- [ ] **Step 1: Write the failing tests**

Create `FancyZonesTests/UnitTests/LayoutConfigurator.Spec.cpp`:

```cpp
#include "pch.h"

#include <FancyZonesLib/LayoutConfigurator.h>
#include <FancyZonesLib/FancyZonesData/LayoutData.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesDataTypes;

namespace FancyZonesUnitTests
{
    TEST_CLASS (LayoutConfiguratorGridUnitTests)
    {
        static LayoutData Data(ZoneSetLayoutType type, int zoneCount)
        {
            return LayoutData{ .type = type, .showSpacing = false, .spacing = 0, .zoneCount = zoneCount };
        }

        static int Sum(const std::vector<int>& values)
        {
            int sum = 0;
            for (int v : values)
            {
                sum += v;
            }
            return sum;
        }

    public:
        TEST_METHOD (GridInfoForGridLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, 4));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(2, info->rows());
            Assert::AreEqual(2, info->columns());
            Assert::AreEqual(10000, Sum(info->rowsPercents()));
            Assert::AreEqual(10000, Sum(info->columnsPercents()));
            Assert::AreEqual(4, info->zoneCount());
        }

        TEST_METHOD (GridInfoForColumnsLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Columns, 3));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(1, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::AreEqual(10000, Sum(info->columnsPercents()));
            Assert::AreEqual(3, info->zoneCount());
        }

        TEST_METHOD (GridInfoForRowsLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Rows, 3));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(3, info->rows());
            Assert::AreEqual(1, info->columns());
            Assert::AreEqual(10000, Sum(info->rowsPercents()));
            Assert::AreEqual(3, info->zoneCount());
        }

        TEST_METHOD (GridInfoForPriorityGridLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::PriorityGrid, 3));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(1, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::AreEqual(2500, info->columnsPercents()[0]);
            Assert::AreEqual(5000, info->columnsPercents()[1]);
            Assert::AreEqual(2500, info->columnsPercents()[2]);
        }

        TEST_METHOD (GridInfoUnsupportedTypes)
        {
            Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Focus, 3)).has_value());
            Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Blank, 0)).has_value());
        }

        TEST_METHOD (GridZonesMatchesGridLayout)
        {
            const FancyZonesUtils::Rect workArea{ RECT{ 0, 0, 1920, 1080 } };
            const auto data = Data(ZoneSetLayoutType::Grid, 4);
            const auto expected = LayoutConfigurator::Grid(workArea, 4, 0);
            const auto actual = LayoutConfigurator::GridZones(workArea, LayoutConfigurator::GridInfo(data).value(), 0);

            Assert::AreEqual(expected.size(), actual.size());
            for (const auto& [id, zone] : expected)
            {
                const RECT e = zone.GetZoneRect();
                const RECT a = actual.at(id).GetZoneRect();
                Assert::AreEqual(e.left, a.left);
                Assert::AreEqual(e.top, a.top);
                Assert::AreEqual(e.right, a.right);
                Assert::AreEqual(e.bottom, a.bottom);
            }
        }

        TEST_METHOD (GridZonesMatchesColumnsLayoutWithinOnePixel)
        {
            const FancyZonesUtils::Rect workArea{ RECT{ 0, 0, 1920, 1080 } };
            const auto expected = LayoutConfigurator::Columns(workArea, 3, 0);
            const auto actual = LayoutConfigurator::GridZones(workArea, LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Columns, 3)).value(), 0);

            Assert::AreEqual(expected.size(), actual.size());
            for (const auto& [id, zone] : expected)
            {
                const RECT e = zone.GetZoneRect();
                const RECT a = actual.at(id).GetZoneRect();
                Assert::IsTrue(std::abs(e.left - a.left) <= 1);
                Assert::IsTrue(std::abs(e.right - a.right) <= 1);
                Assert::AreEqual(e.top, a.top);
                Assert::AreEqual(e.bottom, a.bottom);
            }
        }
    };
}
```

- [ ] **Step 2: Run the build to verify it fails**

Run the unit-test build command.
Expected: FAIL, `GridInfo` is not a member of `LayoutConfigurator`.

- [ ] **Step 3: Update the header**

In `FancyZonesLib/LayoutConfigurator.h` replace the forward declaration block and class with:

```cpp
#pragma once

#include <optional>

#include <FancyZonesLib/FancyZonesData/LayoutData.h>
#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/Zone.h>
#include <FancyZonesLib/util.h>

// Mapping zone id to zone
using ZonesMap = std::map<ZoneIndex, Zone>;

class LayoutConfigurator
{
public:
    static ZonesMap Focus(FancyZonesUtils::Rect workArea, int zoneCount) noexcept;
    static ZonesMap Rows(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap Columns(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap Grid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap PriorityGrid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap Custom(FancyZonesUtils::Rect workArea, HMONITOR monitor, const FancyZonesDataTypes::CustomLayoutData& data, int spacing) noexcept;

    // Zones of an arbitrary grid description (percentages + cell map).
    static ZonesMap GridZones(FancyZonesUtils::Rect workArea, const FancyZonesDataTypes::GridLayoutInfo& info, int spacing) noexcept;

    // Grid description of a grid-based layout (Columns, Rows, Grid, PriorityGrid, custom Grid).
    // nullopt for Blank, Focus and Canvas layouts.
    static std::optional<FancyZonesDataTypes::GridLayoutInfo> GridInfo(const LayoutData& data) noexcept;
};
```

- [ ] **Step 4: Refactor `Grid` and add the new functions**

In `FancyZonesLib/LayoutConfigurator.cpp`:

1. Add includes after the existing ones:

```cpp
#include <FancyZonesLib/FancyZonesData/CustomLayouts.h>
```

2. Inside the existing anonymous `namespace { ... }` at the top of the file (after `predefinedPriorityGridLayouts`, before its closing brace) add:

```cpp
    FancyZonesDataTypes::GridLayoutInfo ColumnsInfo(int zoneCount)
    {
        FancyZonesDataTypes::GridLayoutInfo info(FancyZonesDataTypes::GridLayoutInfo::Minimal{ .rows = 1, .columns = zoneCount });
        info.rowsPercents()[0] = C_MULTIPLIER;
        for (int col = 0; col < zoneCount; col++)
        {
            info.columnsPercents()[col] = C_MULTIPLIER * (col + 1) / zoneCount - C_MULTIPLIER * col / zoneCount;
            info.cellChildMap()[0][col] = col;
        }
        return info;
    }

    FancyZonesDataTypes::GridLayoutInfo RowsInfo(int zoneCount)
    {
        FancyZonesDataTypes::GridLayoutInfo info(FancyZonesDataTypes::GridLayoutInfo::Minimal{ .rows = zoneCount, .columns = 1 });
        info.columnsPercents()[0] = C_MULTIPLIER;
        for (int row = 0; row < zoneCount; row++)
        {
            info.rowsPercents()[row] = C_MULTIPLIER * (row + 1) / zoneCount - C_MULTIPLIER * row / zoneCount;
            info.cellChildMap()[row][0] = row;
        }
        return info;
    }

    FancyZonesDataTypes::GridLayoutInfo GridInfoForZoneCount(int zoneCount)
    {
        int rows = 1, columns = 1;
        while (zoneCount / rows >= rows)
        {
            rows++;
        }
        rows--;
        columns = zoneCount / rows;
        if (zoneCount % rows != 0)
        {
            columns++;
        }

        FancyZonesDataTypes::GridLayoutInfo gridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Minimal{ .rows = rows, .columns = columns });

        // Note: The expressions below are NOT equal to C_MULTIPLIER / {rows|columns} and are done
        // like this to make the sum of all percents exactly C_MULTIPLIER
        for (int row = 0; row < rows; row++)
        {
            gridLayoutInfo.rowsPercents()[row] = C_MULTIPLIER * (row + 1) / rows - C_MULTIPLIER * row / rows;
        }
        for (int col = 0; col < columns; col++)
        {
            gridLayoutInfo.columnsPercents()[col] = C_MULTIPLIER * (col + 1) / columns - C_MULTIPLIER * col / columns;
        }

        int index = 0;
        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < columns; col++)
            {
                gridLayoutInfo.cellChildMap()[row][col] = index++;
                if (index == zoneCount)
                {
                    index--;
                }
            }
        }

        return gridLayoutInfo;
    }
```

3. Replace the whole body of `LayoutConfigurator::Grid` (currently lines 327-382) with:

```cpp
ZonesMap LayoutConfigurator::Grid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept
{
    if (zoneCount == 0)
    {
        return {};
    }

    return CalculateGridZones(workArea, GridInfoForZoneCount(zoneCount), spacing);
}
```

(The old `cellChildMap()[i] = std::vector<int>(columns)` reset loop is dropped because the `Minimal` constructor already sizes every row; the index assignment loop overwrites every cell.)

4. Append at the end of the file:

```cpp
ZonesMap LayoutConfigurator::GridZones(FancyZonesUtils::Rect workArea, const FancyZonesDataTypes::GridLayoutInfo& info, int spacing) noexcept
{
    return CalculateGridZones(workArea, info, spacing);
}

std::optional<FancyZonesDataTypes::GridLayoutInfo> LayoutConfigurator::GridInfo(const LayoutData& data) noexcept
{
    switch (data.type)
    {
    case FancyZonesDataTypes::ZoneSetLayoutType::Columns:
        return data.zoneCount > 0 ? std::optional(ColumnsInfo(data.zoneCount)) : std::nullopt;
    case FancyZonesDataTypes::ZoneSetLayoutType::Rows:
        return data.zoneCount > 0 ? std::optional(RowsInfo(data.zoneCount)) : std::nullopt;
    case FancyZonesDataTypes::ZoneSetLayoutType::Grid:
        return data.zoneCount > 0 ? std::optional(GridInfoForZoneCount(data.zoneCount)) : std::nullopt;
    case FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid:
    {
        if (data.zoneCount <= 0)
        {
            return std::nullopt;
        }

        // Same rule as LayoutConfigurator::PriorityGrid
        constexpr int predefinedLayoutsCount = sizeof(predefinedPriorityGridLayouts) / sizeof(FancyZonesDataTypes::GridLayoutInfo);
        if (data.zoneCount < predefinedLayoutsCount)
        {
            return predefinedPriorityGridLayouts[data.zoneCount - 1];
        }

        return GridInfoForZoneCount(data.zoneCount);
    }
    case FancyZonesDataTypes::ZoneSetLayoutType::Custom:
    {
        const auto custom = CustomLayouts::instance().GetCustomLayoutData(data.uuid);
        if (custom.has_value() &&
            custom->type == FancyZonesDataTypes::CustomLayoutType::Grid &&
            std::holds_alternative<FancyZonesDataTypes::GridLayoutInfo>(custom->info))
        {
            return std::get<FancyZonesDataTypes::GridLayoutInfo>(custom->info);
        }

        return std::nullopt;
    }
    default:
        return std::nullopt;
    }
}
```

`CalculateGridZones` is defined above these functions in the same file, so no extra declaration is needed.

- [ ] **Step 5: Build and run the tests**

Run the build, then tests with filter `FullyQualifiedName~LayoutConfiguratorGrid`, then the whole suite (the `Grid()` refactor must not break existing `Layout`/`WorkArea` tests).
Expected: build `0`; new tests pass; full suite shows no new failures versus the Task 0 baseline.

- [ ] **Step 6: Commit**

```powershell
git add src/modules/fancyzones/FancyZonesLib/LayoutConfigurator.h src/modules/fancyzones/FancyZonesLib/LayoutConfigurator.cpp src/modules/fancyzones/FancyZonesTests/UnitTests/LayoutConfigurator.Spec.cpp src/modules/fancyzones/FancyZonesTests/UnitTests/UnitTests.vcxproj
git commit -m "feat(fancyzones): expose grid info and grid zone calculation"
```

---

### Task 3: Layout::InitFromGrid and Spacing

**Files:**
- Modify: `FancyZonesLib/Layout.h`
- Modify: `FancyZonesLib/Layout.cpp` (add after `Layout::Init`, which ends at line 169)
- Modify: `FancyZonesTests/UnitTests/Layout.Spec.cpp`

**Interfaces:**
- Consumes: `LayoutConfigurator::GridZones` (Task 2).
- Produces:
  - `bool Layout::InitFromGrid(const FancyZonesUtils::Rect& workArea, const FancyZonesDataTypes::GridLayoutInfo& grid) noexcept;` rebuilds `m_zones` from `grid` using this layout's spacing; returns true when the resulting zone count equals the layout's zone count.
  - `int Layout::Spacing() const noexcept;` spacing in pixels actually applied (0 when `showSpacing` is false).

- [ ] **Step 1: Write the failing test**

In `FancyZonesTests/UnitTests/Layout.Spec.cpp` add `#include <FancyZonesLib/LayoutConfigurator.h>` to the includes, and inside `TEST_CLASS (LayoutUnitTests)` after `TestCreateLayout` add (the fixture `m_data` is Grid, 4 zones, spacing 17, showSpacing true):

```cpp
        TEST_METHOD (InitFromGridUsesGivenPercentsAndSpacing)
        {
            auto grid = LayoutConfigurator::GridInfo(m_data).value();
            grid.columnsPercents()[0] = 6000;
            grid.columnsPercents()[1] = 4000;

            Assert::IsTrue(m_layout->InitFromGrid(FancyZonesUtils::Rect(RECT{ 0, 0, 1920, 1080 }), grid));
            Assert::AreEqual(17, m_layout->Spacing());
            Assert::AreEqual(size_t{ 4 }, m_layout->Zones().size());

            // column boundary at 6000 * 1920 / 10000 = 1152, spacing/2 = 8 on each side
            Assert::AreEqual(1144L, m_layout->Zones().at(0).GetZoneRect().right);
            Assert::AreEqual(1160L, m_layout->Zones().at(1).GetZoneRect().left);
            // whole line: the bottom row shares the same boundary
            Assert::AreEqual(1144L, m_layout->Zones().at(2).GetZoneRect().right);
            Assert::AreEqual(1160L, m_layout->Zones().at(3).GetZoneRect().left);
            // rows untouched: 5000 * 1080 / 10000 = 540, minus 8
            Assert::AreEqual(532L, m_layout->Zones().at(0).GetZoneRect().bottom);
        }

        TEST_METHOD (InitFromGridRejectsEmptyWorkArea)
        {
            const auto grid = LayoutConfigurator::GridInfo(m_data).value();
            Assert::IsFalse(m_layout->InitFromGrid(FancyZonesUtils::Rect(RECT{ 0, 0, 0, 0 }), grid));
        }
```

- [ ] **Step 2: Run the build to verify it fails**

Expected: FAIL, `InitFromGrid` / `Spacing` are not members of `Layout`.

- [ ] **Step 3: Declare the methods**

In `FancyZonesLib/Layout.h` add after the `Init` declaration:

```cpp
    // Rebuilds the zones from an explicit grid description (used for working layouts), keeping this layout's spacing.
    bool InitFromGrid(const FancyZonesUtils::Rect& workAreaRect, const FancyZonesDataTypes::GridLayoutInfo& grid) noexcept;
    int Spacing() const noexcept;
```

- [ ] **Step 4: Implement**

In `FancyZonesLib/Layout.cpp` after `Layout::Init`:

```cpp
bool Layout::InitFromGrid(const FancyZonesUtils::Rect& workArea, const FancyZonesDataTypes::GridLayoutInfo& grid) noexcept
{
    if (workArea.width() == 0 || workArea.height() == 0)
    {
        Logger::error(L"Layout grid initialization: invalid work area");
        return false;
    }

    auto zones = LayoutConfigurator::GridZones(workArea, grid, Spacing());
    if (zones.size() != static_cast<size_t>(m_data.zoneCount))
    {
        return false;
    }

    m_zones = std::move(zones);
    return true;
}

int Layout::Spacing() const noexcept
{
    return m_data.showSpacing ? m_data.spacing : 0;
}
```

- [ ] **Step 5: Build and run the tests**

Run build, then tests with filter `FullyQualifiedName~LayoutUnitTests`.
Expected: `0`; all `LayoutUnitTests` pass (new and existing).

- [ ] **Step 6: Commit**

```powershell
git add src/modules/fancyzones/FancyZonesLib/Layout.h src/modules/fancyzones/FancyZonesLib/Layout.cpp src/modules/fancyzones/FancyZonesTests/UnitTests/Layout.Spec.cpp
git commit -m "feat(fancyzones): build layout zones from an explicit grid"
```

---

### Task 4: WorkingLayouts store

**Files:**
- Create: `FancyZonesLib/WorkingLayouts.h`
- Create: `FancyZonesLib/WorkingLayouts.cpp`
- Modify: `FancyZonesLib/FancyZonesLib.vcxproj` (add `<ClInclude Include="WorkingLayouts.h" />` and `<ClCompile Include="WorkingLayouts.cpp" />`)
- Create: `FancyZonesTests/UnitTests/WorkingLayouts.Spec.cpp`
- Modify: `FancyZonesTests/UnitTests/UnitTests.vcxproj` (add `<ClCompile Include="WorkingLayouts.Spec.cpp" />`)

**Interfaces:**
- Consumes: `FancyZonesDividers::DividerModel` (Task 1), `FancyZonesDataTypes::WorkAreaId::toString()`.
- Produces:
  - `static WorkingLayouts& WorkingLayouts::instance();`
  - `FancyZonesDividers::DividerModel* Find(const WorkAreaId& id, const GUID& layoutId, const GridLayoutInfo& base);` returns the entry only if the layout id and base grid still match; a mismatching entry is erased and nullptr is returned.
  - `FancyZonesDividers::DividerModel& GetOrCreate(const WorkAreaId& id, const GUID& layoutId, const GridLayoutInfo& base);` returns the matching entry or replaces a stale one with a fresh model.
  - `void Remove(const WorkAreaId& id);`
  - `void Clear();`
- The key is `WorkAreaId::toString()` (device id, instance id, number, serial number, virtual desktop id), not the `HMONITOR`, so entries survive work-area recreation after hibernate or display changes.

- [ ] **Step 1: Write the failing tests**

Create `FancyZonesTests/UnitTests/WorkingLayouts.Spec.cpp`:

```cpp
#include "pch.h"

#include <FancyZonesLib/WorkingLayouts.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesDividers;
using FancyZonesDataTypes::GridLayoutInfo;
using FancyZonesDataTypes::WorkAreaId;

namespace FancyZonesUnitTests
{
    TEST_CLASS (WorkingLayoutsUnitTests)
    {
        static GridLayoutInfo MakeGrid(const std::vector<int>& cols)
        {
            const std::vector<int> rows{ 10000 };
            std::vector<std::vector<int>> map(1);
            for (size_t c = 0; c < cols.size(); ++c)
            {
                map[0].push_back(static_cast<int>(c));
            }

            return GridLayoutInfo(GridLayoutInfo::Full{
                .rows = 1,
                .columns = static_cast<int>(cols.size()),
                .rowsPercents = rows,
                .columnsPercents = cols,
                .cellChildMap = map,
                .showSpacing = false,
                .spacing = 0,
                .sensitivityRadius = 0 });
        }

        static WorkAreaId Id(const wchar_t* monitor, const wchar_t* desktop)
        {
            WorkAreaId id;
            id.monitorId.deviceId.id = monitor;
            id.monitorId.deviceId.instanceId = L"instance";
            id.monitorId.serialNumber = L"serial";
            id.virtualDesktopId = FancyZonesUtils::GuidFromString(desktop).value();
            return id;
        }

        const GUID m_layout = FancyZonesUtils::GuidFromString(L"{F762BAD6-DAA1-4997-9497-E11DFEB72F21}").value();
        const GUID m_otherLayout = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value();
        const wchar_t* m_desktop = L"{39B25DD2-130D-4B5D-8851-4791D66B1539}";

        TEST_METHOD_INITIALIZE(Init)
        {
            WorkingLayouts::instance().Clear();
        }

        TEST_METHOD_CLEANUP(CleanUp)
        {
            WorkingLayouts::instance().Clear();
        }

    public:
        TEST_METHOD (FindOnEmptyStoreReturnsNull)
        {
            Assert::IsNull(WorkingLayouts::instance().Find(Id(L"A", m_desktop), m_layout, MakeGrid({ 5000, 5000 })));
        }

        TEST_METHOD (GetOrCreateThenFind)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });

            auto& model = WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 6000, 500, 0));

            auto* found = WorkingLayouts::instance().Find(id, m_layout, base);
            Assert::IsNotNull(found);
            Assert::AreEqual(6000, found->Position({ Axis::Column, 1 }));
        }

        TEST_METHOD (GetOrCreateReturnsSameEntry)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });

            WorkingLayouts::instance().GetOrCreate(id, m_layout, base).Move({ Axis::Column, 1 }, 6000, 500, 0);
            auto& again = WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            Assert::AreEqual(6000, again.Position({ Axis::Column, 1 }));
        }

        TEST_METHOD (EntriesAreKeyedByWorkArea)
        {
            const auto base = MakeGrid({ 5000, 5000 });
            WorkingLayouts::instance().GetOrCreate(Id(L"A", m_desktop), m_layout, base).Move({ Axis::Column, 1 }, 6000, 500, 0);

            Assert::IsNull(WorkingLayouts::instance().Find(Id(L"B", m_desktop), m_layout, base));
            Assert::IsNull(WorkingLayouts::instance().Find(Id(L"A", L"{61FA9FC0-26A6-4B37-A834-491C148DFC57}"), m_layout, base));
            Assert::IsNotNull(WorkingLayouts::instance().Find(Id(L"A", m_desktop), m_layout, base));
        }

        TEST_METHOD (EntryIgnoresHMonitor)
        {
            const auto base = MakeGrid({ 5000, 5000 });
            auto before = Id(L"A", m_desktop);
            before.monitorId.monitor = reinterpret_cast<HMONITOR>(0x1111);
            WorkingLayouts::instance().GetOrCreate(before, m_layout, base).Move({ Axis::Column, 1 }, 6000, 500, 0);

            auto after = Id(L"A", m_desktop);
            after.monitorId.monitor = reinterpret_cast<HMONITOR>(0x2222); // handle changed after resume
            Assert::IsNotNull(WorkingLayouts::instance().Find(after, m_layout, base));
        }

        TEST_METHOD (DifferentLayoutIdDiscardsEntry)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });
            WorkingLayouts::instance().GetOrCreate(id, m_layout, base).Move({ Axis::Column, 1 }, 6000, 500, 0);

            Assert::IsNull(WorkingLayouts::instance().Find(id, m_otherLayout, base));
            // the stale entry is gone, even for the original layout id
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, base));
        }

        TEST_METHOD (DifferentBaseDiscardsEntry)
        {
            const auto id = Id(L"A", m_desktop);
            WorkingLayouts::instance().GetOrCreate(id, m_layout, MakeGrid({ 5000, 5000 })).Move({ Axis::Column, 1 }, 6000, 500, 0);

            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, MakeGrid({ 4000, 6000 })));
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, MakeGrid({ 5000, 5000 })));
        }

        TEST_METHOD (GetOrCreateReplacesStaleEntry)
        {
            const auto id = Id(L"A", m_desktop);
            WorkingLayouts::instance().GetOrCreate(id, m_layout, MakeGrid({ 5000, 5000 })).Move({ Axis::Column, 1 }, 6000, 500, 0);

            auto& fresh = WorkingLayouts::instance().GetOrCreate(id, m_otherLayout, MakeGrid({ 3000, 3000, 4000 }));
            Assert::IsFalse(fresh.IsModified());
            Assert::AreEqual(3, fresh.Current().columns());
        }

        TEST_METHOD (RemoveAndClear)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });
            WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            WorkingLayouts::instance().Remove(id);
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, base));

            WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            WorkingLayouts::instance().Clear();
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, base));
        }
    };
}
```

- [ ] **Step 2: Run the build to verify it fails**

Expected: FAIL, cannot open `FancyZonesLib/WorkingLayouts.h`.

- [ ] **Step 3: Write the header**

Create `FancyZonesLib/WorkingLayouts.h`:

```cpp
#pragma once

#include <map>
#include <string>

#include <guiddef.h>

#include <FancyZonesLib/DividerModel.h>
#include <FancyZonesLib/FancyZonesDataTypes.h>

// In-memory, per work area (monitor + virtual desktop) store of modified grid layouts.
// Entries are never written to disk. They outlive WorkArea objects, which are recreated on display changes.
class WorkingLayouts
{
public:
    static WorkingLayouts& instance();

    // Returns the model for the work area if it exists and still belongs to this layout and base grid.
    // A stale entry (different layout id or base grid) is erased and nullptr is returned.
    FancyZonesDividers::DividerModel* Find(const FancyZonesDataTypes::WorkAreaId& id, const GUID& layoutId, const FancyZonesDataTypes::GridLayoutInfo& base);

    // Returns the matching model, or replaces a stale/missing entry with a fresh unmodified one.
    FancyZonesDividers::DividerModel& GetOrCreate(const FancyZonesDataTypes::WorkAreaId& id, const GUID& layoutId, const FancyZonesDataTypes::GridLayoutInfo& base);

    void Remove(const FancyZonesDataTypes::WorkAreaId& id);
    void Clear();

private:
    struct Entry
    {
        GUID layoutId;
        FancyZonesDividers::DividerModel model;
    };

    WorkingLayouts() = default;

    std::map<std::wstring, Entry> m_entries;
};
```

- [ ] **Step 4: Implement**

Create `FancyZonesLib/WorkingLayouts.cpp`:

```cpp
#include "pch.h"
#include "WorkingLayouts.h"

using FancyZonesDataTypes::GridLayoutInfo;
using FancyZonesDataTypes::WorkAreaId;

WorkingLayouts& WorkingLayouts::instance()
{
    static WorkingLayouts self;
    return self;
}

FancyZonesDividers::DividerModel* WorkingLayouts::Find(const WorkAreaId& id, const GUID& layoutId, const GridLayoutInfo& base)
{
    const auto key = id.toString();
    auto it = m_entries.find(key);
    if (it == m_entries.end())
    {
        return nullptr;
    }

    // Template layouts share GUID_NULL, so the base grid comparison is what tells them apart.
    if (!(it->second.layoutId == layoutId) || !it->second.model.SameBase(base))
    {
        m_entries.erase(it);
        return nullptr;
    }

    return &it->second.model;
}

FancyZonesDividers::DividerModel& WorkingLayouts::GetOrCreate(const WorkAreaId& id, const GUID& layoutId, const GridLayoutInfo& base)
{
    if (auto* existing = Find(id, layoutId, base))
    {
        return *existing;
    }

    auto [it, inserted] = m_entries.insert_or_assign(id.toString(), Entry{ layoutId, FancyZonesDividers::DividerModel(base) });
    return it->second.model;
}

void WorkingLayouts::Remove(const WorkAreaId& id)
{
    m_entries.erase(id.toString());
}

void WorkingLayouts::Clear()
{
    m_entries.clear();
}
```

- [ ] **Step 5: Build and run the tests**

Run build, then tests with filter `FullyQualifiedName~WorkingLayouts`.
Expected: `0`; all `WorkingLayoutsUnitTests` pass.

- [ ] **Step 6: Commit**

```powershell
git add src/modules/fancyzones/FancyZonesLib/WorkingLayouts.h src/modules/fancyzones/FancyZonesLib/WorkingLayouts.cpp src/modules/fancyzones/FancyZonesLib/FancyZonesLib.vcxproj src/modules/fancyzones/FancyZonesTests/UnitTests/WorkingLayouts.Spec.cpp src/modules/fancyzones/FancyZonesTests/UnitTests/UnitTests.vcxproj
git commit -m "feat(fancyzones): add in-memory working layout store"
```

---

### Task 5: WorkArea integration

`WorkArea` builds zones from the working layout, applies edge resizes, and shows the cue.

**Files:**
- Modify: `FancyZonesLib/WorkArea.h`
- Modify: `FancyZonesLib/WorkArea.cpp` (`UpdateWindowPositions` at line 259, `CalculateZoneSet` at line 346, new methods)
- Modify: `FancyZonesTests/UnitTests/WorkArea.Spec.cpp`

**Interfaces:**
- Consumes: `DividerModel`, `MovedEdges`, `Divider`, `TotalUnits` (Task 1), `LayoutConfigurator::GridInfo` / `GridZones` (Task 2), `Layout::InitFromGrid` / `Spacing` (Task 3), `WorkingLayouts` (Task 4).
- Produces (used by Task 6):
  - `struct EdgeResizeResult { bool changed = false; bool atBase = true; };` (in `WorkArea.h`)
  - `bool WorkArea::SupportsEdgeResize() const noexcept;` true when the applied layout is grid-based.
  - `EdgeResizeResult WorkArea::ApplyEdgeResize(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesDividers::MovedEdges& moved, HWND skipWindow);` where `currentFrame` is the window's visible frame (DWM extended frame bounds) in screen coordinates; moves the dividers of the moved edges, rebuilds the layout and repositions all snapped windows except `skipWindow`.
  - `void WorkArea::FinishEdgeResize(const ZoneIndexSet& zones, const RECT& finalFrame, const FancyZonesDividers::MovedEdges& moved);` final pass: applies the final frame, repositions every snapped window including the dragged one, and removes the working entry when no divider differs from the base.
  - `void WorkArea::ShowResizeCue(HWND draggedWindow, bool atBase);` draws the base gridlines.
  - `void WorkArea::UpdateWindowPositions(HWND skipWindow = nullptr);`

- [ ] **Step 1: Write the failing tests**

In `FancyZonesTests/UnitTests/WorkArea.Spec.cpp` add these includes:

```cpp
#include <FancyZonesLib/DividerModel.h>
#include <FancyZonesLib/WorkingLayouts.h>
```

and append a new test class inside `namespace FancyZonesUnitTests` (before its closing brace). It applies a Grid layout with 4 zones (2x2) and no spacing on a 1920x1080 work area:

```cpp
    TEST_CLASS (WorkAreaEdgeResizeUnitTests)
    {
        FancyZonesDataTypes::WorkAreaId m_workAreaId;
        FancyZonesUtils::Rect m_workAreaRect{ RECT(0, 0, 1920, 1080) };
        std::unique_ptr<WorkArea> m_workArea;

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            m_workAreaId.monitorId.deviceId.id = L"DELA026";
            m_workAreaId.monitorId.deviceId.instanceId = L"5&10a58c63&0&UID16777488";
            m_workAreaId.monitorId.serialNumber = L"serial-number";
            m_workAreaId.virtualDesktopId = FancyZonesUtils::GuidFromString(L"{39B25DD2-130D-4B5D-8851-4791D66B1539}").value();

            AppZoneHistory::instance().LoadData();
            AppliedLayouts::instance().LoadData();
            CustomLayouts::instance().LoadData();
            DefaultLayouts::instance().LoadData();
            WorkingLayouts::instance().Clear();

            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 4,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);

            m_workArea = WorkArea::Create({}, m_workAreaId, {}, m_workAreaRect);
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            m_workArea.reset();
            WorkingLayouts::instance().Clear();
            std::filesystem::remove(AppliedLayouts::AppliedLayoutsFileName());
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            std::filesystem::remove(CustomLayouts::CustomLayoutsFileName());
            std::filesystem::remove(DefaultLayouts::DefaultLayoutsFileName());
        }

        static FancyZonesDividers::MovedEdges Right()
        {
            FancyZonesDividers::MovedEdges moved;
            moved.right = true;
            return moved;
        }

        static FancyZonesDividers::MovedEdges Left()
        {
            FancyZonesDividers::MovedEdges moved;
            moved.left = true;
            return moved;
        }

    public:
        TEST_METHOD (GridLayoutSupportsEdgeResize)
        {
            Assert::IsTrue(m_workArea->SupportsEdgeResize());
        }

        TEST_METHOD (MovingRightEdgeMovesWholeDivider)
        {
            // zone 0 is the top-left zone, its visible frame is 0,0,960,540; drag the right edge to x = 1160
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsFalse(result.atBase);

            const auto& zones = m_workArea->GetLayout()->Zones();
            Assert::IsTrue(std::abs(zones.at(0).GetZoneRect().right - 1160L) <= 1);
            Assert::IsTrue(std::abs(zones.at(1).GetZoneRect().left - 1160L) <= 1);
            // whole line: the bottom row follows
            Assert::IsTrue(std::abs(zones.at(2).GetZoneRect().right - 1160L) <= 1);
            Assert::IsTrue(std::abs(zones.at(3).GetZoneRect().left - 1160L) <= 1);
            // zones still tile the work area
            Assert::AreEqual(1920L, zones.at(1).GetZoneRect().right);
            Assert::AreEqual(zones.at(0).GetZoneRect().right, zones.at(1).GetZoneRect().left);
        }

        TEST_METHOD (SnapsBackToBaseNearOriginalLine)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);

            // 960 + 5 px is within the snap radius, so the divider returns exactly to the base line
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 965, 540 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsTrue(result.atBase);
            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (OuterEdgeDoesNothing)
        {
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ -100, 0, 960, 540 }, Left(), nullptr);
            Assert::IsFalse(result.changed);
            Assert::AreEqual(0L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().left);
        }

        TEST_METHOD (DividerIsClampedToMinimumZoneSize)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1900, 540 }, Right(), nullptr);
            const auto& zones = m_workArea->GetLayout()->Zones();
            // the right zone keeps at least the minimum track size (90 px)
            Assert::IsTrue(zones.at(1).GetZoneRect().right - zones.at(1).GetZoneRect().left >= 89);
        }

        TEST_METHOD (FinishRemovesEntryWhenBackAtBase)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 962, 540 }, Right());

            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
            const auto base = LayoutConfigurator::GridInfo(AppliedLayouts::instance().GetDeviceLayout(m_workAreaId).value()).value();
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
        }

        TEST_METHOD (FinishKeepsModifiedLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 1160L) <= 1);
        }

        TEST_METHOD (RecreatedWorkAreaKeepsWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            // simulates the work area being rebuilt after display changes / resume from hibernate
            m_workArea.reset();
            m_workArea = WorkArea::Create({}, m_workAreaId, {}, m_workAreaRect);

            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 1160L) <= 1);
        }

        TEST_METHOD (ChangingLayoutDiscardsWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            LayoutData other{
                .uuid = GUID_NULL,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Columns,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 3,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, other);
            m_workArea->InitLayout();

            Assert::AreEqual(size_t{ 3 }, m_workArea->GetLayout()->Zones().size());
            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 640L) <= 1);
        }
    };
```

- [ ] **Step 2: Run the build to verify it fails**

Expected: FAIL, `SupportsEdgeResize`, `ApplyEdgeResize`, `FinishEdgeResize` are not members of `WorkArea`.

- [ ] **Step 3: Update `WorkArea.h`**

Add includes:

```cpp
#include <FancyZonesLib/DividerModel.h>
```

Add before `class WorkArea`:

```cpp
struct EdgeResizeResult
{
    bool changed = false; // a divider moved and the layout was rebuilt
    bool atBase = true; // every divider touched by this resize is at its base position
};
```

In the public section replace `void UpdateWindowPositions();` with:

```cpp
    void UpdateWindowPositions(HWND skipWindow = nullptr);

    // Dynamic grid resize. Frames are visible window frames in screen coordinates.
    bool SupportsEdgeResize() const noexcept { return m_baseGrid.has_value(); }
    EdgeResizeResult ApplyEdgeResize(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesDividers::MovedEdges& moved, HWND skipWindow);
    void FinishEdgeResize(const ZoneIndexSet& zones, const RECT& finalFrame, const FancyZonesDividers::MovedEdges& moved);
    void ShowResizeCue(HWND draggedWindow, bool atBase);
```

In the private section add a member after `m_layout`:

```cpp
    std::optional<FancyZonesDataTypes::GridLayoutInfo> m_baseGrid; // set when the applied layout is grid-based
```

- [ ] **Step 4: Update `WorkArea.cpp`**

1. Add includes after the existing `#include "Settings.h"`:

```cpp
#include "WorkingLayouts.h"
```

2. After the existing anonymous namespace that ends with `WindowPool windowPool;` + `}` (line ~102) add:

```cpp
namespace
{
    constexpr int MinTrackPx = 90; // no zone may become narrower/shorter than this while resizing
    constexpr int SnapRadiusPx = 8; // distance from a base gridline at which a divider snaps back to it
}
```

3. Replace `UpdateWindowPositions`:

```cpp
void WorkArea::UpdateWindowPositions(HWND skipWindow)
{
    const auto& snappedWindows = m_layoutWindows.SnappedWindows();
    for (const auto& [window, zones] : snappedWindows)
    {
        if (window == skipWindow)
        {
            continue;
        }

        Snap(window, zones, true);
    }
}
```

4. In `CalculateZoneSet` replace the last two lines

```cpp
    m_layout = std::make_unique<Layout>(appliedLayout.value());
    m_layout->Init(m_workAreaRect, m_uniqueId.monitorId.monitor);
```

with:

```cpp
    m_layout = std::make_unique<Layout>(appliedLayout.value());
    m_baseGrid = LayoutConfigurator::GridInfo(appliedLayout.value());

    if (m_baseGrid)
    {
        const auto* working = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseGrid);
        if (working && m_layout->InitFromGrid(m_workAreaRect, working->Current()))
        {
            return;
        }
    }

    m_layout->Init(m_workAreaRect, m_uniqueId.monitorId.monitor);
```

5. Add the new methods before `#pragma region private`:

```cpp
EdgeResizeResult WorkArea::ApplyEdgeResize(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesDividers::MovedEdges& moved, HWND skipWindow)
{
    using namespace FancyZonesDividers;

    EdgeResizeResult result;
    if (!m_layout || !m_baseGrid)
    {
        return result;
    }

    auto& model = WorkingLayouts::instance().GetOrCreate(m_uniqueId, m_layout->Id(), *m_baseGrid);
    const auto span = model.SpanOf(zones);
    if (!span)
    {
        return result;
    }

    const auto dividers = model.DividersFor(*span, moved);
    if (dividers.empty())
    {
        return result;
    }

    const int halfSpacing = m_layout->Spacing() / 2;
    for (const auto& divider : dividers)
    {
        const bool column = divider.axis == Axis::Column;
        const long total = column ? m_workAreaRect.width() : m_workAreaRect.height();
        if (total <= 0)
        {
            continue;
        }

        // Boundary between two zones = window edge +/- half the spacing between zones.
        long edge;
        if (column)
        {
            edge = divider.index == span->firstCol ? currentFrame.left - halfSpacing : currentFrame.right + halfSpacing;
            edge -= m_workAreaRect.left();
        }
        else
        {
            edge = divider.index == span->firstRow ? currentFrame.top - halfSpacing : currentFrame.bottom + halfSpacing;
            edge -= m_workAreaRect.top();
        }

        const int position = static_cast<int>(static_cast<long long>(edge) * TotalUnits / total);
        const int minTrack = static_cast<int>(static_cast<long long>(MinTrackPx) * TotalUnits / total);
        const int snapRadius = static_cast<int>(static_cast<long long>(SnapRadiusPx) * TotalUnits / total);

        result.changed |= model.Move(divider, position, minTrack, snapRadius);
        result.atBase &= !model.IsModified(divider);
    }

    if (result.changed)
    {
        if (m_layout->InitFromGrid(m_workAreaRect, model.Current()))
        {
            UpdateWindowPositions(skipWindow);
        }
        else
        {
            // Should not happen (percents always sum to the total); fall back to the base layout.
            Logger::error(L"Working layout produced invalid zones, resetting to the base layout");
            WorkingLayouts::instance().Remove(m_uniqueId);
            CalculateZoneSet();
            UpdateWindowPositions(skipWindow);
            result.atBase = true;
        }
    }

    return result;
}

void WorkArea::FinishEdgeResize(const ZoneIndexSet& zones, const RECT& finalFrame, const FancyZonesDividers::MovedEdges& moved)
{
    ApplyEdgeResize(zones, finalFrame, moved, nullptr);

    if (m_layout && m_baseGrid)
    {
        const auto* model = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseGrid);
        if (model && !model->IsModified())
        {
            WorkingLayouts::instance().Remove(m_uniqueId);
            CalculateZoneSet();
        }
    }

    // Final pass: put every snapped window, including the dragged one, exactly into its zone.
    UpdateWindowPositions();
}

void WorkArea::ShowResizeCue(HWND draggedWindow, bool atBase)
{
    if (!m_layout || !m_baseGrid || !m_zonesOverlay)
    {
        return;
    }

    // Only the zone borders are drawn (fill opacity 0): the outlines of the base layout are its gridlines.
    const Colors::ZoneColors cue{
        .primaryColor = 0,
        .borderColor = atBase ? RGB(102, 204, 102) : RGB(90, 160, 255),
        .highlightColor = 0,
        .numberColor = 0,
        .highlightOpacity = 0,
    };

    const auto baseZones = LayoutConfigurator::GridZones(m_workAreaRect, *m_baseGrid, m_layout->Spacing());
    SetWorkAreaWindowAsTopmost(draggedWindow);
    m_zonesOverlay->DrawActiveZoneSet(baseZones, {}, cue, false);
    m_zonesOverlay->Show();
}
```

`FinishEdgeResize` reads `Find` after `ApplyEdgeResize`, which created the entry via `GetOrCreate`. `ApplyEdgeResize` also works when nothing changed (it simply returns).

- [ ] **Step 5: Build and run the tests**

Run build, then tests with filter `FullyQualifiedName~WorkArea`, then the full suite.
Expected: build `0`; new `WorkAreaEdgeResizeUnitTests` pass; no regressions versus the Task 0 baseline.

If `SnapsBackToBaseNearOriginalLine` or `MovingRightEdgeMovesWholeDivider` fails by 1 px, the cause is integer truncation in the percent conversion; adjust the test tolerance only, never the production arithmetic.

- [ ] **Step 6: Commit**

```powershell
git add src/modules/fancyzones/FancyZonesLib/WorkArea.h src/modules/fancyzones/FancyZonesLib/WorkArea.cpp src/modules/fancyzones/FancyZonesTests/UnitTests/WorkArea.Spec.cpp
git commit -m "feat(fancyzones): apply grid edge resizes through WorkArea"
```

---

### Task 6: WindowResizeHandler and FancyZones wiring

The handler turns a resize-cursor drag of a snapped window into `WorkArea` calls. It uses Win32 (DWM, cursor, tick count) and live windows, so it has no unit test; it is verified in Task 7.

**Files:**
- Create: `FancyZonesLib/WindowResizeHandler.h`
- Create: `FancyZonesLib/WindowResizeHandler.cpp`
- Modify: `FancyZonesLib/FancyZonesLib.vcxproj` (add `<ClInclude Include="WindowResizeHandler.h" />` and `<ClCompile Include="WindowResizeHandler.cpp" />`)
- Modify: `FancyZonesLib/FancyZones.cpp` (member at line 376, `MoveSizeStart` line 510, `MoveSizeEnd` line 540, `AbortMoveSize` line 553, `WM_PRIV_LOCATIONCHANGE` line 1006, `WM_PRIV_WINDOWDESTROYED` line 1018)

**Interfaces:**
- Consumes: `WorkAreaConfiguration::GetWorkAreaFromWindow` (`WorkArea* const GetWorkAreaFromWindow(HWND) const`), `WorkArea::SupportsEdgeResize`, `GetLayoutWindows().GetZoneIndexSetFromWindow(HWND)`, `ApplyEdgeResize`, `FinishEdgeResize`, `ShowResizeCue`, `HideZones` (Task 5), `DetectMovedEdges`, `MovedEdges` (Task 1).
- Produces:
  - `class WindowResizeHandler { explicit WindowResizeHandler(const WorkAreaConfiguration&); bool Begin(HWND); void Update(); void End(); void Abort(); bool IsTracking(HWND) const noexcept; }`

- [ ] **Step 1: Write the header**

Create `FancyZonesLib/WindowResizeHandler.h`:

```cpp
#pragma once

#include <FancyZonesLib/DividerModel.h>
#include <FancyZonesLib/Zone.h>

class WorkAreaConfiguration;
class WorkArea;

// Tracks one user resize (resize cursor drag) of a snapped window and turns it into grid divider moves.
// Work areas are looked up on every call because they can be recreated while a drag is in flight.
class WindowResizeHandler
{
public:
    explicit WindowResizeHandler(const WorkAreaConfiguration& workAreas) :
        m_workAreas(workAreas)
    {
    }

    // MOVESIZESTART with a resize cursor. Returns true when the window is snapped in a grid layout
    // and is now tracked.
    bool Begin(HWND window);

    // LOCATIONCHANGE. Cheap no-op when nothing is tracked, the frame did not change, or it is too soon after the last update.
    void Update();

    // MOVESIZEEND. Final pass using the window's actual frame.
    void End();

    // Stop tracking without applying anything (window destroyed, work areas recreated).
    void Abort();

    bool IsTracking(HWND window) const noexcept { return m_window != nullptr && m_window == window; }

private:
    static bool ReadFrame(HWND window, RECT& frame) noexcept;
    WorkArea* FindWorkArea() const;
    void Reset() noexcept;

    const WorkAreaConfiguration& m_workAreas;
    HWND m_window{};
    ZoneIndexSet m_zones;
    RECT m_startFrame{};
    RECT m_lastFrame{};
    FancyZonesDividers::MovedEdges m_moved;
    ULONGLONG m_lastUpdateTick{};
    bool m_cueAtBase = true;
    bool m_applying = false;
};
```

- [ ] **Step 2: Write the implementation**

Create `FancyZonesLib/WindowResizeHandler.cpp`:

```cpp
#include "pch.h"
#include "WindowResizeHandler.h"

#include <dwmapi.h>

#include <FancyZonesLib/WindowUtils.h>
#include <FancyZonesLib/WorkArea.h>
#include <FancyZonesLib/WorkAreaConfiguration.h>

namespace
{
    constexpr long MovedEdgeTolerancePx = 2; // frame changes smaller than this are not treated as an edge drag
    constexpr ULONGLONG MinUpdateIntervalMs = 16; // about 60 Hz; the final pass in End() catches up
}

bool WindowResizeHandler::ReadFrame(HWND window, RECT& frame) noexcept
{
    // Visible frame, without the invisible resize borders (same basis as WorkArea::Snap).
    return SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)));
}

WorkArea* WindowResizeHandler::FindWorkArea() const
{
    return m_window ? m_workAreas.GetWorkAreaFromWindow(m_window) : nullptr;
}

void WindowResizeHandler::Reset() noexcept
{
    m_window = nullptr;
    m_zones.clear();
    m_startFrame = {};
    m_lastFrame = {};
    m_moved = {};
    m_lastUpdateTick = 0;
    m_cueAtBase = true;
    m_applying = false;
}

bool WindowResizeHandler::Begin(HWND window)
{
    Reset();

    if (!window || (GetWindowLong(window, GWL_STYLE) & WS_SIZEBOX) == 0)
    {
        return false;
    }

    auto* workArea = m_workAreas.GetWorkAreaFromWindow(window);
    if (!workArea || !workArea->SupportsEdgeResize())
    {
        return false;
    }

    auto zones = workArea->GetLayoutWindows().GetZoneIndexSetFromWindow(window);
    if (zones.empty())
    {
        return false;
    }

    RECT frame{};
    if (!ReadFrame(window, frame))
    {
        return false;
    }

    m_window = window;
    m_zones = std::move(zones);
    m_startFrame = frame;
    m_lastFrame = frame;
    workArea->ShowResizeCue(window, true);
    return true;
}

void WindowResizeHandler::Update()
{
    if (!m_window || m_applying)
    {
        return;
    }

    RECT frame{};
    if (!ReadFrame(m_window, frame))
    {
        Abort();
        return;
    }

    // Repositioning the other windows raises LOCATIONCHANGE again; comparing frames makes that idempotent.
    if (EqualRect(&frame, &m_lastFrame))
    {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    if (now - m_lastUpdateTick < MinUpdateIntervalMs)
    {
        return;
    }

    auto* workArea = FindWorkArea();
    if (!workArea)
    {
        Abort();
        return;
    }

    m_applying = true;

    // Edges stay "moved" once they moved, so dragging an edge back to where it started still updates the divider.
    m_moved |= FancyZonesDividers::DetectMovedEdges(m_startFrame, frame, MovedEdgeTolerancePx);
    const auto result = workArea->ApplyEdgeResize(m_zones, frame, m_moved, m_window);

    m_lastFrame = frame;
    m_lastUpdateTick = now;

    if (result.atBase != m_cueAtBase)
    {
        m_cueAtBase = result.atBase;
        workArea->ShowResizeCue(m_window, m_cueAtBase);
    }

    m_applying = false;
}

void WindowResizeHandler::End()
{
    if (!m_window)
    {
        return;
    }

    if (auto* workArea = FindWorkArea())
    {
        RECT frame{};
        if (ReadFrame(m_window, frame))
        {
            m_moved |= FancyZonesDividers::DetectMovedEdges(m_startFrame, frame, MovedEdgeTolerancePx);
            workArea->FinishEdgeResize(m_zones, frame, m_moved);
        }

        workArea->HideZones();
    }

    Reset();
}

void WindowResizeHandler::Abort()
{
    if (m_window)
    {
        if (auto* workArea = FindWorkArea())
        {
            workArea->HideZones();
        }
    }

    Reset();
}
```

- [ ] **Step 3: Wire into `FancyZones.cpp`**

1. Add includes near the other `FancyZonesLib` includes at the top of the file:

```cpp
#include <FancyZonesLib/WindowResizeHandler.h>
```

2. Add the member directly after `WorkAreaConfiguration m_workAreaConfiguration;` (line 376):

```cpp
    WindowResizeHandler m_resizeHandler{ m_workAreaConfiguration };
```

3. In `FancyZones::MoveSizeStart` add as the first statement:

```cpp
    // Resizing a snapped window by its edge moves grid dividers instead of starting a snap drag.
    if (FancyZonesWindowUtils::IsCursorTypeIndicatingSizeEvent() && m_resizeHandler.Begin(window))
    {
        return;
    }

```

4. In `FancyZones::MoveSizeEnd` add as the first statement:

```cpp
    m_resizeHandler.End();

```

5. In `FancyZones::AbortMoveSize` add as the first statement:

```cpp
    m_resizeHandler.Abort();

```

6. In the `WM_PRIV_LOCATIONCHANGE` branch (line 1006) change the body to:

```cpp
        else if (message == WM_PRIV_LOCATIONCHANGE)
        {
            m_resizeHandler.Update();

            if (auto monitor = MonitorFromPoint(ptScreen, MONITOR_DEFAULTTONULL))
            {
                MoveSizeUpdate(monitor, ptScreen);
            }
        }
```

7. In the `WM_PRIV_WINDOWDESTROYED` branch add after `auto hwnd = ...;`:

```cpp
            if (m_resizeHandler.IsTracking(hwnd))
            {
                m_resizeHandler.Abort();
            }
```

`AbortMoveSize()` is already called before every `m_workAreaConfiguration.Clear()` in `OnDisplayChange` (see the comments at lines 1209-1216 and 1233), so work-area recreation aborts an in-flight resize without extra code. `Destroy()` clears the configuration with no resize in flight in the normal case.

- [ ] **Step 4: Build the whole FancyZones module**

```powershell
Push-Location src\modules\fancyzones\FancyZonesLib
..\..\..\..\tools\build\build.cmd
$LASTEXITCODE
Pop-Location
```

Expected: `0`. On failure read `artifacts\logs\FancyZonesLib\build.Debug.x64.errors.log`. If `DwmGetWindowAttribute` is unresolved, `dwmapi.lib` is already linked by `WindowUtils.cpp`; check that `#include <dwmapi.h>` is present.

- [ ] **Step 5: Re-run the unit tests**

Run the unit-test build and the full suite.
Expected: no new failures versus the Task 0 baseline.

- [ ] **Step 6: Commit**

```powershell
git add src/modules/fancyzones/FancyZonesLib/WindowResizeHandler.h src/modules/fancyzones/FancyZonesLib/WindowResizeHandler.cpp src/modules/fancyzones/FancyZonesLib/FancyZonesLib.vcxproj src/modules/fancyzones/FancyZonesLib/FancyZones.cpp
git commit -m "feat(fancyzones): resize snapped windows by moving grid dividers"
```

---

### Task 7: Run the real build and verify on real windows

Nothing in Tasks 1 to 6 proves live behavior. This task does. Tuning constants are only changed here, with evidence from real apps.

**Files:**
- Modify (only if tuning needs it): `FancyZonesLib/WorkArea.cpp` constants `MinTrackPx`, `SnapRadiusPx`; `FancyZonesLib/WindowResizeHandler.cpp` constants `MovedEdgeTolerancePx`, `MinUpdateIntervalMs`.

- [ ] **Step 1: Build and launch a self-built PowerToys**

Stop the installed PowerToys first (tray icon, Exit). Then:

```powershell
cd <workspace>\PowerToys
tools\build\build.cmd
$LASTEXITCODE
```

Expected: `0`. Start the built runner from `x64\Debug\PowerToys.exe`. Check that your managed machine allows running an unsigned self-built executable; if it is blocked, stop here and report it.

- [ ] **Step 2: Manual test checklist**

Use a "Grid" layout with 4 zones (2x2), then a PriorityGrid with 5 zones (a merged middle zone), on one monitor. Snap Notepad, Windows Terminal, Edge and an Electron app (VS Code or Teams) into zones with Shift-drag. Record pass/fail for each item:

1. Drag the right edge of the top-left window: neighbors in both rows follow live; no overlap and no gap.
2. While dragging, base gridlines appear (blue), and turn green when the dragged divider is near its base line.
3. Drag back near the original line: it snaps exactly; after release, the layout is the base layout again (windows exactly in base zones).
4. Corner drag moves two dividers at once.
5. Drag the window edge on the monitor border: nothing moves and the window returns to its zone.
6. Merged zone (PriorityGrid): dragging its left edge moves only the left divider of that zone.
7. Shrink a neighbor to its minimum: the divider stops at 90 px; no overlap even with an app that refuses to shrink (Teams or Excel).
8. Drag a different snapped window after modifying: the new geometry is used; Win+Arrow snapping and Shift-drag overlay show the modified zones.
9. Switch layout via the layout picker: the working layout is discarded and the new layout is shown with base geometry.
10. Hibernate and resume (and, if available, dock or undock a monitor): the modified geometry is still there.
11. Close a snapped window: the other windows stay where they are.
12. FancyZones editor: the base layout is unchanged; `custom-layouts.json` and `applied-layouts.json` are not modified (check their last-write times).
13. Dragging a snapped window by its title bar still snaps and moves as before (regression).

- [ ] **Step 3: Tune only with evidence**

For each failed or janky item, change the single relevant constant (`MinUpdateIntervalMs` for flicker or lag, `SnapRadiusPx` for stickiness, `MinTrackPx` for minimum size, `MovedEdgeTolerancePx` for accidental divider moves), rebuild, and re-run the affected checklist items. Do not change production arithmetic in `DividerModel` or `WorkArea` without a failing unit test first.

- [ ] **Step 4: Commit tuning, if any**

```powershell
git add -A src/modules/fancyzones
git commit -m "fix(fancyzones): tune dynamic grid resize constants"
```

---

### Task 8: Update the spec and record results

**Files:**
- Modify: `docs/superpowers/specs/2026-10-24-fancyzones-dynamic-grid-resize-design.md` (workspace root `<workspace>`)

- [ ] **Step 1: Close the resolved open items**

In the spec section "Open items to verify during implementation":
- Replace the `WorkArea::Snap` / `SaveWindowSizeAndOrigin` item with: "Resolved: `SaveWindowSizeAndOrigin` returns early when the restore-size property already exists, so repeated repositioning does not overwrite the saved size."
- Replace the work-area-rebuild item with the Task 4/5 result: working layouts are keyed by `WorkAreaId::toString()` (device id, instance, serial, desktop), not by `HMONITOR`, and the Task 7 hibernate check result.
- Replace the tuning item with the final constants chosen in Task 7.
- Add to "Out of scope": "Persistent faint base lines while a layout is modified (the cue is shown only while dragging, as 1 px solid zone outlines)."

- [ ] **Step 2: Commit the repository work**

The workspace root is not a git repository, so the spec has no commit. The implementation lives on branch `feature/dynamic-grid-resize` in the `PowerToys` clone. Report the final `git log --oneline` of that branch and the Task 7 checklist results to the user.

---

## Self-Review

**Spec coverage**

| Spec requirement | Task |
|---|---|
| Fork PowerToys, no companion app | Tasks 0, 1 to 6 |
| Whole divider line moves | Task 1 (`Move`, `MoveMovesWholeLine`), Task 5 (`MovingRightEdgeMovesWholeDivider`) |
| Grid layouts only (Columns, Rows, Grid, PriorityGrid, custom Grid); Focus/Canvas unchanged | Task 2 (`GridInfo`, nullopt cases), Task 5 (`SupportsEdgeResize`) |
| In-memory, per work area, survives hibernate, never writes files | Task 4 (`WorkingLayouts`, `EntryIgnoresHMonitor`), Task 5 (`RecreatedWorkAreaKeepsWorkingLayout`), Task 7 items 10 and 12 |
| Discard on layout change / base grid change | Task 4 (`DifferentLayoutIdDiscardsEntry`, `DifferentBaseDiscardsEntry`), Task 5 (`ChangingLayoutDiscardsWorkingLayout`) |
| Resize flow: start, throttled update, end with final pass | Task 6 (`WindowResizeHandler`), Task 5 (`FinishEdgeResize`) |
| Outer edges ignored | Task 1 (`OuterEdgesHaveNoDivider`), Task 5 (`OuterEdgeDoesNothing`) |
| Multi-zone windows only move outer dividers | Task 1 (`MergedZoneUsesItsOuterDividers`), Task 7 item 6 |
| Minimum zone size | Task 1 (`MoveClampsToMinimumTrack`), Task 5 (`DividerIsClampedToMinimumZoneSize`) |
| Snap to base line; reset by dragging back; entry removed when all at base | Task 1 (`MoveSnapsToBaseWithinRadius`), Task 5 (`SnapsBackToBaseNearOriginalLine`, `FinishRemovesEntryWhenBackAtBase`) |
| Base gridline cue while dragging, highlight at base | Task 5 (`ShowResizeCue`), Task 6 (`Begin`, `Update`), Task 7 items 2 and 3 |
| Non-resizable windows | Task 6 (`WS_SIZEBOX` check in `Begin`) |
| Re-entrancy from repositioning | Task 6 (`m_applying`, frame comparison, throttle) |
| Win+arrow and drag-to-zone use the working layout | Automatic via `m_layout` (Task 5); verified in Task 7 item 8 |
| Spacing and DWM invisible borders | Task 5 (`halfSpacing`), Task 6 (`DWMWA_EXTENDED_FRAME_BOUNDS`) |
| Open items (restore-size, rebuild timing, tuning) | Task 8 (first resolved by reading `SaveWindowSizeAndOrigin`) |
| Optional faint lines while modified | Deliberately dropped, recorded in the header and Task 8 |

**Placeholder scan:** no "TBD", "TODO" or "similar to Task N". Every code step contains the code. The only `Task 7` actions are explicit manual checks and named constants.

**Type consistency:** `Divider`, `Axis`, `Span`, `MovedEdges`, `DetectMovedEdges`, `TotalUnits` (Task 1) are used with the same names and signatures in Tasks 4 to 6. `EdgeResizeResult`, `ApplyEdgeResize(zones, currentFrame, moved, skipWindow)`, `FinishEdgeResize(zones, finalFrame, moved)`, `ShowResizeCue(HWND, bool)`, `SupportsEdgeResize()` are defined in Task 5 and called identically in Task 6. `LayoutConfigurator::GridInfo(const LayoutData&)` and `GridZones(Rect, const GridLayoutInfo&, int)` (Task 2) match their uses in Tasks 3 and 5. `WorkingLayouts::Find/GetOrCreate/Remove/Clear` (Task 4) match Task 5.

**Known risks the executor should watch:** (1) the unit-test `Find` call in `FinishRemovesEntryWhenBackAtBase` relies on `m_workArea->GetLayoutId()` returning the layout uuid set in the test; (2) template layouts share `GUID_NULL`, so identity relies on the base-grid comparison (documented in `WorkingLayouts::Find`); (3) `Columns` and `Rows` layouts switch from their dedicated zone formulas to the grid formula only while a working layout is active, which can differ by up to 1 px (covered by `GridZonesMatchesColumnsLayoutWithinOnePixel`).
