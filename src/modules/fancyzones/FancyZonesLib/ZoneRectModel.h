#pragma once

#include <map>
#include <optional>
#include <vector>

#include <windef.h>

#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/Zone.h>

namespace FancyZonesZoneRects
{
    // Coordinates are expressed in units of 1/10000 of the work area on each axis, like GridLayoutInfo percents.
    constexpr int TotalUnits = 10000;

    // Zone rectangle without spacing (pure tiling boundaries).
    struct UnitRect
    {
        int left;
        int top;
        int right;
        int bottom;

        bool operator==(const UnitRect& other) const noexcept = default;
    };

    using UnitRects = std::map<ZoneIndex, UnitRect>;

    enum class Side
    {
        Left,
        Top,
        Right,
        Bottom,
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

    // Zone rectangles of a grid description, from its cumulative row/column boundaries (no spacing).
    UnitRects RectsFromGrid(const FancyZonesDataTypes::GridLayoutInfo& grid);

    enum class MoveResult
    {
        Outer, // the side lies on the work-area border: nothing to move
        Unchanged,
        Moved,
    };

    struct MoveOutcome
    {
        MoveResult result = MoveResult::Unchanged;
        bool snapped = false; // the side ended exactly on an original gridline that was within the snap radius
        int snappedPosition = 0; // that gridline (units), valid when snapped
        bool ownBaseLine = false; // that gridline is the dragged block's own base side position
    };

    // An original gridline a dragged side is snapped to (magnet), as reported to the resize cue.
    struct EdgeSnap
    {
        bool vertical; // vertical gridline (a Left/Right side snapped): position is an x coordinate
        int position; // units
        bool ownLine; // the dragged side's own original line

        bool operator==(const EdgeSnap& other) const noexcept = default;
    };

    // Distinct inner coordinates of the rectangles' sides on one axis (x for vertical lines, y for horizontal lines),
    // sorted ascending; 0 and TotalUnits (work-area borders) are excluded. These are the original gridlines.
    std::vector<int> InnerCoordinates(const UnitRects& rects, bool vertical);

    // One original gridline segment: the inner sides of the rectangles at position, merged where they overlap or touch.
    struct GridLine
    {
        bool vertical; // vertical line: position is an x coordinate and start/end are y coordinates
        int position;
        int start;
        int end;

        bool operator==(const GridLine& other) const noexcept = default;
    };

    // Every inner gridline segment of the rectangles: vertical lines first, then horizontal ones, each sorted by
    // position and start.
    std::vector<GridLine> InnerGridLines(const UnitRects& rects);

    // Working copy of a grid layout as independent zone rectangles. Moving a side of a block of zones moves only the
    // wall segment that block touches (plus whatever is needed to keep the tiling consistent), not the whole gridline.
    class ZoneRectModel
    {
    public:
        explicit ZoneRectModel(const UnitRects& base);

        const UnitRects& Base() const noexcept { return m_base; }
        const UnitRects& Current() const noexcept { return m_current; }

        bool SameBase(const UnitRects& other) const noexcept;
        bool IsModified() const noexcept;

        // Bounding rect of the given zones (current / base); nullopt when a zone id is unknown or the set is empty.
        std::optional<UnitRect> BoundsOf(const ZoneIndexSet& zones) const;
        std::optional<UnitRect> BaseBoundsOf(const ZoneIndexSet& zones) const;

        // Moves one side of the block formed by zones (the dragged window's zone set) to position. Magnet: the side
        // snaps to the nearest original gridline on its axis (InnerCoordinates of the base rects) within snapRadius;
        // on a tie the block's own base side position wins, then the lower coordinate. Then it is clamped so that
        // every zone on either side of the moved wall keeps at least min(minSize, its current size, its base size)
        // (negative minSize = 0). The outcome reports the snap only when the clamped position is the snapped line.
        MoveOutcome MoveEdge(const ZoneIndexSet& zones, Side side, int position, int minSize, int snapRadius);

        // False when the side of the block lies on the work-area border (or a zone id is unknown).
        bool HasNeighbour(const ZoneIndexSet& zones, Side side) const;

    private:
        UnitRects m_base;
        UnitRects m_current;
    };
}
