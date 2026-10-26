#include "pch.h"
#include "WorkArea.h"

#include <common/logger/logger.h>

#include "FancyZonesData/AppliedLayouts.h"
#include "FancyZonesData/AppZoneHistory.h"
#include "FancyZonesData/CustomLayouts.h"
#include "ZonesOverlay.h"
#include "Settings.h"
#include "WorkingLayouts.h"

#include <algorithm>
#include <array>
#include <FancyZonesLib/FancyZonesWindowProperties.h>
#include <FancyZonesLib/VirtualDesktop.h>
#include <FancyZonesLib/WindowUtils.h>

// disabling warning 4458 - declaration of 'identifier' hides class member
// to avoid warnings from GDI files - can't add winRT directory to external code
// in the Cpp.Build.props
#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

// Non-Localizable strings
namespace NonLocalizable
{
    const wchar_t ToolWindowClassName[] = L"FancyZones_ZonesOverlay";
    const wchar_t ToolWindowName[] = L"FancyZones_ZonesOverlay";
}

using namespace FancyZonesUtils;

namespace
{
    // The reason for using this class is the need to call ShowWindow(window, SW_SHOWNORMAL); on each
    // newly created window for it to be displayed properly. The call sometimes has side effects when
    // a fullscreen app is running, and happens when the resolution change event is triggered
    // (e.g. when running some games).
    // This class will serve as a pool of windows for which this call was already done.
    class WindowPool
    {
        std::vector<HWND> m_pool;
        std::mutex m_mutex;

        HWND ExtractWindow()
        {
            std::unique_lock lock(m_mutex);

            if (m_pool.empty())
            {
                return NULL;
            }

            HWND window = m_pool.back();
            m_pool.pop_back();
            return window;
        }

    public:
        HWND NewZonesOverlayWindow(Rect position, HINSTANCE hinstance, WorkArea* owner)
        {
            HWND windowFromPool = ExtractWindow();
            if (windowFromPool == NULL)
            {
                HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, NonLocalizable::ToolWindowClassName, NonLocalizable::ToolWindowName, WS_POPUP, position.left(), position.top(), position.width(), position.height(), nullptr, nullptr, hinstance, owner);
                Logger::info("Creating new ZonesOverlay window, hWnd = {}", (void*)window);
                FancyZonesWindowUtils::MakeWindowTransparent(window);

                // According to ShowWindow docs, we must call it with SW_SHOWNORMAL the first time
                ShowWindow(window, SW_SHOWNORMAL);
                ShowWindow(window, SW_HIDE);
                return window;
            }
            else
            {
                Logger::info("Reusing ZonesOverlay window from pool, hWnd = {}", (void*)windowFromPool);
                SetWindowLongPtrW(windowFromPool, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
                MoveWindow(windowFromPool, position.left(), position.top(), position.width(), position.height(), TRUE);
                return windowFromPool;
            }
        }

        void FreeZonesOverlayWindow(HWND window)
        {
            Logger::info("Freeing ZonesOverlay window into pool, hWnd = {}", (void*)window);
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            ShowWindow(window, SW_HIDE);

            std::unique_lock lock(m_mutex);
            m_pool.push_back(window);
        }

        ~WindowPool()
        {
            for (HWND window : m_pool)
            {
                DestroyWindow(window);
            }
        }
    };

    WindowPool windowPool;
}

namespace
{
    constexpr long MinSizePx = 50; // no zone may become narrower/shorter than this while resizing (capped per zone by its current and base size)
    constexpr long SnapRadiusPx = 20; // distance from its base position at which a dragged side snaps back to it (magnet)

    // Ceiling of numerator / denominator for denominator > 0 (C++ division truncates toward zero, which already is the
    // ceiling for negative quotients). Converting a pixel edge with it makes floor(units * total / TotalUnits) return
    // the same pixel again (total < TotalUnits), so a released edge does not move by 1 px.
    constexpr long long CeilDiv(long long numerator, long long denominator) noexcept
    {
        const long long quotient = numerator / denominator;
        return (numerator % denominator != 0 && numerator > 0) ? quotient + 1 : quotient;
    }
}

// Diagnostics text for the edge-resize log lines (no window titles: HWND values and rectangles only).
std::string EdgeResizeLogText::Rect(const RECT& rect)
{
    return "(" + std::to_string(rect.left) + "," + std::to_string(rect.top) + "," + std::to_string(rect.right) + "," + std::to_string(rect.bottom) + ")";
}

std::string EdgeResizeLogText::Zones(const ZoneIndexSet& zones)
{
    std::string text = "{";
    for (const ZoneIndex id : zones)
    {
        text += (text.size() > 1 ? "," : "") + std::to_string(id);
    }

    return text + "}";
}

std::string EdgeResizeLogText::Moved(const FancyZonesZoneRects::MovedEdges& moved)
{
    std::string text;
    text += moved.left ? "L" : "";
    text += moved.top ? "T" : "";
    text += moved.right ? "R" : "";
    text += moved.bottom ? "B" : "";
    return text.empty() ? "-" : text;
}

std::string EdgeResizeLogText::Sides(const std::vector<EdgeResizeSide>& sides)
{
    using FancyZonesZoneRects::Side;
    std::string text;
    for (const auto& side : sides)
    {
        const char* name = side.side == Side::Left ? "left" : side.side == Side::Top ? "top" : side.side == Side::Right ? "right" : "bottom";
        text += std::string(text.empty() ? "" : "; ") + name + " requested " + std::to_string(side.requested) + " final " + std::to_string(side.final) +
                (side.snapped ? " snapped" : "") + (side.clamped ? " clamped" : "");
    }

    return text.empty() ? "-" : text;
}

namespace
{
    std::string UnitBoundsText(const std::optional<FancyZonesZoneRects::UnitRect>& rect)
    {
        if (!rect)
        {
            return "base";
        }

        return "[" + std::to_string(rect->left) + "," + std::to_string(rect->top) + "," + std::to_string(rect->right) + "," + std::to_string(rect->bottom) + "]";
    }
}

WorkArea::WorkArea(HINSTANCE hinstance, const FancyZonesDataTypes::WorkAreaId& uniqueId, const FancyZonesUtils::Rect& workAreaRect) :
    m_uniqueId(uniqueId),
    m_workAreaRect(workAreaRect)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = s_WndProc;
    wcex.hInstance = hinstance;
    wcex.lpszClassName = NonLocalizable::ToolWindowClassName;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wcex);
}

WorkArea::~WorkArea()
{
    // Tear down the renderer (joining its background thread) before returning
    // the HWND to the pool. Otherwise, the render thread can still be drawing
    // through m_renderTarget into an HWND that has already been recycled by a
    // subsequent NewZonesOverlayWindow call.
    m_zonesOverlay.reset();
    windowPool.FreeZonesOverlayWindow(m_window);
}

bool WorkArea::Snap(HWND window, const ZoneIndexSet& zones, bool updatePosition)
{
    if (!m_layout || zones.empty())
    {
        return false;
    }

    for (ZoneIndex zone : zones)
    {
        if (static_cast<size_t>(zone) >= m_layout->Zones().size())
        {
            return false;
        }
    }

    m_layoutWindows.Assign(window, zones);
    AppZoneHistory::instance().SetAppLastZones(window, m_uniqueId, m_layout->Id(), zones);

    if (updatePosition)
    {
        // The working zones when the layout was resized (the layout's own zones otherwise).
        const auto rect = Layout::CombinedRect(ActiveZones(), zones);
        const auto adjustedRect = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(window, rect, m_window);
        FancyZonesWindowUtils::SaveWindowSizeAndOrigin(window);
        FancyZonesWindowUtils::SizeWindowToRect(window, adjustedRect);
    }

    return FancyZonesWindowProperties::StampZoneIndexProperty(window, zones);
}

bool WorkArea::Unsnap(HWND window)
{
    if (!m_layout)
    {
        return false;
    }
    
    m_layoutWindows.Dismiss(window);
    AppZoneHistory::instance().RemoveAppLastZone(window, m_uniqueId, m_layout->Id());
    FancyZonesWindowProperties::RemoveZoneIndexProperty(window);

    return true;
}

const GUID WorkArea::GetLayoutId() const noexcept
{
    if (m_layout)
    {
        return m_layout->Id();
    }

    return GUID{};
}

void WorkArea::ShowZones(const ZoneIndexSet& highlight, HWND draggedWindow/* = nullptr*/)
{
    if (m_layout && m_zonesOverlay)
    {
        SetWorkAreaWindowAsTopmost(draggedWindow);
        m_zonesOverlay->DrawActiveZoneSet(m_layout->Zones(), highlight, Colors::GetZoneColors(), FancyZonesSettings::settings().showZoneNumber);
        m_zonesOverlay->Show();
    }
}

void WorkArea::HideZones()
{
    if (m_zonesOverlay)
    {
        m_zonesOverlay->Hide();
    }
}

void WorkArea::FlashZones()
{
    if (m_layout && m_zonesOverlay)
    {
        SetWorkAreaWindowAsTopmost(nullptr);
        m_zonesOverlay->DrawActiveZoneSet(m_layout->Zones(), {}, Colors::GetZoneColors(), FancyZonesSettings::settings().showZoneNumber);
        m_zonesOverlay->Flash();
    }
}

void WorkArea::ShowLayoutNameLabel(const std::wstring& name)
{
    if (m_zonesOverlay && FancyZonesSettings::settings().layoutNameLabelEnabled)
    {
        const auto& settings = FancyZonesSettings::settings();
        const ZonesOverlay::LayoutNameLabelOptions options{
            .textColor = FancyZonesUtils::HexToRGB(settings.layoutNameLabelTextColor),
            .backgroundColor = FancyZonesUtils::HexToRGB(settings.layoutNameLabelBackgroundColor, RGB(38, 38, 38)),
            .fontSize = settings.layoutNameLabelFontSize,
            .padding = settings.layoutNameLabelPadding,
            .durationMillis = settings.layoutNameLabelDuration,
            .placement = settings.layoutNameLabelPlacement,
        };
        m_zonesOverlay->ShowLayoutName(name, options);
    }
}

void WorkArea::ShowMonitorRotationPreview(const std::vector<RECT>& windowRects, size_t monitorNumber, std::optional<bool> reverse, bool animateRotation)
{
    if (!m_zonesOverlay)
    {
        return;
    }

    std::vector<RECT> localWindowRects;
    localWindowRects.reserve(windowRects.size());
    for (const auto& rect : windowRects)
    {
        localWindowRects.push_back(RECT{
            .left = rect.left - m_workAreaRect.left(),
            .top = rect.top - m_workAreaRect.top(),
            .right = rect.right - m_workAreaRect.left(),
            .bottom = rect.bottom - m_workAreaRect.top(),
        });
    }

    SetWorkAreaWindowAsTopmost(nullptr);
    m_zonesOverlay->DrawMonitorRotationPreview(localWindowRects, monitorNumber, reverse, animateRotation);
    m_zonesOverlay->Show();
}

void WorkArea::InitLayout()
{
    InitLayout({});

    if (m_window && m_layout)
    {
        m_zonesOverlay->DrawActiveZoneSet(m_layout->Zones(), {}, Colors::GetZoneColors(), FancyZonesSettings::settings().showZoneNumber);
    }
}

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

void WorkArea::CycleWindows(HWND window, bool reverse)
{
    m_layoutWindows.CycleWindows(window, reverse);
}

namespace
{
    std::optional<RECT> CombinedZonesRect(const ZonesMap& zonesMap, const ZoneIndexSet& zones)
    {
        std::optional<RECT> combined;
        for (ZoneIndex id : zones)
        {
            const auto it = zonesMap.find(id);
            if (it == zonesMap.end())
            {
                return std::nullopt;
            }

            const RECT rect = it->second.GetZoneRect();
            if (!combined)
            {
                combined = rect;
            }
            else
            {
                combined->left = (std::min)(combined->left, rect.left);
                combined->top = (std::min)(combined->top, rect.top);
                combined->right = (std::max)(combined->right, rect.right);
                combined->bottom = (std::max)(combined->bottom, rect.bottom);
            }
        }

        return combined;
    }
}

bool WorkArea::ZonesRectChanged(const ZonesMap& before, const ZonesMap& after, const ZoneIndexSet& zones)
{
    const auto beforeRect = CombinedZonesRect(before, zones);
    const auto afterRect = CombinedZonesRect(after, zones);
    if (!beforeRect || !afterRect)
    {
        return true;
    }

    return EqualRect(&*beforeRect, &*afterRect) == FALSE;
}

void WorkArea::RepositionSnappedWindows(HWND skipWindow, const ZonesMap* previousZones)
{
    // Lightweight live-resize path (called up to ~60 Hz): only moves windows. Unlike Snap it does not restore
    // minimized/maximized windows, write app-zone history, reassign windows or touch window properties.
    // With previousZones, windows whose zones kept the same combined rect are left alone, so a window the user
    // resized freely earlier is not pulled back into its zone by an unrelated divider move.
    if (!m_layout)
    {
        return;
    }

    const auto& layoutZones = ActiveZones();
    const auto snappedWindows = m_layoutWindows.SnappedWindows();
    for (const auto& [window, zones] : snappedWindows)
    {
        if (window == skipWindow)
        {
            continue;
        }

        if (previousZones && !ZonesRectChanged(*previousZones, layoutZones, zones))
        {
            continue;
        }

        RepositionWindow(window, zones);
    }
}

void WorkArea::RepositionWindow(HWND window, const ZoneIndexSet& zones)
{
    if (!m_layout || !IsWindow(window) || !IsWindowVisible(window) || IsIconic(window) || IsZoomed(window))
    {
        return;
    }

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
    {
        return;
    }

    const auto& layoutZones = ActiveZones();
    if (zones.empty() || std::any_of(zones.begin(), zones.end(), [&layoutZones](ZoneIndex id) { return !layoutZones.contains(id); }))
    {
        return;
    }

    const auto rect = Layout::CombinedRect(layoutZones, zones);
    const auto adjusted = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(window, rect, m_window);

    RECT current{};
    if (GetWindowRect(window, &current) && EqualRect(&current, &adjusted))
    {
        return;
    }

    SetWindowPos(window, nullptr, adjusted.left, adjusted.top, adjusted.right - adjusted.left, adjusted.bottom - adjusted.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS);
}

EdgeResizeResult WorkArea::ApplyEdgeResize(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesZoneRects::MovedEdges& moved, HWND skipWindow)
{
    return ApplyEdgeResizeInternal(zones, currentFrame, moved, skipWindow, true);
}

EdgeResizeResult WorkArea::ApplyEdgeResizeInternal(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesZoneRects::MovedEdges& moved, HWND skipWindow, bool reposition)
{
    using namespace FancyZonesZoneRects;

    EdgeResizeResult result;
    result.clampedFrame = currentFrame;
    if (!m_layout || !m_baseRects)
    {
        return result;
    }

    // Boundary between two zones = window edge +/- half the spacing between zones. Vertical sides first, then
    // horizontal sides.
    struct DraggedSide
    {
        Side side;
        bool moved;
        long edge; // boundary relative to the work-area origin, in pixels
        long length; // work-area length on the side's axis
        long frameEdge; // the window edge (screen)
        long toFrame; // boundary (relative) -> window edge: + origin -/+ half spacing
    };

    const int halfSpacing = m_layout->Spacing() / 2;
    const long left = m_workAreaRect.left();
    const long top = m_workAreaRect.top();
    const long width = m_workAreaRect.width();
    const long height = m_workAreaRect.height();
    const std::array<DraggedSide, 4> sides{ {
        { Side::Left, moved.left, currentFrame.left - halfSpacing - left, width, currentFrame.left, left + halfSpacing },
        { Side::Right, moved.right, currentFrame.right + halfSpacing - left, width, currentFrame.right, left - halfSpacing },
        { Side::Top, moved.top, currentFrame.top - halfSpacing - top, height, currentFrame.top, top + halfSpacing },
        { Side::Bottom, moved.bottom, currentFrame.bottom + halfSpacing - top, height, currentFrame.bottom, top - halfSpacing },
    } };

    // Classify the moved sides first, without creating a working layout entry: an edge on the work-area border is
    // the natural FancyZones resize and must not create or touch an entry.
    ZoneRectModel* existing = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects);
    {
        std::optional<ZoneRectModel> probe;
        const ZoneRectModel& lookup = existing ? *existing : probe.emplace(*m_baseRects);
        if (!lookup.BoundsOf(zones))
        {
            return result;
        }

        for (const auto& dragged : sides)
        {
            if (!dragged.moved)
            {
                continue;
            }

            if (lookup.HasNeighbour(zones, dragged.side))
            {
                result.dividerDragged = true;
            }
            else
            {
                result.outerEdgeMoved = true;
            }
        }
    }

    if (!result.dividerDragged)
    {
        return result;
    }

    auto& model = existing ? *existing : WorkingLayouts::instance().GetOrCreate(m_uniqueId, m_layout->Id(), *m_baseRects);
    const auto sideOf = [](const UnitRect& rect, Side side) {
        switch (side)
        {
        case Side::Left:
            return rect.left;
        case Side::Top:
            return rect.top;
        case Side::Right:
            return rect.right;
        default:
            return rect.bottom;
        }
    };

    // Only zones holding a window take part; the dragged window's own zones always do (see ZoneRectModel::MoveEdge).
    const FancyZonesZoneRects::OccupiedFn occupied = [this](ZoneIndex id) { return !m_layoutWindows.IsZoneEmpty(id); };

    for (const auto& dragged : sides)
    {
        if (!dragged.moved || dragged.length <= 0 || !model.HasNeighbour(zones, dragged.side))
        {
            continue;
        }

        // Ceiling: the rebuilt zone edge lands exactly on the dragged pixel edge.
        const int position = static_cast<int>(CeilDiv(static_cast<long long>(dragged.edge) * TotalUnits, dragged.length));
        const int minSize = static_cast<int>(static_cast<long long>(MinSizePx) * TotalUnits / dragged.length);
        const int snapRadius = static_cast<int>(static_cast<long long>(SnapRadiusPx) * TotalUnits / dragged.length);
        const auto outcome = model.MoveEdge(zones, dragged.side, position, minSize, snapRadius, occupied);
        if (outcome.result == MoveResult::Moved)
        {
            result.changed = true;
        }

        if (outcome.snapped)
        {
            const bool vertical = dragged.side == Side::Left || dragged.side == Side::Right;
            result.snaps.push_back(EdgeSnap{ vertical, outcome.snappedPosition, outcome.ownBaseLine });
        }

        // Where the side ended, as a window edge (the same floor maths as Layout::ZonesFromRects).
        const long finalEdge = static_cast<long>(static_cast<long long>(dragged.length) * outcome.position / TotalUnits) + dragged.toFrame;
        result.sides.push_back(EdgeResizeSide{ dragged.side, dragged.frameEdge, finalEdge, outcome.snapped, outcome.clamped });
        if (outcome.clamped)
        {
            result.clamped = true;
            switch (dragged.side)
            {
            case Side::Left:
                result.clampedFrame.left = finalEdge;
                break;
            case Side::Top:
                result.clampedFrame.top = finalEdge;
                break;
            case Side::Right:
                result.clampedFrame.right = finalEdge;
                break;
            default:
                result.clampedFrame.bottom = finalEdge;
                break;
            }
        }

        // The dragged block's side is back where it is in the base layout.
        const auto bounds = model.BoundsOf(zones);
        const auto baseBounds = model.BaseBoundsOf(zones);
        result.atBase &= bounds && baseBounds && sideOf(*bounds, dragged.side) == sideOf(*baseBounds, dragged.side);
    }

    if (result.changed)
    {
        // Only the working zones (window positions) change; the layout keeps its original zones.
        const ZonesMap previousZones = ActiveZones();
        auto working = Layout::ZonesFromRects(m_workAreaRect, model.Current(), m_layout->Spacing(), m_layout->Zones().size());
        if (working)
        {
            m_workingZones = std::move(*working);
        }
        else
        {
            // Should not happen (the model keeps every zone at least as large as its minimum); fall back to the
            // base layout.
            Logger::error(L"Working layout produced invalid zones, resetting to the base layout");
            RemoveWorkingLayout();
            result.atBase = true;
            result.snaps.clear();
            result.clamped = false;
            result.clampedFrame = currentFrame;
        }

        if (reposition)
        {
            RepositionSnappedWindows(skipWindow, &previousZones);
        }
    }

    result.snapped = !result.snaps.empty();
    result.snappedOwnLine = result.snapped && std::all_of(result.snaps.begin(), result.snaps.end(), [](const EdgeSnap& snap) { return snap.ownLine; });
    return result;
}

void WorkArea::FinishEdgeResize(const ZoneIndexSet& zones, const RECT& finalFrame, const FancyZonesZoneRects::MovedEdges& moved, HWND draggedWindow)
{
    if (!m_layout)
    {
        return;
    }

    // Release rule: every dragged inner edge ends either exactly where the user released it, or exactly on an
    // original gridline (magnet: the nearest one within SnapRadiusPx, any line on that axis), or at a clamp limit
    // (MinSizePx for the zones on both sides of the wall, never below a zone's current/base size). finalFrame is the
    // frame read at MOVESIZEEND, not the last (throttled) live frame, so the snap and the clamp are decided on the
    // release position itself. With spacing, "on a gridline" means the zone boundary is the gridline (the centre of
    // the gap drawn by the cue): the window edge is spacing / 2 away from it, exactly like the template zones.
    // Single pass: update the model without moving windows, drop an unmodified entry, then move only the windows
    // whose zones changed during this pass.
    const ZonesMap previousZones = ActiveZones();
    std::optional<FancyZonesZoneRects::UnitRect> modelBefore;
    if (m_baseRects)
    {
        if (const auto* before = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects))
        {
            modelBefore = before->BoundsOf(zones);
        }
    }

    const auto result = ApplyEdgeResizeInternal(zones, finalFrame, moved, nullptr, false);
    if (!result.dividerDragged || !m_baseRects)
    {
        // Natural FancyZones resize (outer edges or no divider): the window stays where the user left it and
        // nothing else moves.
        Logger::info("Edge resize end: window {}, zones {}, final frame {}, moved {}: no divider dragged, natural resize",
                     static_cast<void*>(draggedWindow), EdgeResizeLogText::Zones(zones), EdgeResizeLogText::Rect(finalFrame), EdgeResizeLogText::Moved(moved));
        return;
    }

    std::optional<FancyZonesZoneRects::UnitRect> modelAfter;
    if (const auto* after = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects))
    {
        modelAfter = after->BoundsOf(zones);
    }

    const auto logEnd = [&](const std::string& placement) {
        Logger::info("Edge resize end: window {}, zones {}, final frame {}, moved {}, model before {} after {} (units), sides: {}; dragged window: {}",
                     static_cast<void*>(draggedWindow), EdgeResizeLogText::Zones(zones), EdgeResizeLogText::Rect(finalFrame), EdgeResizeLogText::Moved(moved),
                     UnitBoundsText(modelBefore), UnitBoundsText(modelAfter), EdgeResizeLogText::Sides(result.sides), placement);
    };

    // The moved sides that lie on a divider (whether a side is on the work-area border is the same in the base and
    // the working rectangles: walls never reach the border).
    FancyZonesZoneRects::MovedEdges dividerSides;
    {
        using FancyZonesZoneRects::Side;
        const FancyZonesZoneRects::ZoneRectModel probe(*m_baseRects);
        dividerSides.left = moved.left && probe.HasNeighbour(zones, Side::Left);
        dividerSides.top = moved.top && probe.HasNeighbour(zones, Side::Top);
        dividerSides.right = moved.right && probe.HasNeighbour(zones, Side::Right);
        dividerSides.bottom = moved.bottom && probe.HasNeighbour(zones, Side::Bottom);
    }

    const auto* model = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects);
    if (model && !model->IsModified())
    {
        RemoveWorkingLayout();
    }

    if (result.outerEdgeMoved)
    {
        // An outer edge stays where the user put it: the dragged window is not repositioned.
        logEnd("kept at the release frame (an outer edge moved)");
        RepositionSnappedWindows(draggedWindow, &previousZones);
        return;
    }

    // Only inner edges moved. The dragged window is placed into its zones only when a dragged edge ends up more than
    // 1 px away from where the user released it (it snapped to the original line or was clamped to the minimum
    // size); otherwise the release position is kept as it is.
    std::string placement = "kept at the release frame";
    if (draggedWindow)
    {
        const auto windowZones = m_layoutWindows.GetZoneIndexSetFromWindow(draggedWindow);
        if (!windowZones.empty())
        {
            RECT zoneRect = Layout::CombinedRect(ActiveZones(), windowZones);
            OffsetRect(&zoneRect, m_workAreaRect.left(), m_workAreaRect.top());
            if (ReleasedEdgeNeedsPlacement(zoneRect, finalFrame, dividerSides))
            {
                placement = std::string("placed into its zones at ") + EdgeResizeLogText::Rect(zoneRect) +
                            (result.clamped ? " (clamped)" : result.snapped ? " (snapped)" : " (zone differs from the release frame)");
                RepositionWindow(draggedWindow, windowZones);
            }
        }
        else
        {
            placement = "not snapped any more: not moved";
        }
    }

    logEnd(placement);
    RepositionSnappedWindows(draggedWindow, &previousZones);
}

bool WorkArea::ReleasedEdgeNeedsPlacement(const RECT& zoneRect, const RECT& frame, const FancyZonesZoneRects::MovedEdges& sides) noexcept
{
    constexpr long tolerancePx = 1;
    const auto differs = [](long a, long b) noexcept { return (a > b ? a - b : b - a) > tolerancePx; };
    return (sides.left && differs(zoneRect.left, frame.left)) ||
           (sides.top && differs(zoneRect.top, frame.top)) ||
           (sides.right && differs(zoneRect.right, frame.right)) ||
           (sides.bottom && differs(zoneRect.bottom, frame.bottom));
}

void WorkArea::DiscardUnmodifiedWorkingLayout()
{
    if (m_layout && m_baseRects)
    {
        const auto* model = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects);
        if (model && !model->IsModified())
        {
            const ZonesMap previousZones = ActiveZones();
            RemoveWorkingLayout();
            RepositionSnappedWindows(nullptr, &previousZones);
        }
    }
}

void WorkArea::PrepareUserSnap(HWND window, const ZoneIndexSet& zones)
{
    if (!m_layout || !m_baseRects || zones.empty())
    {
        return;
    }

    auto* model = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects);
    if (!model)
    {
        return;
    }

    // The other snapped windows, each one as a unit (a window spanning several zones conflicts as a whole). The
    // window being snapped does not occupy its old zones any more: it never conflicts with itself.
    std::vector<ZoneIndexSet> windows;
    for (const auto& [snapped, windowZones] : m_layoutWindows.SnappedWindows())
    {
        if (snapped != window && !windowZones.empty())
        {
            windows.push_back(windowZones);
        }
    }

    const FancyZonesZoneRects::OccupiedFn occupied = [&windows](ZoneIndex id) {
        return std::any_of(windows.begin(), windows.end(), [id](const ZoneIndexSet& windowZones) {
            return std::find(windowZones.begin(), windowZones.end(), id) != windowZones.end();
        });
    };

    const ZonesMap previousZones = ActiveZones();
    const auto changed = model->ResolveSnap(zones, occupied, windows);
    Logger::info("User snap: window {}, zones {}: zones reset to their original rects: {}",
                 static_cast<void*>(window), EdgeResizeLogText::Zones(zones), EdgeResizeLogText::Zones(changed));

    if (!model->IsModified())
    {
        // Back to the base layout: drop the entry (as FinishEdgeResize does).
        RemoveWorkingLayout();
    }
    else if (!changed.empty())
    {
        auto working = Layout::ZonesFromRects(m_workAreaRect, model->Current(), m_layout->Spacing(), m_layout->Zones().size());
        if (working)
        {
            m_workingZones = std::move(*working);
        }
        else
        {
            // Should not happen (base rects are valid zones and the others are unchanged); fall back to the base layout.
            Logger::error(L"Working layout produced invalid zones on a user snap, resetting to the base layout");
            RemoveWorkingLayout();
        }
    }
    else
    {
        return;
    }

    // Only the windows whose zones changed move; window is placed by the caller (Snap).
    RepositionSnappedWindows(window, &previousZones);
}

const ZonesMap& WorkArea::ActiveZones() const noexcept
{
    if (!m_workingZones.empty())
    {
        return m_workingZones;
    }

    return m_layout ? m_layout->Zones() : m_baseZones;
}

void WorkArea::RemoveWorkingLayout()
{
    WorkingLayouts::instance().Remove(m_uniqueId);
    m_workingZones.clear();
}

std::vector<ResizeGuideLine> WorkArea::ResizeGuideLines(const FancyZonesZoneRects::UnitRects& base, long width, long height)
{
    using FancyZonesZoneRects::TotalUnits;

    // floor(length * units / TotalUnits) for units in [0, TotalUnits]: the pixel maths of Layout::ZonesFromRects and
    // CalculateGridZones, so a line is exactly where the base zones put the boundary.
    const auto toPixels = [](long length, int units) {
        return static_cast<long>(static_cast<long long>(length) * units / TotalUnits);
    };

    std::vector<ResizeGuideLine> lines;
    for (const auto& line : FancyZonesZoneRects::InnerGridLines(base))
    {
        const long across = line.vertical ? width : height; // axis of the position
        const long along = line.vertical ? height : width; // axis of the segment
        lines.push_back(ResizeGuideLine{
            .vertical = line.vertical,
            .unitPosition = line.position,
            .position = toPixels(across, line.position),
            .start = toPixels(along, line.start),
            .end = toPixels(along, line.end),
        });
    }

    return lines;
}

void WorkArea::ShowResizeCue(HWND draggedWindow, const std::vector<FancyZonesZoneRects::EdgeSnap>& snaps)
{
    if (!m_layout || !m_baseRects || !m_zonesOverlay)
    {
        return;
    }

    constexpr COLORREF gridlineColor = RGB(90, 160, 255); // blue: original gridline
    constexpr COLORREF ownLineColor = RGB(102, 204, 102); // green: snapped to the dragged edge's own original line
    constexpr COLORREF otherLineColor = RGB(255, 255, 255); // white: snapped to another original line

    // Plain lines first, highlighted lines on top (they cross the plain ones).
    std::vector<ZonesOverlay::GuideLine> plain;
    std::vector<ZonesOverlay::GuideLine> highlighted;
    for (const auto& line : ResizeGuideLines(*m_baseRects, m_workAreaRect.width(), m_workAreaRect.height()))
    {
        const auto snap = std::find_if(snaps.begin(), snaps.end(), [&line](const FancyZonesZoneRects::EdgeSnap& s) {
            return s.vertical == line.vertical && s.position == line.unitPosition;
        });

        const ZonesOverlay::GuideLine guide{
            .vertical = line.vertical,
            .position = line.position,
            .start = line.start,
            .end = line.end,
            .color = snap == snaps.end() ? gridlineColor : (snap->ownLine ? ownLineColor : otherLineColor),
        };
        (snap == snaps.end() ? plain : highlighted).push_back(guide);
    }

    plain.insert(plain.end(), highlighted.begin(), highlighted.end());

    SetWorkAreaWindowAsTopmost(draggedWindow);
    m_zonesOverlay->DrawGuideLines(plain);
    m_zonesOverlay->Show();
}

#pragma region private

bool WorkArea::InitWindow(HINSTANCE hinstance)
{
    m_window = windowPool.NewZonesOverlayWindow(m_workAreaRect, hinstance, this);
    if (!m_window)
    {
        Logger::error(L"No work area window");
        return false;
    }

    m_zonesOverlay = std::make_unique<ZonesOverlay>(m_window);
    return true;
}

void WorkArea::InitLayout(const FancyZonesDataTypes::WorkAreaId& parentUniqueId)
{
    Logger::info(L"Initialize layout on {}, work area rect = {}x{}", m_uniqueId.toString(), m_workAreaRect.width(), m_workAreaRect.height());

    const bool isLayoutAlreadyApplied = AppliedLayouts::instance().IsLayoutApplied(m_uniqueId);
    if (!isLayoutAlreadyApplied)
    {
        if (!AppliedLayouts::instance().CloneLayout(parentUniqueId, m_uniqueId))
        {
            AppliedLayouts::instance().ApplyDefaultLayout(m_uniqueId);
        }

        AppliedLayouts::instance().SaveData();
    }

    CalculateZoneSet();
}

void WorkArea::InitSnappedWindows()
{
    static bool updatePositionOnceOnStartFlag = true;
    Logger::info(L"Init work area {} windows, update positions = {}", m_uniqueId.toString(), updatePositionOnceOnStartFlag);

    for (const auto& window : VirtualDesktop::instance().GetWindowsFromCurrentDesktop())
    {
        auto indexes = FancyZonesWindowProperties::RetrieveZoneIndexProperty(window);
        if (indexes.size() == 0)
        {
            continue;
        }

        if (!m_uniqueId.monitorId.monitor) // one work area across monitors
        {
            Snap(window, indexes, updatePositionOnceOnStartFlag);
        }
        else
        {
            const auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
            if (monitor && m_uniqueId.monitorId.monitor == monitor)
            {
                // prioritize snapping on the current monitor if the window was snapped to several work areas
                Snap(window, indexes, updatePositionOnceOnStartFlag);
            }
            else
            {
                // if the window is not snapped on the current monitor, then check the others
                auto savedIndexes = AppZoneHistory::instance().GetAppLastZoneIndexSet(window, m_uniqueId, GetLayoutId());
                if (savedIndexes == indexes)
                {
                    Snap(window, indexes, updatePositionOnceOnStartFlag);
                }
            }
        }
    }

    updatePositionOnceOnStartFlag = false;
}

void WorkArea::CalculateZoneSet()
{
    m_workingZones.clear();
    auto appliedLayout = AppliedLayouts::instance().GetDeviceLayout(m_uniqueId);
    if (!appliedLayout.has_value())
    {
        Logger::error(L"Layout wasn't applied. Can't init layout on work area {}x{}", m_workAreaRect.width(), m_workAreaRect.height());
        return;
    }

    // For custom layouts the spacing, sensitivity radius and zone count live in the custom
    // layout definition (custom-layouts.json), while the applied-layouts.json snapshot keeps a
    // copy taken at apply time. Editing those properties only rewrites custom-layouts.json, so
    // without this sync the snapshot stays stale and the edits don't take effect until the layout
    // is re-applied (see GH #44058). Re-derive the scalar properties from the current custom
    // layout using the same logic the apply path uses (CustomLayouts::GetLayout also derives the
    // canvas zone count from the zone list, keeping it consistent with Layout::Init validation).
    if (appliedLayout->type == FancyZonesDataTypes::ZoneSetLayoutType::Custom)
    {
        if (const auto refreshed = CustomLayouts::instance().GetLayout(appliedLayout->uuid))
        {
            appliedLayout = refreshed;
        }
    }

    // Base (template) zones first: they are what the resize cue draws, independent of any working layout.
    m_layout = std::make_unique<Layout>(appliedLayout.value());
    m_layout->Init(m_workAreaRect, m_uniqueId.monitorId.monitor);
    m_baseZones = m_layout->Zones();
    m_baseRects.reset();
    if (const auto grid = LayoutConfigurator::GridInfo(appliedLayout.value(), m_workAreaRect))
    {
        // From the grid description (Columns/Rows keep their template-derived percents).
        m_baseRects = FancyZonesZoneRects::RectsFromGrid(*grid);
    }

    // A working (resized) layout for this work area positions the windows; the layout keeps the template zones.
    if (m_baseRects)
    {
        const auto* working = WorkingLayouts::instance().Find(m_uniqueId, m_layout->Id(), *m_baseRects);
        if (working)
        {
            auto zones = Layout::ZonesFromRects(m_workAreaRect, working->Current(), m_layout->Spacing(), m_layout->Zones().size());
            if (zones)
            {
                m_workingZones = std::move(*zones);
            }
            else
            {
                Logger::error(L"Working layout does not fit the work area, dropping it");
                WorkingLayouts::instance().Remove(m_uniqueId);
            }
        }
    }
}

LRESULT WorkArea::WndProc(UINT message, WPARAM wparam, LPARAM lparam) noexcept
{
    switch (message)
    {
    case WM_NCDESTROY:
    {
        ::DefWindowProc(m_window, message, wparam, lparam);
        SetWindowLongPtr(m_window, GWLP_USERDATA, 0);
    }
    break;

    case WM_ERASEBKGND:
        return 1;

    default:
    {
        return DefWindowProc(m_window, message, wparam, lparam);
    }
    }
    return 0;
}

void WorkArea::SetWorkAreaWindowAsTopmost(HWND draggedWindow) noexcept
{
    if (!m_window)
    {
        return;
    }

    HWND windowInsertAfter = draggedWindow ? draggedWindow : HWND_TOPMOST;

    constexpr UINT flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
    SetWindowPos(m_window, windowInsertAfter, 0, 0, 0, 0, flags);
}

#pragma endregion

LRESULT CALLBACK WorkArea::s_WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
{
    auto thisRef = reinterpret_cast<WorkArea*>(GetWindowLongPtr(window, GWLP_USERDATA));
    if ((thisRef == nullptr) && (message == WM_CREATE))
    {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
        thisRef = static_cast<WorkArea*>(createStruct->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(thisRef));
    }

    return (thisRef != nullptr) ? thisRef->WndProc(message, wparam, lparam) :
                                  DefWindowProc(window, message, wparam, lparam);
}
