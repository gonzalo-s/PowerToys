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

    std::vector<int> InnerCoordinates(const UnitRects& rects, bool vertical)
    {
        std::set<int> coordinates;
        for (const auto& entry : rects)
        {
            const UnitRect& rect = entry.second;
            for (const int value : { Low(rect, vertical), High(rect, vertical) })
            {
                if (value > 0 && value < TotalUnits)
                {
                    coordinates.insert(value);
                }
            }
        }

        return std::vector<int>(coordinates.begin(), coordinates.end());
    }

    std::vector<GridLine> InnerGridLines(const UnitRects& rects)
    {
        std::vector<GridLine> lines;
        for (const bool vertical : { true, false })
        {
            // position -> extents along the line of every inner side lying on it
            std::map<int, std::vector<std::pair<int, int>>> sides;
            for (const auto& entry : rects)
            {
                const UnitRect& rect = entry.second;
                const int start = vertical ? rect.top : rect.left;
                const int end = vertical ? rect.bottom : rect.right;
                for (const int value : { Low(rect, vertical), High(rect, vertical) })
                {
                    if (value > 0 && value < TotalUnits && end > start)
                    {
                        sides[value].emplace_back(start, end);
                    }
                }
            }

            for (auto& [position, extents] : sides)
            {
                std::sort(extents.begin(), extents.end());
                std::optional<GridLine> current;
                for (const auto& [start, end] : extents)
                {
                    if (current && start <= current->end)
                    {
                        current->end = (std::max)(current->end, end); // overlapping or touching: same segment
                    }
                    else
                    {
                        if (current)
                        {
                            lines.push_back(*current);
                        }

                        current = GridLine{ vertical, position, start, end };
                    }
                }

                if (current)
                {
                    lines.push_back(*current);
                }
            }
        }

        return lines;
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

    MoveOutcome ZoneRectModel::MoveEdge(const ZoneIndexSet& zones, Side side, int position, int minSize, int snapRadius, const OccupiedFn& occupied)
    {
        const auto bounds = BoundsOf(zones);
        const auto baseBounds = BaseBoundsOf(zones);
        if (!bounds || !baseBounds)
        {
            return { MoveResult::Unchanged };
        }

        const int wall = SideOf(*bounds, side);
        if (OnBorder(wall, side))
        {
            return { MoveResult::Outer };
        }

        const bool vertical = IsVertical(side);
        const bool high = IsHighSide(side);
        const int outward = high ? 1 : -1; // direction in which the dragged side grows its block

        // near edge: the side of a rect facing the same way as the dragged side; far edge: the opposite side.
        const auto nearEdge = [&](const UnitRect& rect) { return high ? High(rect, vertical) : Low(rect, vertical); };
        const auto farEdge = [&](const UnitRect& rect) { return high ? Low(rect, vertical) : High(rect, vertical); };
        // a lies strictly beyond b in the outward direction
        const auto beyond = [outward](int a, int b) { return outward * (a - b) > 0; };
        const auto isDragged = [&zones](ZoneIndex id) { return std::find(zones.begin(), zones.end(), id) != zones.end(); };
        // Empty zones (no window) are transparent: never pushed, never pulled, never clamping, left untouched.
        const auto isOccupied = [&](ZoneIndex id) { return isDragged(id) || !occupied || occupied(id); };
        const auto touches = [&](const std::set<ZoneIndex>& group, const UnitRect& rect) {
            return std::any_of(group.begin(), group.end(), [&](ZoneIndex id) { return OverlapsAlongWall(m_current.at(id), rect, vertical); });
        };

        // The dragged block's zones whose side lies on the wall.
        std::set<ZoneIndex> dragged;
        for (ZoneIndex id : zones)
        {
            if (nearEdge(m_current.at(id)) == wall)
            {
                dragged.insert(id);
            }
        }

        // Zones moving with the wall for a target t: a = near edge moves to t, b = far edge moves to t.
        struct Groups
        {
            std::set<ZoneIndex> a;
            std::set<ZoneIndex> b;
        };

        const auto groupsFor = [&](int t) {
            Groups groups{ dragged, {} };
            bool grew = true;
            while (grew)
            {
                grew = false;
                for (const auto& [id, rect] : m_current)
                {
                    if (groups.a.contains(id) || groups.b.contains(id) || !isOccupied(id))
                    {
                        continue;
                    }

                    if (beyond(t, wall))
                    {
                        // Expand (push): an occupied zone lying beyond the old near edge of a moving zone, overlapping it
                        // along the wall and starting before t, is pushed (its far edge moves to t).
                        const bool pushed = !isDragged(id) && beyond(t, farEdge(rect)) &&
                                            std::any_of(groups.a.begin(), groups.a.end(), [&](ZoneIndex m) {
                                                const UnitRect& moving = m_current.at(m);
                                                return !beyond(nearEdge(moving), farEdge(rect)) && OverlapsAlongWall(moving, rect, vertical);
                                            });
                        // A zone whose near edge is the old far edge of a pushed zone (and overlaps it) follows the wall.
                        const bool follows = !pushed && std::any_of(groups.b.begin(), groups.b.end(), [&](ZoneIndex p) {
                                                 const UnitRect& pushedRect = m_current.at(p);
                                                 return nearEdge(rect) == farEdge(pushedRect) && OverlapsAlongWall(pushedRect, rect, vertical);
                                             });
                        if (pushed || follows)
                        {
                            (pushed ? groups.b : groups.a).insert(id);
                            grew = true;
                        }
                    }
                    else if (beyond(wall, t))
                    {
                        // Contract: the occupied zones on the other side of the wall that touch the moving zones follow
                        // (closure: a zone spanning several zones on the other side pulls all of them in).
                        if (farEdge(rect) == wall && !isDragged(id) && touches(groups.a, rect))
                        {
                            groups.b.insert(id);
                            grew = true;
                        }
                        else if (nearEdge(rect) == wall && touches(groups.b, rect))
                        {
                            groups.a.insert(id);
                            grew = true;
                        }
                    }
                }
            }

            return groups;
        };

        // Magnet: the nearest original gridline on this axis within snapRadius (before the clamp, which still wins).
        // Tie: the block's own base side position, then the lower coordinate (candidates are sorted ascending).
        const int basePosition = SideOf(*baseBounds, side);
        std::optional<int> snapLine;
        if (snapRadius >= 0)
        {
            for (const int candidate : InnerCoordinates(m_base, vertical))
            {
                const int distance = std::abs(position - candidate);
                if (distance > snapRadius)
                {
                    continue;
                }

                const int best = snapLine ? std::abs(position - *snapLine) : (std::numeric_limits<int>::max)();
                if (distance < best || (distance == best && candidate == basePosition))
                {
                    snapLine = candidate;
                }
            }
        }

        const int requested = snapLine.value_or(position);
        int target = requested;

        // Per-zone minimum: a zone already smaller than minSize may grow but never shrinks below the smaller of its
        // current and its base size, so it does not force the wall to jump and can return to its base size.
        const int effectiveMin = (std::max)(minSize, 0);
        const auto minFor = [&](ZoneIndex id) {
            const int current = High(m_current.at(id), vertical) - Low(m_current.at(id), vertical);
            const int base = High(m_base.at(id), vertical) - Low(m_base.at(id), vertical);
            return (std::min)({ effectiveMin, current, base });
        };

        Groups groups;
        if (beyond(target, wall))
        {
            // Expand. The work-area border is the limit (0 / TotalUnits): no minimum is kept free beyond the dragged edge,
            // so the window can reach the screen border. Pushed zones keep their minimum below.
            const int borderLimit = high ? TotalUnits : 0;
            if (beyond(target, borderLimit))
            {
                target = borderLimit;
            }

            // Every pushed zone keeps its minimum: t may not pass its near edge minus that minimum. The pushed set only
            // shrinks when t moves back toward the wall, so iterating to the fixpoint gives the farthest feasible t.
            while (true)
            {
                groups = groupsFor(target);
                int limit = target;
                for (ZoneIndex id : groups.b)
                {
                    const int bound = nearEdge(m_current.at(id)) - outward * minFor(id);
                    if (beyond(limit, bound))
                    {
                        limit = bound;
                    }
                }

                if (limit == target)
                {
                    break;
                }

                target = limit;
            }
        }
        else if (beyond(wall, target))
        {
            // Contract: every zone whose near edge moves (the dragged zones and the zones pulled in) keeps its minimum.
            groups = groupsFor(target);
            for (ZoneIndex id : groups.a)
            {
                const int bound = farEdge(m_current.at(id)) + outward * minFor(id);
                if (beyond(bound, target))
                {
                    target = bound;
                }
            }

            if (beyond(target, wall))
            {
                target = wall; // defensive only: the current wall position always satisfies the minimum
            }
        }

        MoveOutcome outcome;
        if (snapLine && target == *snapLine)
        {
            // only reported when the clamp kept the snapped line
            outcome.snapped = true;
            outcome.snappedPosition = *snapLine;
            outcome.ownBaseLine = *snapLine == basePosition;
        }

        // Clamped: the side stopped at a limit that the requested (raw) position lies beyond, and no snap inside the
        // limit took it there.
        outcome.clamped = target != requested && position != target && ((position > target) == (requested > target));
        outcome.position = target;

        if (target == wall)
        {
            outcome.result = MoveResult::Unchanged;
            return outcome;
        }

        // the groups of the final target (contract groups do not depend on t)
        groups = groupsFor(target);
        for (ZoneIndex id : groups.a)
        {
            UnitRect& rect = m_current.at(id);
            (high ? High(rect, vertical) : Low(rect, vertical)) = target;
        }
        for (ZoneIndex id : groups.b)
        {
            UnitRect& rect = m_current.at(id);
            (high ? Low(rect, vertical) : High(rect, vertical)) = target;
        }

        outcome.result = MoveResult::Moved;
        return outcome;
    }
}
