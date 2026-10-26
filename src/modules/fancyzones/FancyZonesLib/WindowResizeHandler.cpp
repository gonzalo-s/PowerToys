#include "pch.h"
#include "WindowResizeHandler.h"

#include <dwmapi.h>

#include <algorithm>

#include <common/logger/logger.h>

#include <FancyZonesLib/FancyZonesWindowProcessing.h>
#include <FancyZonesLib/WindowUtils.h>
#include <FancyZonesLib/WorkArea.h>
#include <FancyZonesLib/WorkAreaConfiguration.h>

namespace
{
    constexpr long MovedEdgeTolerancePx = 2; // frame changes smaller than this are not treated as an edge drag
    constexpr ULONGLONG MinUpdateIntervalMs = 16; // about 60 Hz; the final pass in End() catches up
    constexpr ULONGLONG LogIntervalMs = 100; // at most one snapped/clamped diagnostics line per 100 ms while dragging
    constexpr long ClampFeedbackTolerancePx = 2; // the dragged edge is held at a clamp limit only beyond this distance
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
    m_lastLogTick = 0;
    m_cueSnaps.clear();
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

    const auto& rect = workArea->GetWorkAreaRect();
    Logger::info("Edge resize begin: window {}, zones {}, start frame {}, work area {} {}",
                 static_cast<void*>(window), EdgeResizeLogText::Zones(m_zones), EdgeResizeLogText::Rect(frame),
                 winrt::to_string(m_workAreaId.toString()), EdgeResizeLogText::Rect(RECT{ rect.left(), rect.top(), rect.right(), rect.bottom() }));
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
    m_moved |= FancyZonesZoneRects::DetectMovedEdges(m_startFrame, frame, MovedEdgeTolerancePx);
    const auto result = workArea->ApplyEdgeResize(m_zones, frame, m_moved, m_window);

    m_lastFrame = frame;
    m_lastUpdateTick = now;

    // Diagnostics: only snapped or clamped updates, at most one line per LogIntervalMs.
    const bool interesting = std::any_of(result.sides.begin(), result.sides.end(), [](const EdgeResizeSide& side) { return side.snapped || side.clamped; });
    if (interesting && now - m_lastLogTick >= LogIntervalMs)
    {
        m_lastLogTick = now;
        Logger::info("Edge resize update: window {}, frame {}, moved {}, sides: {}",
                     static_cast<void*>(m_window), EdgeResizeLogText::Rect(frame), EdgeResizeLogText::Moved(m_moved), EdgeResizeLogText::Sides(result.sides));
    }

    if (result.clamped)
    {
        HoldAtClampLimit(*workArea, frame, result.clampedFrame);
    }

    // Redraw the cue when it appears and whenever the highlighted line changes (snapped on/off, another line,
    // own/other line); the 16 ms throttle above still applies.
    if (result.dividerDragged && (!m_cueShown || result.snaps != m_cueSnaps))
    {
        m_cueShown = true;
        m_cueSnaps = result.snaps;
        workArea->ShowResizeCue(m_window, m_cueSnaps);
    }
}

void WindowResizeHandler::HoldAtClampLimit(WorkArea& workArea, const RECT& frame, const RECT& limitFrame)
{
    // Live clamp feedback: the zone wall stopped at a limit (a pushed window's or the dragged window's own minimum, or
    // the work-area border margin) while the mouse kept going. Put the clamped edge(s) of the dragged window on the
    // limit so what the user sees while dragging is where the window will end up. Only the clamped edges change
    // (limitFrame keeps the other edges of frame). The OS sizing loop re-applies the mouse position on the next mouse
    // move; the next (throttled) update clamps again, so the edge may jitter briefly past the limit but the final
    // position (FinishEdgeResize) is the same limit.
    const auto beyondTolerance = [](long a, long b) { return (a > b ? a - b : b - a) > ClampFeedbackTolerancePx; };
    if (!beyondTolerance(frame.left, limitFrame.left) && !beyondTolerance(frame.top, limitFrame.top) &&
        !beyondTolerance(frame.right, limitFrame.right) && !beyondTolerance(frame.bottom, limitFrame.bottom))
    {
        return; // already (almost) at the limit: no SetWindowPos, no LOCATIONCHANGE feedback loop
    }

    // Same border adjustment as the windows placed into zones: AdjustRectForSizeWindowToRect takes a rect relative
    // to the work-area window and returns screen coordinates.
    const auto& workAreaRect = workArea.GetWorkAreaRect();
    RECT local = limitFrame;
    OffsetRect(&local, -workAreaRect.left(), -workAreaRect.top());
    const RECT adjusted = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(m_window, local, workArea.GetWorkAreaWindow());
    SetWindowPos(m_window, nullptr, adjusted.left, adjusted.top, adjusted.right - adjusted.left, adjusted.bottom - adjusted.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
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
        m_moved |= FancyZonesZoneRects::DetectMovedEdges(m_startFrame, frame, MovedEdgeTolerancePx);
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
