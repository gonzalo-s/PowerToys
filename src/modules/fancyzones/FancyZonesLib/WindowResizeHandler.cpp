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
            workArea->DiscardUnmodifiedWorkingLayout();
        }
    }

    Reset();
}
