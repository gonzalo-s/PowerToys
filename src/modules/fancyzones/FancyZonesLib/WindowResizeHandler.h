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
