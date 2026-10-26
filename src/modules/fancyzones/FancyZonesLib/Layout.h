#pragma once

#include <FancyZonesLib/FancyZonesData/LayoutData.h>
#include <FancyZonesLib/util.h>

#include <FancyZonesLib/LayoutConfigurator.h> // ZonesMap

class Layout
{
public:
    Layout(const LayoutData& data);
    ~Layout() = default;

    bool Init(const FancyZonesUtils::Rect& workAreaRect, HMONITOR monitor) noexcept;
    // Rebuilds the zones from an explicit grid description (used for working layouts), keeping this layout's spacing.
    bool InitFromGrid(const FancyZonesUtils::Rect& workAreaRect, const FancyZonesDataTypes::GridLayoutInfo& grid) noexcept;
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
