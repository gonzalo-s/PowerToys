#pragma once

#include <guiddef.h>

#include <FancyZonesLib/DividerModel.h>
#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/Zone.h>

class WorkAreaConfiguration;
class WorkArea;

// Tracks one user resize (resize cursor drag) of a snapped window and turns it into grid divider moves.
// Additive to the natural FancyZones resize: edges on the work-area border (or without a divider) are left to the
// user and nothing else moves; only edges on a divider move that divider and the windows whose zones changed.
// The handler is bound to the work area (WorkAreaId) and layout the drag started on. The work area object is looked
// up by that id on every call because it can be recreated while a drag is in flight; if it is gone or its layout
// changed, the drag is aborted.
class WindowResizeHandler
{
public:
    explicit WindowResizeHandler(const WorkAreaConfiguration& workAreas) :
        m_workAreas(workAreas)
    {
    }

    // MOVESIZESTART with a resize cursor. Returns true when the window is snapped in a grid layout
    // and is now tracked. Shows nothing yet.
    bool Begin(HWND window);

    // LOCATIONCHANGE. Cheap no-op when nothing is tracked, the frame did not change, or it is too soon after the last update.
    // Shows the base-gridline cue the first time a divider edge is dragged, then toggles its colour (at base / moved).
    void Update();

    // MOVESIZEEND. Final pass using the window's actual frame. The dragged window is placed into its zones only when
    // every moved edge was a divider edge; after an outer-edge (or mixed) resize it stays where the user left it.
    void End();

    // Stop tracking (window destroyed, drag cancelled, work area gone or layout changed). Hides the cue and, if the
    // working layout of the bound work area is unmodified, discards it and re-places the snapped windows. A layout
    // already modified by live updates is kept.
    void Abort();

    bool IsTracking(HWND window) const noexcept { return m_window != nullptr && m_window == window; }

private:
    static bool ReadFrame(HWND window, RECT& frame) noexcept;
    WorkArea* FindWorkArea() const;
    WorkArea* FindWorkAreaWithSameLayout() const; // nullptr when the bound work area is gone or its layout changed
    void Reset() noexcept;

    const WorkAreaConfiguration& m_workAreas;
    HWND m_window{};
    ZoneIndexSet m_zones;
    RECT m_startFrame{};
    RECT m_lastFrame{};
    FancyZonesDividers::MovedEdges m_moved;
    ULONGLONG m_lastUpdateTick{};
    bool m_cueAtBase = true;
    bool m_cueShown = false; // the cue is shown only once a divider edge is dragged
    FancyZonesDataTypes::WorkAreaId m_workAreaId{}; // work area the drag started on
    GUID m_layoutId{}; // layout applied on that work area when the drag started
};
