#include "pch.h"

#include <FancyZonesLib/ZoneRectModel.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesZoneRects;
using FancyZonesDataTypes::GridLayoutInfo;

namespace FancyZonesUnitTests
{
    namespace
    {
        GridLayoutInfo MakeGrid(const std::vector<int>& rows, const std::vector<int>& cols, const std::vector<std::vector<int>>& map)
        {
            return GridLayoutInfo(GridLayoutInfo::Full{
                .rows = static_cast<int>(rows.size()),
                .columns = static_cast<int>(cols.size()),
                .rowsPercents = rows,
                .columnsPercents = cols,
                .cellChildMap = map,
                .showSpacing = false,
                .spacing = 0,
                .sensitivityRadius = 0 });
        }

        // 2x2 grid, zones 0 1 / 2 3
        UnitRects G22() { return RectsFromGrid(MakeGrid({ 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } })); }

        // 2 rows x 3 columns, zone 1 spans both rows in the middle column: 0 1 2 / 3 1 4
        UnitRects P5() { return RectsFromGrid(MakeGrid({ 5000, 5000 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 3, 1, 4 } })); }

        // zones 0 1 on top, zone 2 spans the whole bottom row
        UnitRects Wide() { return RectsFromGrid(MakeGrid({ 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 2 } })); }

        UnitRects OneRow(const std::vector<int>& cols)
        {
            std::vector<std::vector<int>> map(1);
            for (size_t c = 0; c < cols.size(); ++c)
            {
                map[0].push_back(static_cast<int>(c));
            }

            return RectsFromGrid(MakeGrid({ 10000 }, cols, map));
        }

        void AssertRect(const UnitRect& expected, const UnitRect& actual)
        {
            Assert::AreEqual(expected.left, actual.left);
            Assert::AreEqual(expected.top, actual.top);
            Assert::AreEqual(expected.right, actual.right);
            Assert::AreEqual(expected.bottom, actual.bottom);
        }

        void AssertAtBase(const ZoneRectModel& model, ZoneIndex id)
        {
            AssertRect(model.Base().at(id), model.Current().at(id));
        }

        void AssertMoved(MoveResult result)
        {
            Assert::IsTrue(result == MoveResult::Moved);
        }

        void AssertUnchanged(MoveResult result)
        {
            Assert::IsTrue(result == MoveResult::Unchanged);
        }
    }

    TEST_CLASS (ZoneRectModelUnitTests)
    {
    public:
        TEST_METHOD (RectsFromGridOfTwoByTwo)
        {
            const auto rects = G22();
            Assert::AreEqual(size_t{ 4 }, rects.size());
            AssertRect({ 0, 0, 5000, 5000 }, rects.at(0));
            AssertRect({ 5000, 0, 10000, 5000 }, rects.at(1));
            AssertRect({ 0, 5000, 5000, 10000 }, rects.at(2));
            AssertRect({ 5000, 5000, 10000, 10000 }, rects.at(3));
        }

        TEST_METHOD (RectsFromGridOfMergedCells)
        {
            const auto rects = P5();
            Assert::AreEqual(size_t{ 5 }, rects.size());
            AssertRect({ 0, 0, 2500, 5000 }, rects.at(0));
            AssertRect({ 2500, 0, 7500, 10000 }, rects.at(1));
            AssertRect({ 7500, 0, 10000, 5000 }, rects.at(2));
            AssertRect({ 0, 5000, 2500, 10000 }, rects.at(3));
            AssertRect({ 7500, 5000, 10000, 10000 }, rects.at(4));

            const auto wide = Wide();
            Assert::AreEqual(size_t{ 3 }, wide.size());
            AssertRect({ 0, 5000, 10000, 10000 }, wide.at(2));
        }

        TEST_METHOD (RectsFromGridForcesLastBoundaryToTotal)
        {
            // 3333 * 3 = 9999: the last boundary is forced to 10000
            const auto rects = OneRow({ 3333, 3333, 3333 });
            AssertRect({ 0, 0, 3333, 10000 }, rects.at(0));
            AssertRect({ 3333, 0, 6666, 10000 }, rects.at(1));
            AssertRect({ 6666, 0, 10000, 10000 }, rects.at(2));
        }

        TEST_METHOD (MoveRightEdgeMovesOnlyTouchingZones)
        {
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 0));
            AssertRect({ 0, 0, 6000, 5000 }, model.Current().at(0));
            AssertRect({ 6000, 0, 10000, 5000 }, model.Current().at(1));
            // the bottom row is a separate wall segment
            AssertAtBase(model, 2);
            AssertAtBase(model, 3);
            Assert::IsTrue(model.IsModified());
        }

        TEST_METHOD (MoveBottomEdgeMovesOnlyTouchingZones)
        {
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Bottom, 3000, 500, 0));
            AssertRect({ 0, 0, 5000, 3000 }, model.Current().at(0));
            AssertRect({ 0, 3000, 5000, 10000 }, model.Current().at(2));
            AssertAtBase(model, 1);
            AssertAtBase(model, 3);
        }

        TEST_METHOD (MoveRightEdgePullsInZonesAlongASpanningNeighbour)
        {
            // zone 1 spans both rows, so moving zone 0's right edge also moves zone 3's right edge
            ZoneRectModel model(P5());
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 3000, 500, 0));
            AssertRect({ 0, 0, 3000, 5000 }, model.Current().at(0));
            AssertRect({ 0, 5000, 3000, 10000 }, model.Current().at(3));
            AssertRect({ 3000, 0, 7500, 10000 }, model.Current().at(1));
            AssertAtBase(model, 2);
            AssertAtBase(model, 4);
        }

        TEST_METHOD (MoveLeftEdgeOfSpanningZoneMovesAllLeftNeighbours)
        {
            ZoneRectModel model(P5());
            AssertMoved(model.MoveEdge({ 1 }, Side::Left, 2000, 500, 0));
            AssertRect({ 2000, 0, 7500, 10000 }, model.Current().at(1));
            AssertRect({ 0, 0, 2000, 5000 }, model.Current().at(0));
            AssertRect({ 0, 5000, 2000, 10000 }, model.Current().at(3));
            AssertAtBase(model, 2);
            AssertAtBase(model, 4);
        }

        TEST_METHOD (MoveBottomEdgePullsInZonesAboveAWideNeighbour)
        {
            // zone 2 spans the bottom row, so zone 1 must follow zone 0 (otherwise zone 2's top edge would split)
            ZoneRectModel model(Wide());
            AssertMoved(model.MoveEdge({ 0 }, Side::Bottom, 3000, 500, 0));
            AssertRect({ 0, 0, 5000, 3000 }, model.Current().at(0));
            AssertRect({ 5000, 0, 10000, 3000 }, model.Current().at(1));
            AssertRect({ 0, 3000, 10000, 10000 }, model.Current().at(2));
        }

        TEST_METHOD (OuterSidesAreNotMoved)
        {
            ZoneRectModel model(G22());
            Assert::IsTrue(model.MoveEdge({ 0 }, Side::Left, 1000, 500, 0) == MoveResult::Outer);
            Assert::IsTrue(model.MoveEdge({ 0 }, Side::Top, 1000, 500, 0) == MoveResult::Outer);
            Assert::IsTrue(model.MoveEdge({ 3 }, Side::Right, 9000, 500, 0) == MoveResult::Outer);
            Assert::IsTrue(model.MoveEdge({ 3 }, Side::Bottom, 9000, 500, 0) == MoveResult::Outer);
            Assert::IsFalse(model.IsModified());
            Assert::IsTrue(model.Current() == model.Base());

            Assert::IsFalse(model.HasNeighbour({ 0 }, Side::Left));
            Assert::IsFalse(model.HasNeighbour({ 0 }, Side::Top));
            Assert::IsTrue(model.HasNeighbour({ 0 }, Side::Right));
            Assert::IsTrue(model.HasNeighbour({ 0 }, Side::Bottom));
            // the block {0, 1} spans the whole width: its right side is the work-area border
            Assert::IsFalse(model.HasNeighbour({ 0, 1 }, Side::Right));
            Assert::IsTrue(model.HasNeighbour({ 0, 1 }, Side::Bottom));
        }

        TEST_METHOD (MultiZoneBlockMovesItsWholeSide)
        {
            ZoneRectModel rows(G22());
            AssertMoved(rows.MoveEdge({ 0, 1 }, Side::Bottom, 3000, 500, 0));
            AssertRect({ 0, 0, 5000, 3000 }, rows.Current().at(0));
            AssertRect({ 5000, 0, 10000, 3000 }, rows.Current().at(1));
            AssertRect({ 0, 3000, 5000, 10000 }, rows.Current().at(2));
            AssertRect({ 5000, 3000, 10000, 10000 }, rows.Current().at(3));

            ZoneRectModel columns(G22());
            AssertMoved(columns.MoveEdge({ 0, 2 }, Side::Right, 6000, 500, 0));
            AssertRect({ 0, 0, 6000, 5000 }, columns.Current().at(0));
            AssertRect({ 0, 5000, 6000, 10000 }, columns.Current().at(2));
            AssertRect({ 6000, 0, 10000, 5000 }, columns.Current().at(1));
            AssertRect({ 6000, 5000, 10000, 10000 }, columns.Current().at(3));
        }

        TEST_METHOD (MoveClampsToMinimumSize)
        {
            ZoneRectModel model(G22());
            // zone 1 keeps 500: 10000 - 500 = 9500
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 9900, 500, 0));
            Assert::AreEqual(9500, model.Current().at(0).right);
            Assert::AreEqual(9500, model.Current().at(1).left);

            // zone 0 keeps 500: 0 + 500
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 10, 500, 0));
            Assert::AreEqual(500, model.Current().at(0).right);
            Assert::AreEqual(500, model.Current().at(1).left);
            AssertAtBase(model, 2);
            AssertAtBase(model, 3);
        }

        TEST_METHOD (SmallBaseZoneCanReturnToItsBaseSize)
        {
            // columns 300 | 9400 | 300, zone 1 spans both rows; zones 0 and 3 (300 wide) are below the minimum (1000)
            ZoneRectModel model(RectsFromGrid(MakeGrid({ 5000, 5000 }, { 300, 9400, 300 }, { { 0, 1, 2 }, { 3, 1, 4 } })));

            // low = 0 + min(1000, 300, 300) = 300, high = 9700 - min(1000, 9400, 9400) = 8700
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 600, 1000, 0));
            Assert::AreEqual(600, model.Current().at(0).right);
            Assert::AreEqual(600, model.Current().at(3).right);
            Assert::AreEqual(600, model.Current().at(1).left);
            Assert::IsTrue(model.IsModified());

            // low = 0 + min(1000, 600, 300) = 300: back to base
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 300, 1000, 0));
            Assert::IsFalse(model.IsModified());
            Assert::IsTrue(model.Current() == model.Base());

            // never below its base size
            AssertUnchanged(model.MoveEdge({ 0 }, Side::Right, 100, 1000, 0));
            Assert::AreEqual(300, model.Current().at(0).right);
        }

        TEST_METHOD (SmallZoneCanGrowButNotShrink)
        {
            ZoneRectModel model(OneRow({ 500, 9500 }));

            // clamped at its current size: no jump up to the minimum
            AssertUnchanged(model.MoveEdge({ 0 }, Side::Right, 300, 1000, 0));
            Assert::AreEqual(500, model.Current().at(0).right);

            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 2000, 1000, 0));
            Assert::AreEqual(2000, model.Current().at(0).right);
            Assert::AreEqual(2000, model.Current().at(1).left);

            // high = 10000 - min(1000, 8000, 9500) = 9000
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 9800, 1000, 0));
            Assert::AreEqual(9000, model.Current().at(0).right);
            Assert::AreEqual(9000, model.Current().at(1).left);
        }

        TEST_METHOD (MoveAllowedWhenBothZonesAreBelowTwiceTheMinimum)
        {
            ZoneRectModel model(OneRow({ 500, 1400, 8100 }));
            // low = 0 + min(1000, 500, 500) = 500, high = 1900 - min(1000, 1400, 1400) = 900
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 800, 1000, 0));
            Assert::AreEqual(800, model.Current().at(0).right);
            Assert::AreEqual(800, model.Current().at(1).left);

            // high = 1900 - min(1000, 1100, 1400) = 900
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 1500, 1000, 0));
            Assert::AreEqual(900, model.Current().at(0).right);
            Assert::AreEqual(900, model.Current().at(1).left);
            AssertAtBase(model, 2);
        }

        TEST_METHOD (WallBetweenTwoSmallBaseZonesStaysAtBase)
        {
            // low = 0 + min(1000, 500, 500) = 500, high = 1000 - min(1000, 500, 500) = 500
            ZoneRectModel model(OneRow({ 500, 500, 9000 }));
            AssertUnchanged(model.MoveEdge({ 0 }, Side::Right, 700, 1000, 0));
            Assert::AreEqual(500, model.Current().at(0).right);
            Assert::IsFalse(model.IsModified());
        }

        TEST_METHOD (WallBetweenTwoSmallZonesIsNotFrozen)
        {
            ZoneRectModel model(OneRow({ 500, 500, 9000 }));

            // zone 1's right edge: low = 500 + min(1000, 500, 500) = 1000, high = 10000 - 1000 = 9000 -> 1300
            AssertMoved(model.MoveEdge({ 1 }, Side::Right, 1300, 1000, 0));
            AssertRect({ 500, 0, 1300, 10000 }, model.Current().at(1));

            // zone 0's right edge: low = 500, high = 1300 - min(1000, 800, 500) = 800
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 700, 1000, 0));
            Assert::AreEqual(700, model.Current().at(0).right);
            Assert::AreEqual(700, model.Current().at(1).left);

            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 900, 1000, 0));
            Assert::AreEqual(800, model.Current().at(0).right);

            // back to base: zone 0 right -> 500 (low = 0 + min(1000, 800, 500) = 500), zone 1 right -> 1000
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 500, 1000, 0));
            AssertMoved(model.MoveEdge({ 1 }, Side::Right, 1000, 1000, 0));
            Assert::IsFalse(model.IsModified());
        }

        TEST_METHOD (NegativeMinimumIsTreatedAsZero)
        {
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, -500, -100, 0));
            Assert::AreEqual(0, model.Current().at(0).right);
            Assert::AreEqual(0, model.Current().at(1).left);
        }

        TEST_METHOD (MoveSnapsToBaseWithinRadius)
        {
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 50));
            Assert::IsTrue(model.IsModified());

            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 5030, 500, 50));
            Assert::AreEqual(5000, model.Current().at(0).right);
            Assert::AreEqual(5000, model.Current().at(1).left);
            Assert::IsFalse(model.IsModified());
        }

        TEST_METHOD (MoveReportsNoChange)
        {
            ZoneRectModel model(G22());
            AssertUnchanged(model.MoveEdge({ 0 }, Side::Right, 5000, 500, 0));
            AssertUnchanged(model.MoveEdge({ 0 }, Side::Right, 5010, 500, 50)); // snaps to the same base value
            Assert::IsFalse(model.IsModified());
        }

        TEST_METHOD (UnknownOrEmptyZonesChangeNothing)
        {
            ZoneRectModel model(G22());
            AssertUnchanged(model.MoveEdge({ 42 }, Side::Right, 6000, 500, 0));
            AssertUnchanged(model.MoveEdge({ 0, 42 }, Side::Right, 6000, 500, 0));
            AssertUnchanged(model.MoveEdge({}, Side::Right, 6000, 500, 0));
            Assert::IsFalse(model.IsModified());
            Assert::IsFalse(model.HasNeighbour({ 42 }, Side::Right));
            Assert::IsFalse(model.HasNeighbour({}, Side::Right));
        }

        TEST_METHOD (BoundsOfZones)
        {
            ZoneRectModel model(G22());
            Assert::IsFalse(model.BoundsOf({}).has_value());
            Assert::IsFalse(model.BoundsOf({ 42 }).has_value());
            Assert::IsFalse(model.BoundsOf({ 0, 42 }).has_value());
            Assert::IsFalse(model.BaseBoundsOf({ 42 }).has_value());
            AssertRect({ 0, 0, 10000, 5000 }, model.BoundsOf({ 0, 1 }).value());
            AssertRect({ 5000, 0, 10000, 10000 }, model.BoundsOf({ 1, 3 }).value());

            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 0));
            AssertRect({ 0, 0, 6000, 5000 }, model.BoundsOf({ 0 }).value());
            AssertRect({ 0, 0, 5000, 5000 }, model.BaseBoundsOf({ 0 }).value());
            AssertRect({ 0, 0, 6000, 10000 }, model.BoundsOf({ 0, 2 }).value());
        }

        TEST_METHOD (CoincidingWallsStayIndependentSegments)
        {
            // Moving zone 0's and zone 1's bottoms separately to 6000 makes the two wall segments collinear again,
            // exactly like the base 2x2 grid. As in the base grid (MoveBottomEdgeMovesOnlyTouchingZones) they are two
            // segments meeting at a cross: zone 3 does not overlap zone 0, so dragging zone 0's bottom moves 0 and 2 only.
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Bottom, 6000, 500, 0));
            AssertMoved(model.MoveEdge({ 1 }, Side::Bottom, 6000, 500, 0));
            Assert::AreEqual(6000, model.Current().at(2).top);
            Assert::AreEqual(6000, model.Current().at(3).top);

            AssertMoved(model.MoveEdge({ 0 }, Side::Bottom, 7000, 500, 0));
            Assert::AreEqual(7000, model.Current().at(0).bottom);
            Assert::AreEqual(7000, model.Current().at(2).top);
            Assert::AreEqual(6000, model.Current().at(1).bottom);
            Assert::AreEqual(6000, model.Current().at(3).top);
        }

        TEST_METHOD (JaggedWallMovesAsOneSegment)
        {
            // after zone 0's bottom moves to 6000, zone 3 (left = 5000) touches zone 0 along 5000..6000, so the
            // vertical wall is one connected segment: dragging zone 0's right edge moves zones 0, 2 and 1, 3
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Bottom, 6000, 500, 0));
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 0));
            AssertRect({ 0, 0, 6000, 6000 }, model.Current().at(0));
            AssertRect({ 0, 6000, 6000, 10000 }, model.Current().at(2));
            AssertRect({ 6000, 0, 10000, 5000 }, model.Current().at(1));
            AssertRect({ 6000, 5000, 10000, 10000 }, model.Current().at(3));
        }

        TEST_METHOD (DraggingBackToBaseClearsModification)
        {
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 0));
            // zone 3 overlaps zone 0 along x 5000..6000, so the whole horizontal wall moves: 0, 1 bottoms and 2, 3 tops
            AssertMoved(model.MoveEdge({ 2 }, Side::Top, 4000, 500, 0));
            Assert::AreEqual(4000, model.Current().at(1).bottom);
            Assert::AreEqual(4000, model.Current().at(3).top);
            Assert::IsTrue(model.IsModified());

            AssertMoved(model.MoveEdge({ 1 }, Side::Left, 5000, 500, 0));
            Assert::IsTrue(model.IsModified());
            AssertMoved(model.MoveEdge({ 0, 1 }, Side::Bottom, 5000, 500, 0));
            Assert::IsFalse(model.IsModified());
            Assert::IsTrue(model.Current() == model.Base());
        }

        TEST_METHOD (SameBaseComparesRects)
        {
            ZoneRectModel model(G22());
            Assert::IsTrue(model.SameBase(G22()));
            Assert::IsFalse(model.SameBase(P5()));
            Assert::IsFalse(model.SameBase(RectsFromGrid(MakeGrid({ 5000, 5000 }, { 4000, 6000 }, { { 0, 1 }, { 2, 3 } }))));

            // still the same base after a move
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 0));
            Assert::IsTrue(model.SameBase(G22()));
        }

        TEST_METHOD (DetectMovedEdgesUsesTolerance)
        {
            const RECT before{ 100, 100, 500, 400 };
            const RECT after{ 101, 90, 560, 400 };
            const auto moved = DetectMovedEdges(before, after, 2);
            Assert::IsFalse(moved.left);
            Assert::IsTrue(moved.top);
            Assert::IsTrue(moved.right);
            Assert::IsFalse(moved.bottom);
            Assert::IsTrue(moved.any());
            Assert::IsFalse(DetectMovedEdges(before, before, 2).any());
        }

        TEST_METHOD (MovedEdgesAccumulate)
        {
            MovedEdges a;
            a.left = true;
            MovedEdges b;
            b.bottom = true;
            a |= b;
            Assert::IsTrue(a.left);
            Assert::IsTrue(a.bottom);
            Assert::IsFalse(a.right);
            Assert::IsFalse(a.top);
        }
    };
}
