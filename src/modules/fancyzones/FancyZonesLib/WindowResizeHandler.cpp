#include "pch.h"
#include "WindowResizeHandler.h"

#include <dwmapi.h>

#include <FancyZonesLib/FancyZonesWindowProcessing.h>
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
    if (!m_window)
    {
        return nullptr;
    }

    // Bound to the work area the drag started on, even if the window is dragged (mostly) onto another monitor.
    for (const auto& [monitor, workArea] : m_workAreas.GetAllWorkAreas())
    {
        if (workArea && workArea->UniqueId() == m_workAreaId)
        {
            return workArea.get();
        }
    }

    return nullptr;
}

WorkArea* WindowResizeHandler::FindWorkAreaWithSameLayout() const
{
    auto* workArea = FindWorkArea();
    return workArea && workArea->GetLayoutId() == m_layoutId ? workArea : nullptr;
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
    m_cueShown = false;
    m_workAreaId = {};
    m_layoutId = GUID_NULL;
}

bool WindowResizeHandler::Begin(HWND window)
{
    if (m_window)
    {
        // A previous drag never ended: clean it up (cue, unmodified working entry) before starting a new one.
        Abort();
    }

    Reset();

    if (!window || (GetWindowLong(window, GWL_STYLE) & WS_SIZEBOX) == 0 || !FancyZonesWindowProcessing::IsProcessableManually(window))
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
    m_workAreaId = workArea->UniqueId();
    m_layoutId = workArea->GetLayoutId();
    // No cue yet: it appears only once a divider edge is actually dragged (see Update). An outer-edge resize
    // stays the natural FancyZones resize, without any visual change.
    return true;
}

void WindowResizeHandler::Update()
{
    if (!m_window)
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

    auto* workArea = FindWorkAreaWithSameLayout();
    if (!workArea)
    {
        // Work area gone or layout changed mid-drag (stale zone indices): stop tracking.
        Abort();
        return;
    }

    // Edges stay "moved" once they moved, so dragging an edge back to where it started still updates the divider.
    m_moved |= FancyZonesDividers::DetectMovedEdges(m_startFrame, frame, MovedEdgeTolerancePx);
    const auto result = workArea->ApplyEdgeResize(m_zones, frame, m_moved, m_window);

    m_lastFrame = frame;
    m_lastUpdateTick = now;

    if (result.dividerDragged && (!m_cueShown || result.atBase != m_cueAtBase))
    {
        m_cueShown = true;
        m_cueAtBase = result.atBase;
        workArea->ShowResizeCue(m_window, m_cueAtBase);
    }
}

void WindowResizeHandler::End()
{
    if (!m_window)
    {
        return;
    }

    auto* workArea = FindWorkAreaWithSameLayout();
    if (!workArea)
    {
        Abort();
        return;
    }

    RECT frame{};
    if (ReadFrame(m_window, frame))
    {
        m_moved |= FancyZonesDividers::DetectMovedEdges(m_startFrame, frame, MovedEdgeTolerancePx);
        workArea->FinishEdgeResize(m_zones, frame, m_moved, m_window);
    }
    else
    {
        // frame unreadable: nothing to commit, drop an unmodified working entry like Abort() does
        workArea->DiscardUnmodifiedWorkingLayout();
    }

    if (m_cueShown)
    {
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
            if (m_cueShown)
            {
                workArea->HideZones();
            }

            workArea->DiscardUnmodifiedWorkingLayout();
        }
    }

    Reset();
}
