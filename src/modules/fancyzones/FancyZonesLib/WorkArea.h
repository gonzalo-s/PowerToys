#pragma once

#include <optional>
#include <vector>

#include <FancyZonesLib/Layout.h>
#include <FancyZonesLib/LayoutAssignedWindows.h>
#include <FancyZonesLib/ZoneRectModel.h>

class ZonesOverlay;

struct EdgeResizeResult
{
    bool changed = false; // a zone wall moved and the layout was rebuilt
    bool atBase = true; // every inner side of the dragged block touched by this resize is at its base position
    bool dividerDragged = false; // at least one moved edge is an inner edge (it has a neighbouring zone)
    bool outerEdgeMoved = false; // a moved edge lies on the work-area border: it stays where the user put it
};

class WorkArea
{
    WorkArea(HINSTANCE hinstance, const FancyZonesDataTypes::WorkAreaId& uniqueId, const FancyZonesUtils::Rect& workAreaRect);

public:
    ~WorkArea();

    static std::unique_ptr<WorkArea> Create(HINSTANCE hinstance, const FancyZonesDataTypes::WorkAreaId& uniqueId, const FancyZonesDataTypes::WorkAreaId& parentUniqueId, const FancyZonesUtils::Rect& workAreaRect)
    {
        auto self = std::unique_ptr<WorkArea>(new WorkArea(hinstance, uniqueId, workAreaRect));
        if (!self->Init(hinstance, parentUniqueId))
        {
            return nullptr;
        }

        return self;
    }

    inline bool Init([[maybe_unused]] HINSTANCE hinstance, const FancyZonesDataTypes::WorkAreaId& parentUniqueId)
    {
#ifndef UNIT_TESTS
        if (!InitWindow(hinstance))
        {
            return false;
        }
#endif
        InitLayout(parentUniqueId);
        
        return true;
    }
    
    FancyZonesDataTypes::WorkAreaId UniqueId() const noexcept { return { m_uniqueId }; }
    const std::unique_ptr<Layout>& GetLayout() const noexcept { return m_layout; }
    const LayoutAssignedWindows& GetLayoutWindows() const noexcept { return m_layoutWindows; }
    const HWND GetWorkAreaWindow() const noexcept { return m_window; }
    const GUID GetLayoutId() const noexcept;
    const FancyZonesUtils::Rect& GetWorkAreaRect() const noexcept { return m_workAreaRect; }
    
    void InitLayout();
    void InitSnappedWindows();
    void UpdateWindowPositions(HWND skipWindow = nullptr);

    // Dynamic grid resize. Frames are visible window frames in screen coordinates. An inner edge moves only the zone
    // wall segment it touches (the zones on both sides of it). Edges on the work-area border keep the natural
    // FancyZones behaviour: no entry, no other window moves.
    // FinishEdgeResize repositions only windows whose zones changed; the dragged window is placed into its zones
    // only when every moved edge was an inner edge, and nothing is repositioned when no inner edge was dragged.
    bool SupportsEdgeResize() const noexcept { return m_baseRects.has_value(); }
    EdgeResizeResult ApplyEdgeResize(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesZoneRects::MovedEdges& moved, HWND skipWindow);
    void FinishEdgeResize(const ZoneIndexSet& zones, const RECT& finalFrame, const FancyZonesZoneRects::MovedEdges& moved, HWND draggedWindow = nullptr);
    void DiscardUnmodifiedWorkingLayout();
    void ShowResizeCue(HWND draggedWindow, bool atBase);

    // Moves snapped windows (except skipWindow) into their zones with a single async SetWindowPos each.
    // With previousZones, only windows whose combined zone rect differs from previousZones are moved.
    // Skips hidden, minimized, maximized and cloaked windows. Safe for the live resize path.
    void RepositionSnappedWindows(HWND skipWindow = nullptr, const ZonesMap* previousZones = nullptr);

    // True when the combined rect of zones differs between the two zone maps (or a zone is missing from either).
    static bool ZonesRectChanged(const ZonesMap& before, const ZonesMap& after, const ZoneIndexSet& zones);

    bool Snap(HWND window, const ZoneIndexSet& zones, bool updatePosition = true);
    bool Unsnap(HWND window);

    void ShowZones(const ZoneIndexSet& highlight, HWND draggedWindow = nullptr);
    void HideZones();
    void FlashZones();
    void ShowLayoutNameLabel(const std::wstring& name);
    void ShowMonitorRotationPreview(const std::vector<RECT>& windowRects, size_t monitorNumber, std::optional<bool> reverse, bool animateRotation);
    
    void CycleWindows(HWND window, bool reverse);

protected:
    static LRESULT CALLBACK s_WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

private:
    bool InitWindow(HINSTANCE hinstance);
    void RepositionWindow(HWND window, const ZoneIndexSet& zones);
    EdgeResizeResult ApplyEdgeResizeInternal(const ZoneIndexSet& zones, const RECT& currentFrame, const FancyZonesZoneRects::MovedEdges& moved, HWND skipWindow, bool reposition);
    void InitLayout(const FancyZonesDataTypes::WorkAreaId& parentUniqueId);
    
    void CalculateZoneSet();
    void SetWorkAreaWindowAsTopmost(HWND draggedWindow) noexcept;

    LRESULT WndProc(UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    
    const FancyZonesUtils::Rect m_workAreaRect{};
    const FancyZonesDataTypes::WorkAreaId m_uniqueId;
    HWND m_window{}; // Hidden tool window used to represent current monitor desktop work area.
    std::unique_ptr<Layout> m_layout;
    std::optional<FancyZonesZoneRects::UnitRects> m_baseRects; // zone rectangles of the applied layout, set when it is grid-based
    ZonesMap m_baseZones; // template zones of the applied layout, before any working-layout override (drawn by the resize cue)
    LayoutAssignedWindows m_layoutWindows{};
    std::unique_ptr<ZonesOverlay> m_zonesOverlay;
};
