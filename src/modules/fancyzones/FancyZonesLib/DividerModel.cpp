#include "pch.h"
#include "DividerModel.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>

using FancyZonesDataTypes::GridLayoutInfo;

namespace FancyZonesDividers
{
    namespace
    {
        const std::vector<int>& Tracks(const GridLayoutInfo& info, Axis axis) noexcept
        {
            return axis == Axis::Column ? info.columnsPercents() : info.rowsPercents();
        }

        std::vector<int>& Tracks(GridLayoutInfo& info, Axis axis) noexcept
        {
            return axis == Axis::Column ? info.columnsPercents() : info.rowsPercents();
        }

        // Sum of the first `index` tracks = position of the boundary before track `index`.
        int EdgeOf(const std::vector<int>& tracks, int index) noexcept
        {
            return std::accumulate(tracks.begin(), tracks.begin() + index, 0);
        }

        bool IsValid(const GridLayoutInfo& info, Divider divider) noexcept
        {
            const int count = static_cast<int>(Tracks(info, divider.axis).size());
            return divider.index >= 1 && divider.index < count;
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

    DividerModel::DividerModel(const GridLayoutInfo& base) :
        m_base(base),
        m_current(base)
    {
    }

    bool DividerModel::SameBase(const GridLayoutInfo& other) const noexcept
    {
        return m_base.rows() == other.rows() &&
               m_base.columns() == other.columns() &&
               m_base.rowsPercents() == other.rowsPercents() &&
               m_base.columnsPercents() == other.columnsPercents() &&
               m_base.cellChildMap() == other.cellChildMap();
    }

    std::optional<Span> DividerModel::SpanOf(const ZoneIndexSet& zones) const noexcept
    {
        std::optional<Span> span;
        const auto& map = m_base.cellChildMap();
        for (int row = 0; row < m_base.rows(); ++row)
        {
            for (int col = 0; col < m_base.columns(); ++col)
            {
                const ZoneIndex id = map[row][col];
                if (std::find(zones.begin(), zones.end(), id) == zones.end())
                {
                    continue;
                }

                if (!span)
                {
                    span = Span{ row, row, col, col };
                }
                else
                {
                    span->firstRow = (std::min)(span->firstRow, row);
                    span->lastRow = (std::max)(span->lastRow, row);
                    span->firstCol = (std::min)(span->firstCol, col);
                    span->lastCol = (std::max)(span->lastCol, col);
                }
            }
        }

        return span;
    }

    std::vector<Divider> DividerModel::DividersFor(const Span& span, const MovedEdges& moved) const
    {
        std::vector<Divider> dividers;
        if (moved.left && span.firstCol > 0)
        {
            dividers.push_back({ Axis::Column, span.firstCol });
        }
        if (moved.right && span.lastCol + 1 < m_base.columns())
        {
            dividers.push_back({ Axis::Column, span.lastCol + 1 });
        }
        if (moved.top && span.firstRow > 0)
        {
            dividers.push_back({ Axis::Row, span.firstRow });
        }
        if (moved.bottom && span.lastRow + 1 < m_base.rows())
        {
            dividers.push_back({ Axis::Row, span.lastRow + 1 });
        }

        return dividers;
    }

    int DividerModel::Position(Divider divider) const noexcept
    {
        return IsValid(m_current, divider) ? EdgeOf(Tracks(m_current, divider.axis), divider.index) : 0;
    }

    int DividerModel::BasePosition(Divider divider) const noexcept
    {
        return IsValid(m_base, divider) ? EdgeOf(Tracks(m_base, divider.axis), divider.index) : 0;
    }

    bool DividerModel::IsModified(Divider divider) const noexcept
    {
        return IsValid(m_current, divider) && Position(divider) != BasePosition(divider);
    }

    bool DividerModel::IsModified() const noexcept
    {
        return m_current.rowsPercents() != m_base.rowsPercents() || m_current.columnsPercents() != m_base.columnsPercents();
    }

    bool DividerModel::Move(Divider divider, int position, int minTrack, int snapRadius)
    {
        if (!IsValid(m_current, divider))
        {
            return false;
        }

        auto& tracks = Tracks(m_current, divider.axis);
        const int previous = EdgeOf(tracks, divider.index - 1);
        const int next = EdgeOf(tracks, divider.index + 1);

        const int basePosition = BasePosition(divider);
        if (std::abs(position - basePosition) <= snapRadius)
        {
            position = basePosition;
        }

        // Per-side effective minimum: a track that is already smaller than minTrack may grow but never
        // shrinks further, and it does not force the divider to jump. With these minimums low <= high always
        // holds (the current position lies between them).
        const size_t nextTrack = static_cast<size_t>(divider.index);
        const size_t prevTrack = nextTrack - 1;
        const int effectiveMin = (std::max)(minTrack, 0);
        const int lowMin = (std::min)(effectiveMin, tracks[prevTrack]);
        const int highMin = (std::min)(effectiveMin, tracks[nextTrack]);
        const int low = previous + lowMin;
        const int high = next - highMin;
        if (low > high)
        {
            return false; // defensive only: cannot happen with non-negative tracks
        }

        position = std::clamp(position, low, high);

        const int before = position - previous;
        const int after = next - position;
        if (tracks[prevTrack] == before && tracks[nextTrack] == after)
        {
            return false;
        }

        tracks[prevTrack] = before;
        tracks[nextTrack] = after;
        return true;
    }
}
