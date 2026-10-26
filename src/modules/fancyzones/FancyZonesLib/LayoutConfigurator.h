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
