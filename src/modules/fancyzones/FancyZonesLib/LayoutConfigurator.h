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
    // For Columns/Rows the percents are derived from the template geometry on this work area (including spacing),
    // so GridZones(workArea, info, spacing) reproduces the template zones within 1 px. The result therefore
    // depends on the work-area size for these two types; WorkingLayouts::Find drops an entry whose base no longer
    // matches (e.g. after a resolution/DPI change), which is accepted.
    static std::optional<FancyZonesDataTypes::GridLayoutInfo> GridInfo(const LayoutData& data, const FancyZonesUtils::Rect& workArea) noexcept;
};
