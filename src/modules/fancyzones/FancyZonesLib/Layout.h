#pragma once

#include <FancyZonesLib/FancyZonesData/LayoutData.h>
#include <FancyZonesLib/util.h>

#include <FancyZonesLib/LayoutConfigurator.h> // ZonesMap
#include <FancyZonesLib/ZoneRectModel.h>

class Layout
{
public:
    Layout(const LayoutData& data);
    ~Layout() = default;

    bool Init(const FancyZonesUtils::Rect& workAreaRect, HMONITOR monitor) noexcept;
    // Rebuilds the zones from zone rectangles in units of 1/10000 of the work area (used for working layouts), keeping
    // this layout's spacing. Returns false and leaves the zones untouched on an empty work area, a zone-count mismatch
    // or an invalid zone.
    bool InitFromRects(const FancyZonesUtils::Rect& workAreaRect, const FancyZonesZoneRects::UnitRects& rects) noexcept;
    // Zones of the given zone rectangles (units of 1/10000 of the work area) with the grid pixel maths of
    // LayoutConfigurator (floor, spacing inset of spacing/2 between zones and spacing at the work-area border).
    // nullopt on an empty work area, a zone-count mismatch or an invalid zone.
    static std::optional<ZonesMap> ZonesFromRects(const FancyZonesUtils::Rect& workArea, const FancyZonesZoneRects::UnitRects& rects, int spacing, size_t expectedZoneCount) noexcept;
    // Bounding rect of the given zones of any zone map; unknown ids are skipped, an empty rect when none is known.
    static RECT CombinedRect(const ZonesMap& zones, const ZoneIndexSet& indices) noexcept;
    int Spacing() const noexcept;

    GUID Id() const noexcept;
    FancyZonesDataTypes::ZoneSetLayoutType Type() const noexcept;

    const ZonesMap& Zones() const noexcept;
    ZoneIndexSet ZonesFromPoint(POINT pt) const noexcept;
    /**
     * Returns all zones spanned by the minimum bounding rectangle containing the two given zone index sets.
     */
    ZoneIndexSet GetCombinedZoneRange(const ZoneIndexSet& initialZones, const ZoneIndexSet& finalZones) const noexcept; 

    RECT GetCombinedZonesRect(const ZoneIndexSet& zones);

private:
    const LayoutData m_data;
    ZonesMap m_zones{};
};
