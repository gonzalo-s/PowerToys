#pragma once

#include <optional>
#include <vector>

#include <windef.h>

#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/Zone.h>

namespace FancyZonesDividers
{
    // Positions and track sizes are expressed in units of 1/10000 of the work area, like GridLayoutInfo percents.
    constexpr int TotalUnits = 10000;

    enum class Axis
    {
        Column, // vertical divider, positioned on the x axis
        Row, // horizontal divider, positioned on the y axis
    };

    // Boundary between track (index - 1) and track (index) on the given axis. Valid range: 1..trackCount-1.
    struct Divider
    {
        Axis axis;
        int index;
    };

    // Inclusive range of rows/columns covered by a set of zones.
    struct Span
    {
        int firstRow;
        int lastRow;
        int firstCol;
        int lastCol;
    };

    struct MovedEdges
    {
        bool left = false;
        bool top = false;
        bool right = false;
        bool bottom = false;

        bool any() const noexcept { return left || top || right || bottom; }

        MovedEdges& operator|=(const MovedEdges& other) noexcept
        {
            left |= other.left;
            top |= other.top;
            right |= other.right;
            bottom |= other.bottom;
            return *this;
        }
    };

    MovedEdges DetectMovedEdges(const RECT& before, const RECT& after, long tolerance) noexcept;

    class DividerModel
    {
    public:
        explicit DividerModel(const FancyZonesDataTypes::GridLayoutInfo& base);

        const FancyZonesDataTypes::GridLayoutInfo& Base() const noexcept { return m_base; }
        const FancyZonesDataTypes::GridLayoutInfo& Current() const noexcept { return m_current; }

        // True when other has the same rows, columns, percents and cellChildMap as the base grid.
        bool SameBase(const FancyZonesDataTypes::GridLayoutInfo& other) const noexcept;

        // Bounding rows/columns of the given zones; nullopt when none of them is in the grid.
        std::optional<Span> SpanOf(const ZoneIndexSet& zones) const noexcept;

        // Dividers on the inner sides of span that correspond to the moved edges. Outer (work area) sides are skipped.
        std::vector<Divider> DividersFor(const Span& span, const MovedEdges& moved) const;

        int Position(Divider divider) const noexcept;
        int BasePosition(Divider divider) const noexcept;
        bool IsModified(Divider divider) const noexcept;
        bool IsModified() const noexcept;

        // Moves the whole divider line to position (clamped so both neighbouring tracks stay >= minTrack,
        // snapped to the base position when within snapRadius). Returns true when the grid changed.
        bool Move(Divider divider, int position, int minTrack, int snapRadius);

    private:
        FancyZonesDataTypes::GridLayoutInfo m_base;
        FancyZonesDataTypes::GridLayoutInfo m_current;
    };
}
