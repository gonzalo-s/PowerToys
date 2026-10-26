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

        // Positive-area intersection: rectangles that only touch along a side or at a corner do not overlap.
        constexpr bool Overlaps(const UnitRect& a, const UnitRect& b) noexcept
        {
            return (std::min)(a.right, b.right) > (std::max)(a.left, b.left) && (std::min)(a.bottom, b.bottom) > (std::max)(a.top, b.top);
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

    MovedEdges RefusedSides(const RECT& requested, const RECT& actual, long tolerance) noexcept
    {
        MovedEdges refused;
        refused.left = actual.left < requested.left - tolerance;
        refused.top = actual.top < requested.top - tolerance;
        refused.right = actual.right > requested.right + tolerance;
        refused.bottom = actual.bottom > requested.bottom + tolerance;
        return refused;
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

    MoveOutcome ZoneRectModel::MoveEdge(const ZoneIndexSet& zones, Side side, int position, int minSize, int snapRadius, const OccupiedFn& occupied, const std::vector<ZoneIndexSet>& windows)
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

        // Units squeezed as a whole: the dragged block and every known window (a zone may belong to several).
        std::map<ZoneIndex, std::vector<size_t>> unitsOf;
        {
            size_t unit = 0;
            const auto addUnit = [&](const ZoneIndexSet& ids) {
                for (ZoneIndex id : ids)
                {
                    if (m_current.contains(id))
                    {
                        unitsOf[id].push_back(unit);
                    }
                }

                ++unit;
            };

            addUnit(zones);
            for (const auto& window : windows)
            {
                addUnit(window);
            }
        }

        const auto sharesUnit = [&unitsOf](ZoneIndex x, ZoneIndex y) {
            const auto ux = unitsOf.find(x);
            const auto uy = unitsOf.find(y);
            if (ux == unitsOf.end() || uy == unitsOf.end())
            {
                return false;
            }

            return std::any_of(ux->second.begin(), ux->second.end(), [&uy](size_t u) { return std::find(uy->second.begin(), uy->second.end(), u) != uy->second.end(); });
        };
        const auto sharesUnitWithAny = [&](const std::set<ZoneIndex>& group, ZoneIndex id) {
            return std::any_of(group.begin(), group.end(), [&](ZoneIndex other) { return other != id && sharesUnit(other, id); });
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
                        const bool pushedDirectly = !isDragged(id) && beyond(t, farEdge(rect)) &&
                                                    std::any_of(groups.a.begin(), groups.a.end(), [&](ZoneIndex m) {
                                                        const UnitRect& moving = m_current.at(m);
                                                        return !beyond(nearEdge(moving), farEdge(rect)) && OverlapsAlongWall(moving, rect, vertical);
                                                    });
                        // A zone of a pushed zone's window (unit) that extends past t, on the far side of the wall, is
                        // pushed too: the window's bounding rect never overlaps the dragged block.
                        const bool pushed = pushedDirectly ||
                                            (!isDragged(id) && beyond(t, farEdge(rect)) && !beyond(wall, farEdge(rect)) && sharesUnitWithAny(groups.b, id));
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
                        else if (nearEdge(rect) == wall && (touches(groups.b, rect) || sharesUnitWithAny(groups.a, id)))
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

        // Squeezed zones: expanding squeezes the pushed zones (b), contracting the dragged and pulled zones (a). s is the
        // direction the wall moves in; front = a squeezed zone's side moving with the wall, back = its opposite side.
        const bool expanding = beyond(requested, wall);
        const int s = expanding ? outward : -outward;
        const auto ahead = [s](int a, int b) { return s * (a - b) > 0; };
        const auto front = [&](const UnitRect& rect) { return expanding ? farEdge(rect) : nearEdge(rect); };
        const auto back = [&](const UnitRect& rect) { return expanding ? nearEdge(rect) : farEdge(rect); };
        const auto frontRef = [&](UnitRect& rect) -> int& {
            return (expanding != high) ? High(rect, vertical) : Low(rect, vertical);
        };
        const auto backRef = [&](UnitRect& rect) -> int& {
            return (expanding != high) ? Low(rect, vertical) : High(rect, vertical);
        };

        // The occupied zones right behind a squeezed zone: their front lies on its back, overlapping it along the wall.
        const auto behind = [&](ZoneIndex id) {
            const UnitRect& rect = m_current.at(id);
            std::vector<ZoneIndex> result;
            for (const auto& [other, otherRect] : m_current)
            {
                if (other != id && isOccupied(other) && !(expanding && isDragged(other)) && front(otherRect) == back(rect) &&
                    OverlapsAlongWall(otherRect, rect, vertical))
                {
                    result.push_back(other);
                }
            }

            return result;
        };

        // A squeezed zone at its minimum can push the zones behind it only when they all belong to its own unit.
        const auto compressible = [&](ZoneIndex id, const std::vector<ZoneIndex>& behindZones) {
            return !behindZones.empty() &&
                   std::all_of(behindZones.begin(), behindZones.end(), [&](ZoneIndex other) { return sharesUnit(id, other); });
        };

        // Farthest position the front of a squeezed zone can reach: its back minus its minimum, or, when it can push
        // the zones of its unit behind it, the most restrictive of their reach minus its minimum.
        std::map<ZoneIndex, int> reachMemo;
        std::function<int(ZoneIndex)> reach = [&](ZoneIndex id) -> int {
            if (const auto it = reachMemo.find(id); it != reachMemo.end())
            {
                return it->second;
            }

            const int minimum = minFor(id);
            int result = back(m_current.at(id)) - s * minimum;
            reachMemo[id] = result; // provisional: guards against cycles of zero-size zones
            const auto behindZones = behind(id);
            if (compressible(id, behindZones))
            {
                std::optional<int> limit;
                for (ZoneIndex other : behindZones)
                {
                    const int candidate = reach(other) - s * minimum;
                    if (!limit || ahead(*limit, candidate))
                    {
                        limit = candidate;
                    }
                }

                if (limit && ahead(*limit, result))
                {
                    result = *limit;
                }
            }

            reachMemo[id] = result;
            return result;
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

            // Every pushed zone keeps its minimum: t may not pass its reach (its near edge minus that minimum, farther
            // when it pushes the zones of its own window behind it). The pushed set only shrinks when t moves back
            // toward the wall, so iterating to the fixpoint gives the farthest feasible t.
            while (true)
            {
                groups = groupsFor(target);
                int limit = target;
                for (ZoneIndex id : groups.b)
                {
                    const int bound = reach(id);
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
            // Contract: every zone whose near edge moves (the dragged zones and the zones pulled in) keeps its minimum
            // (its reach: farther when it pushes the zones of its own window behind it).
            groups = groupsFor(target);
            for (ZoneIndex id : groups.a)
            {
                const int bound = reach(id);
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
        UnitRects next = m_current;

        // Growing zones: their side on the wall moves to the target.
        for (ZoneIndex id : expanding ? groups.a : groups.b)
        {
            UnitRect& rect = next.at(id);
            (expanding == high ? High(rect, vertical) : Low(rect, vertical)) = target;
        }

        // Squeezed zones: the front moves to the target; a zone that would become smaller than its minimum keeps the
        // minimum and pushes the zones of its unit right behind it (their front moves to its new back), and so on.
        // Behind zones have their front farther in the direction s, so they are visited after the zones pushing them;
        // the passes repeat until nothing changes (only matters for zero-size zones).
        std::map<ZoneIndex, int> frontAt;
        for (ZoneIndex id : expanding ? groups.b : groups.a)
        {
            frontAt[id] = target;
        }

        std::vector<ZoneIndex> order;
        for (const auto& entry : m_current)
        {
            order.push_back(entry.first);
        }

        std::stable_sort(order.begin(), order.end(), [&](ZoneIndex x, ZoneIndex y) { return ahead(front(m_current.at(y)), front(m_current.at(x))); });

        bool pushedFurther = true;
        while (pushedFurther)
        {
            pushedFurther = false;
            for (ZoneIndex id : order)
            {
                const auto it = frontAt.find(id);
                if (it == frontAt.end())
                {
                    continue;
                }

                const int newFront = it->second;
                const UnitRect& rect = m_current.at(id);
                UnitRect& out = next.at(id);
                frontRef(out) = newFront;
                const int minimum = minFor(id);
                if (s * (back(rect) - newFront) >= minimum)
                {
                    continue;
                }

                const auto behindZones = behind(id);
                if (!compressible(id, behindZones))
                {
                    continue; // not reached: the target respects this zone's reach
                }

                const int newBack = newFront + s * minimum;
                backRef(out) = newBack;
                for (ZoneIndex other : behindZones)
                {
                    const auto [pos, inserted] = frontAt.emplace(other, newBack);
                    if (!inserted && ahead(newBack, pos->second))
                    {
                        pos->second = newBack;
                        pushedFurther = true;
                    }
                    else if (inserted)
                    {
                        pushedFurther = true;
                    }
                }
            }
        }

        m_current = std::move(next);
        outcome.result = MoveResult::Moved;
        return outcome;
    }

    std::vector<ZoneIndex> ZoneRectModel::ResolveSnap(const ZoneIndexSet& zones, const OccupiedFn& occupied, const std::vector<ZoneIndexSet>& windows)
    {
        if (!BoundsOf(zones))
        {
            return {};
        }

        // Units that move together: every window's (known) zones, then every other occupied zone on its own.
        std::vector<ZoneIndexSet> units;
        std::set<ZoneIndex> inWindows;
        for (const auto& window : windows)
        {
            ZoneIndexSet known;
            for (ZoneIndex id : window)
            {
                if (m_current.contains(id))
                {
                    known.push_back(id);
                    inWindows.insert(id);
                }
            }

            if (!known.empty())
            {
                units.push_back(std::move(known));
            }
        }

        for (const auto& entry : m_current)
        {
            if (!inWindows.contains(entry.first) && (!occupied || occupied(entry.first)))
            {
                units.push_back(ZoneIndexSet{ entry.first });
            }
        }

        // Fixpoint over the current rects (nothing is reset before every conflict is known).
        std::set<ZoneIndex> reset(zones.begin(), zones.end());
        std::vector<bool> resetUnits(units.size(), false);
        const auto conflicts = [&](const ZoneIndexSet& unit) {
            for (ZoneIndex id : unit)
            {
                if (reset.contains(id))
                {
                    return true;
                }

                const UnitRect& current = m_current.at(id);
                for (ZoneIndex target : reset)
                {
                    if (Overlaps(current, m_base.at(target)))
                    {
                        return true;
                    }
                }
            }

            return false;
        };

        bool grew = true;
        while (grew)
        {
            grew = false;
            for (size_t i = 0; i < units.size(); ++i)
            {
                if (!resetUnits[i] && conflicts(units[i]))
                {
                    resetUnits[i] = true;
                    reset.insert(units[i].begin(), units[i].end());
                    grew = true;
                }
            }
        }

        std::vector<ZoneIndex> changed;
        for (ZoneIndex id : reset)
        {
            UnitRect& current = m_current.at(id);
            const UnitRect& base = m_base.at(id);
            if (current != base)
            {
                current = base;
                changed.push_back(id);
            }
        }

        return changed;
    }
}
