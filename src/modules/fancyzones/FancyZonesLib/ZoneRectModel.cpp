#include "pch.h"
#include "ZoneRectModel.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>

using FancyZonesDataTypes::GridLayoutInfo;

namespace FancyZonesZoneRects
{
    namespace
    {
        constexpr bool IsVertical(Side side) noexcept
        {
            return side == Side::Left || side == Side::Right;
        }

        // Right/Bottom: the side is the high end of its axis.
        constexpr bool IsHighSide(Side side) noexcept
        {
            return side == Side::Right || side == Side::Bottom;
        }

        // Low/high end of a rect on the axis the side moves along (x for vertical sides, y for horizontal sides).
        constexpr int& Low(UnitRect& rect, bool vertical) noexcept
        {
            return vertical ? rect.left : rect.top;
        }

        constexpr int& High(UnitRect& rect, bool vertical) noexcept
        {
            return vertical ? rect.right : rect.bottom;
        }

        constexpr int Low(const UnitRect& rect, bool vertical) noexcept
        {
            return vertical ? rect.left : rect.top;
        }

        constexpr int High(const UnitRect& rect, bool vertical) noexcept
        {
            return vertical ? rect.right : rect.bottom;
        }

        // Positive-length overlap of the extents along the wall (y for vertical sides, x for horizontal sides).
        constexpr bool OverlapsAlongWall(const UnitRect& a, const UnitRect& b, bool vertical) noexcept
        {
            const int start = vertical ? (std::max)(a.top, b.top) : (std::max)(a.left, b.left);
            const int end = vertical ? (std::min)(a.bottom, b.bottom) : (std::min)(a.right, b.right);
            return end > start;
        }

        constexpr int SideOf(const UnitRect& rect, Side side) noexcept
        {
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
        }

        constexpr bool OnBorder(int position, Side side) noexcept
        {
            return IsHighSide(side) ? position >= TotalUnits : position <= 0;
        }

        std::optional<UnitRect> Bounds(const UnitRects& rects, const ZoneIndexSet& zones)
        {
            std::optional<UnitRect> bounds;
            for (ZoneIndex id : zones)
            {
                const auto it = rects.find(id);
                if (it == rects.end())
                {
                    return std::nullopt;
                }

                const UnitRect& rect = it->second;
                if (!bounds)
                {
                    bounds = rect;
                }
                else
                {
                    bounds->left = (std::min)(bounds->left, rect.left);
                    bounds->top = (std::min)(bounds->top, rect.top);
                    bounds->right = (std::max)(bounds->right, rect.right);
                    bounds->bottom = (std::max)(bounds->bottom, rect.bottom);
                }
            }

            return bounds;
        }

        // Cumulative boundaries of the tracks; the last one is forced to TotalUnits.
        std::vector<int> Boundaries(const std::vector<int>& percents, int count)
        {
            const size_t tracks = count > 0 ? static_cast<size_t>(count) : 0;
            std::vector<int> boundaries(tracks + 1, 0);
            for (size_t i = 0; i < tracks; ++i)
            {
                const int percent = i < percents.size() ? percents[i] : 0;
                boundaries[i + 1] = boundaries[i] + percent;
            }

            boundaries[tracks] = TotalUnits;
            return boundaries;
        }
    }

    MovedEdges DetectMovedEdges(const RECT& before, const RECT& after, long tolerance) noexcept
    {
        MovedEdges moved;
        moved.left = std::labs(after.left - before.left) > tolerance;
        moved.top = std::labs(after.top - before.top) > tolerance;
        moved.right = std::labs(after.right - before.right) > tolerance;
        moved.bottom = std::labs(after.bottom - before.bottom) > tolerance;
        return moved;
    }

    UnitRects RectsFromGrid(const GridLayoutInfo& grid)
    {
        struct Cells
        {
            size_t minRow;
            size_t maxRow;
            size_t minCol;
            size_t maxCol;
        };

        const auto xs = Boundaries(grid.columnsPercents(), grid.columns());
        const auto ys = Boundaries(grid.rowsPercents(), grid.rows());
        const size_t rows = ys.size() - 1;
        const size_t columns = xs.size() - 1;
        const auto& map = grid.cellChildMap();

        std::map<ZoneIndex, Cells> cells;
        for (size_t row = 0; row < rows && row < map.size(); ++row)
        {
            for (size_t col = 0; col < columns && col < map[row].size(); ++col)
            {
                const ZoneIndex id = map[row][col];
                auto it = cells.find(id);
                if (it == cells.end())
                {
                    cells.emplace(id, Cells{ row, row, col, col });
                }
                else
                {
                    it->second.minRow = (std::min)(it->second.minRow, row);
                    it->second.maxRow = (std::max)(it->second.maxRow, row);
                    it->second.minCol = (std::min)(it->second.minCol, col);
                    it->second.maxCol = (std::max)(it->second.maxCol, col);
                }
            }
        }

        UnitRects rects;
        for (const auto& [id, c] : cells)
        {
            rects.emplace(id, UnitRect{ xs[c.minCol], ys[c.minRow], xs[c.maxCol + 1], ys[c.maxRow + 1] });
        }

        return rects;
    }

    ZoneRectModel::ZoneRectModel(const UnitRects& base) :
        m_base(base),
        m_current(base)
    {
    }

    bool ZoneRectModel::SameBase(const UnitRects& other) const noexcept
    {
        return m_base == other;
    }

    bool ZoneRectModel::IsModified() const noexcept
    {
        return m_current != m_base;
    }

    std::optional<UnitRect> ZoneRectModel::BoundsOf(const ZoneIndexSet& zones) const
    {
        return Bounds(m_current, zones);
    }

    std::optional<UnitRect> ZoneRectModel::BaseBoundsOf(const ZoneIndexSet& zones) const
    {
        return Bounds(m_base, zones);
    }

    bool ZoneRectModel::HasNeighbour(const ZoneIndexSet& zones, Side side) const
    {
        const auto bounds = BoundsOf(zones);
        return bounds.has_value() && !OnBorder(SideOf(*bounds, side), side);
    }

    MoveResult ZoneRectModel::MoveEdge(const ZoneIndexSet& zones, Side side, int position, int minSize, int snapRadius)
    {
        const auto bounds = BoundsOf(zones);
        const auto baseBounds = BaseBoundsOf(zones);
        if (!bounds || !baseBounds)
        {
            return MoveResult::Unchanged;
        }

        const int wall = SideOf(*bounds, side);
        if (OnBorder(wall, side))
        {
            return MoveResult::Outer;
        }

        const bool vertical = IsVertical(side);
        const bool high = IsHighSide(side);

        // A: zones whose moved side lies on the wall (the dragged block's zones on that side plus the zones pulled in);
        // B: zones on the other side of the wall whose opposite side lies on it and that touch A along the wall.
        const auto edgeA = [&](const UnitRect& rect) { return high ? High(rect, vertical) : Low(rect, vertical); };
        const auto edgeB = [&](const UnitRect& rect) { return high ? Low(rect, vertical) : High(rect, vertical); };
        const auto touches = [&](const std::set<ZoneIndex>& group, const UnitRect& rect) {
            return std::any_of(group.begin(), group.end(), [&](ZoneIndex id) { return OverlapsAlongWall(m_current.at(id), rect, vertical); });
        };
        const auto isDragged = [&zones](ZoneIndex id) { return std::find(zones.begin(), zones.end(), id) != zones.end(); };

        std::set<ZoneIndex> a;
        for (ZoneIndex id : zones)
        {
            if (edgeA(m_current.at(id)) == wall)
            {
                a.insert(id);
            }
        }

        // Grow both sides to a fixpoint: a zone spanning several zones on the other side pulls all of them in.
        std::set<ZoneIndex> b;
        bool grew = true;
        while (grew)
        {
            grew = false;
            for (const auto& [id, rect] : m_current)
            {
                if (a.contains(id) || b.contains(id))
                {
                    continue;
                }

                if (edgeB(rect) == wall && !isDragged(id) && touches(a, rect))
                {
                    b.insert(id);
                    grew = true;
                }
                else if (edgeA(rect) == wall && touches(b, rect))
                {
                    a.insert(id);
                    grew = true;
                }
            }
        }

        if (b.empty())
        {
            return MoveResult::Unchanged; // inconsistent tiling
        }

        int target = position;
        const int basePosition = SideOf(*baseBounds, side);
        if (std::abs(target - basePosition) <= snapRadius)
        {
            target = basePosition;
        }

        // Per-zone minimum: a zone already smaller than minSize may grow but never shrinks below the smaller of its
        // current and its base size, so it does not force the wall to jump and can return to its base size.
        const int effectiveMin = (std::max)(minSize, 0);
        const auto minFor = [&](ZoneIndex id) {
            const int current = High(m_current.at(id), vertical) - Low(m_current.at(id), vertical);
            const int base = High(m_base.at(id), vertical) - Low(m_base.at(id), vertical);
            return (std::min)({ effectiveMin, current, base });
        };

        // Zones below the wall (their high end is on it) bound it from below, zones above it bound it from above.
        const auto& lowSide = high ? a : b;
        const auto& highSide = high ? b : a;
        int low = (std::numeric_limits<int>::min)();
        int top = (std::numeric_limits<int>::max)();
        for (ZoneIndex id : lowSide)
        {
            low = (std::max)(low, Low(m_current.at(id), vertical) + minFor(id));
        }
        for (ZoneIndex id : highSide)
        {
            top = (std::min)(top, High(m_current.at(id), vertical) - minFor(id));
        }

        if (low > top)
        {
            return MoveResult::Unchanged; // defensive only: the current wall position always satisfies both bounds
        }

        target = std::clamp(target, low, top);
        if (target == wall)
        {
            return MoveResult::Unchanged;
        }

        for (ZoneIndex id : a)
        {
            UnitRect& rect = m_current.at(id);
            (high ? High(rect, vertical) : Low(rect, vertical)) = target;
        }
        for (ZoneIndex id : b)
        {
            UnitRect& rect = m_current.at(id);
            (high ? Low(rect, vertical) : High(rect, vertical)) = target;
        }

        return MoveResult::Moved;
    }
}
