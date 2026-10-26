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

        // top row: five 2000-wide zones 0..4; bottom row: zone 5 (0..2000) and zone 6 spanning 2000..10000.
        // Vertical gridlines at 2000, 4000, 6000, 8000; the bottom-row wall (zone 5 | zone 6) can reach all of them.
        UnitRects Steps() { return RectsFromGrid(MakeGrid({ 5000, 5000 }, { 2000, 2000, 2000, 2000, 2000 }, { { 0, 1, 2, 3, 4 }, { 5, 6, 6, 6, 6 } })); }

        // Steps transposed: left column zones 0..4 (2000 high each); right column zone 5 (y 0..2000) and zone 6 (y 2000..10000).
        UnitRects StepsTransposed()
        {
            return RectsFromGrid(MakeGrid({ 2000, 2000, 2000, 2000, 2000 }, { 5000, 5000 }, { { 0, 5 }, { 1, 6 }, { 2, 6 }, { 3, 6 }, { 4, 6 } }));
        }

        void AssertSnapped(const MoveOutcome& outcome, int position, bool ownBaseLine)
        {
            Assert::IsTrue(outcome.snapped);
            Assert::AreEqual(position, outcome.snappedPosition);
            Assert::AreEqual(ownBaseLine, outcome.ownBaseLine);
        }

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

        void AssertMoved(const MoveOutcome& outcome)
        {
            Assert::IsTrue(outcome.result == MoveResult::Moved);
        }

        void AssertUnchanged(const MoveOutcome& outcome)
        {
            Assert::IsTrue(outcome.result == MoveResult::Unchanged);
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
            Assert::IsTrue(model.MoveEdge({ 0 }, Side::Left, 1000, 500, 0).result == MoveResult::Outer);
            Assert::IsTrue(model.MoveEdge({ 0 }, Side::Top, 1000, 500, 0).result == MoveResult::Outer);
            Assert::IsTrue(model.MoveEdge({ 3 }, Side::Right, 9000, 500, 0).result == MoveResult::Outer);
            Assert::IsTrue(model.MoveEdge({ 3 }, Side::Bottom, 9000, 500, 0).result == MoveResult::Outer);
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

        TEST_METHOD (InnerCoordinatesExcludeBorders)
        {
            Assert::IsTrue(InnerCoordinates(P5(), true) == std::vector<int>{ 2500, 7500 });
            Assert::IsTrue(InnerCoordinates(P5(), false) == std::vector<int>{ 5000 });
            Assert::IsTrue(InnerCoordinates(Steps(), true) == std::vector<int>{ 2000, 4000, 6000, 8000 });
            Assert::IsTrue(InnerCoordinates(Steps(), false) == std::vector<int>{ 5000 });
            Assert::IsTrue(InnerCoordinates(G22(), true) == std::vector<int>{ 5000 });
            Assert::IsTrue(InnerCoordinates(OneRow({ 3333, 3333, 3333 }), true) == std::vector<int>{ 3333, 6666 });
            Assert::IsTrue(InnerCoordinates(OneRow({ 3333, 3333, 3333 }), false).empty());
        }

        TEST_METHOD (InnerGridLinesMergeSegments)
        {
            // P5: both vertical lines run through the whole height (zone 1 spans both rows); the horizontal line is
            // interrupted by zone 1 (no inner side between 2500 and 7500)
            const std::vector<GridLine> p5{
                { true, 2500, 0, 10000 },
                { true, 7500, 0, 10000 },
                { false, 5000, 0, 2500 },
                { false, 5000, 7500, 10000 },
            };
            Assert::IsTrue(InnerGridLines(P5()) == p5);

            // Steps: x = 2000 is made of the top-row segment (0..5000) and the bottom-row segment (5000..10000), which
            // touch and merge; the other vertical lines exist only in the top row; the five touching bottom sides of
            // the top row merge into one horizontal line
            const std::vector<GridLine> steps{
                { true, 2000, 0, 10000 },
                { true, 4000, 0, 5000 },
                { true, 6000, 0, 5000 },
                { true, 8000, 0, 5000 },
                { false, 5000, 0, 10000 },
            };
            Assert::IsTrue(InnerGridLines(Steps()) == steps);
        }

        TEST_METHOD (SnapsToAnotherOriginalLine)
        {
            // zone 5's right side (own base 2000) dragged to 6100: the gridline 6000 is 100 away (<= 200)
            ZoneRectModel model(Steps());
            const auto outcome = model.MoveEdge({ 5 }, Side::Right, 6100, 500, 200);
            AssertMoved(outcome);
            AssertSnapped(outcome, 6000, false);
            Assert::AreEqual(6000, model.Current().at(5).right);
            Assert::AreEqual(6000, model.Current().at(6).left);
            // the top row is a separate wall segment
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);
        }

        TEST_METHOD (NearestOfTwoCandidatesWins)
        {
            ZoneRectModel model(Steps());
            // 4800: 4000 is 800 away, 6000 is 1200 away, both <= 1500
            auto outcome = model.MoveEdge({ 5 }, Side::Right, 4800, 500, 1500);
            AssertMoved(outcome);
            AssertSnapped(outcome, 4000, false);
            Assert::AreEqual(4000, model.Current().at(5).right);

            // 5300: 4000 is 1300 away, 6000 is 700 away
            outcome = model.MoveEdge({ 5 }, Side::Right, 5300, 500, 1500);
            AssertMoved(outcome);
            AssertSnapped(outcome, 6000, false);
            Assert::AreEqual(6000, model.Current().at(5).right);

            // 3100: the own line 2000 is 1100 away but 4000 is nearer (900)
            outcome = model.MoveEdge({ 5 }, Side::Right, 3100, 500, 1500);
            AssertMoved(outcome);
            AssertSnapped(outcome, 4000, false);
            Assert::AreEqual(4000, model.Current().at(5).right);
        }

        TEST_METHOD (TiePrefersOwnBaseLineThenLowerCoordinate)
        {
            ZoneRectModel model(Steps());
            auto outcome = model.MoveEdge({ 5 }, Side::Right, 3500, 500, 0);
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);

            // 3000: 2000 (own) and 4000 are both 1000 away
            outcome = model.MoveEdge({ 5 }, Side::Right, 3000, 500, 1000);
            AssertMoved(outcome);
            AssertSnapped(outcome, 2000, true);
            Assert::IsFalse(model.IsModified());

            // 7000: 6000 and 8000 are both 1000 away, neither is the own line: the lower one wins
            outcome = model.MoveEdge({ 5 }, Side::Right, 7000, 500, 1000);
            AssertMoved(outcome);
            AssertSnapped(outcome, 6000, false);
            Assert::AreEqual(6000, model.Current().at(5).right);
        }

        TEST_METHOD (NoSnapOutsideRadius)
        {
            ZoneRectModel model(Steps());
            // 7000: 6000 and 8000 are 1000 away (> 999)
            auto outcome = model.MoveEdge({ 5 }, Side::Right, 7000, 500, 999);
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::IsFalse(outcome.ownBaseLine);
            Assert::AreEqual(7000, model.Current().at(5).right);

            // the work-area border (10000) is not a gridline: 9900 stays (high = 10000 - min(0, ...) = 10000)
            outcome = model.MoveEdge({ 5 }, Side::Right, 9900, 0, 200);
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::AreEqual(9900, model.Current().at(5).right);
        }

        TEST_METHOD (SnapsToOwnBaseLine)
        {
            ZoneRectModel model(Steps());
            AssertMoved(model.MoveEdge({ 5 }, Side::Right, 2600, 500, 0));

            auto outcome = model.MoveEdge({ 5 }, Side::Right, 2150, 500, 200);
            AssertMoved(outcome);
            AssertSnapped(outcome, 2000, true);
            Assert::IsFalse(model.IsModified());

            // still on the line: nothing moves, but the snap is reported
            outcome = model.MoveEdge({ 5 }, Side::Right, 2050, 500, 200);
            AssertUnchanged(outcome);
            AssertSnapped(outcome, 2000, true);
        }

        TEST_METHOD (ClampedSnapIsNotReported)
        {
            // 7900 snaps to 8000, but zone 6 keeps min(2500, 8000, 8000): high = 10000 - 2500 = 7500
            ZoneRectModel model(Steps());
            auto outcome = model.MoveEdge({ 5 }, Side::Right, 7900, 2500, 200);
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::AreEqual(7500, model.Current().at(5).right);
            Assert::AreEqual(7500, model.Current().at(6).left);

            // P5: zone 0's right side (base 2500) dragged to 7400 snaps to 7500, but zone 1 (2500..7500) keeps 500:
            // high = 7500 - min(500, 5000, 5000) = 7000
            ZoneRectModel p5(P5());
            outcome = p5.MoveEdge({ 0 }, Side::Right, 7400, 500, 200);
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::AreEqual(7000, p5.Current().at(0).right);
            Assert::AreEqual(7000, p5.Current().at(1).left);
        }

        TEST_METHOD (HorizontalSidesSnapToHorizontalLines)
        {
            ZoneRectModel model(StepsTransposed());
            // zone 5's bottom (own base 2000) dragged to 6100: the horizontal gridline 6000 is 100 away
            auto outcome = model.MoveEdge({ 5 }, Side::Bottom, 6100, 500, 200);
            AssertMoved(outcome);
            AssertSnapped(outcome, 6000, false);
            Assert::AreEqual(6000, model.Current().at(5).bottom);
            Assert::AreEqual(6000, model.Current().at(6).top);

            // the vertical gridline 5000 is not a candidate for a horizontal side
            outcome = model.MoveEdge({ 5 }, Side::Bottom, 5050, 500, 200);
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::AreEqual(5050, model.Current().at(5).bottom);
        }

        TEST_METHOD (OuterSideIsNotSnapped)
        {
            ZoneRectModel model(G22());
            const auto outcome = model.MoveEdge({ 0 }, Side::Left, 10, 500, 200);
            Assert::IsTrue(outcome.result == MoveResult::Outer);
            Assert::IsFalse(outcome.snapped);
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

        // ---- Occupancy (wave 7): empty zones are transparent ----
        // Units used below: min 260 (50 px of 1920), snap radius 104 (20 px of 1920).

        static OccupiedFn Only(std::vector<ZoneIndex> ids)
        {
            return [ids](ZoneIndex id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
        }

        TEST_METHOD (SingleWindowExpandsOverEmptyZones)
        {
            // The user's case: four 2500 columns, only zone 0 holds a window.
            const auto four = OneRow({ 2500, 2500, 2500, 2500 });

            // to the 75 % line (another original line, 0 away): exactly 7500, nothing else moves
            ZoneRectModel model(four);
            auto outcome = model.MoveEdge({ 0 }, Side::Right, 7500, 260, 104, Only({ 0 }));
            AssertMoved(outcome);
            AssertSnapped(outcome, 7500, false);
            Assert::IsFalse(outcome.clamped);
            Assert::AreEqual(7500, model.Current().at(0).right);
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);
            AssertAtBase(model, 3);

            // 7300: 200 from 7500 (> 104): exactly 7300
            ZoneRectModel free(four);
            outcome = free.MoveEdge({ 0 }, Side::Right, 7300, 260, 104, Only({ 0 }));
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::IsFalse(outcome.clamped);
            Assert::AreEqual(7300, outcome.position);
            Assert::AreEqual(7300, free.Current().at(0).right);

            // 9990: nothing is pushed and the border is the limit: exactly 9990 (no margin)
            outcome = free.MoveEdge({ 0 }, Side::Right, 9990, 260, 104, Only({ 0 }));
            AssertMoved(outcome);
            Assert::IsFalse(outcome.clamped);
            Assert::AreEqual(9990, outcome.position);
            Assert::AreEqual(9990, free.Current().at(0).right);
            AssertAtBase(free, 3);

            // and back (contract, no occupied neighbour): exactly where requested
            outcome = free.MoveEdge({ 0 }, Side::Right, 6000, 260, 104, Only({ 0 }));
            AssertMoved(outcome);
            Assert::AreEqual(6000, free.Current().at(0).right);
            AssertAtBase(free, 1);
        }

        TEST_METHOD (SingleWindowReachesTheBorderOnEverySide)
        {
            // Four columns, one window: the border is the limit; 10000 and 10500 both give exactly TotalUnits
            const auto four = OneRow({ 2500, 2500, 2500, 2500 });
            for (const int requested : { 10000, 10500 })
            {
                ZoneRectModel model(four);
                const auto outcome = model.MoveEdge({ 0 }, Side::Right, requested, 260, 104, Only({ 0 }));
                AssertMoved(outcome);
                Assert::AreEqual(10000, outcome.position);
                Assert::AreEqual(10000, model.Current().at(0).right);
                Assert::AreEqual(requested > 10000, outcome.clamped);
            }

            // Left of the last column toward 0 (0 and -500)
            for (const int requested : { 0, -500 })
            {
                ZoneRectModel model(four);
                const auto outcome = model.MoveEdge({ 3 }, Side::Left, requested, 260, 104, Only({ 3 }));
                AssertMoved(outcome);
                Assert::AreEqual(0, outcome.position);
                Assert::AreEqual(0, model.Current().at(3).left);
                Assert::AreEqual(requested < 0, outcome.clamped);
            }

            // two rows: Bottom of the top zone to 10500 -> 10000; Top of the bottom zone to -500 -> 0
            const auto rows = RectsFromGrid(MakeGrid({ 5000, 5000 }, { 10000 }, { { 0 }, { 1 } }));
            ZoneRectModel down(rows);
            auto outcome = down.MoveEdge({ 0 }, Side::Bottom, 10500, 260, 104, Only({ 0 }));
            AssertMoved(outcome);
            Assert::AreEqual(10000, down.Current().at(0).bottom);
            ZoneRectModel up(rows);
            outcome = up.MoveEdge({ 1 }, Side::Top, -500, 260, 104, Only({ 1 }));
            AssertMoved(outcome);
            Assert::AreEqual(0, up.Current().at(1).top);

            // the dragged zone itself still keeps its minimum from its opposite edge (contract side)
            ZoneRectModel shrink(four);
            outcome = shrink.MoveEdge({ 0 }, Side::Right, 10, 260, 104, Only({ 0 }));
            Assert::AreEqual(260, shrink.Current().at(0).right);
        }

        TEST_METHOD (PushedNeighbourAtTheBorderKeepsItsMinimum)
        {
            // zones 0 and 3 occupied: the border request stops at zone 3's bound, 10000 - 260 = 9740
            const auto four = OneRow({ 2500, 2500, 2500, 2500 });
            for (const int requested : { 10000, 10500 })
            {
                ZoneRectModel model(four);
                const auto outcome = model.MoveEdge({ 0 }, Side::Right, requested, 260, 104, Only({ 0, 3 }));
                Assert::IsTrue(outcome.clamped);
                Assert::AreEqual(9740, model.Current().at(0).right);
                Assert::AreEqual(9740, model.Current().at(3).left);
                Assert::AreEqual(10000, model.Current().at(3).right);
            }
        }

        TEST_METHOD (ExpandPushesOnlyOccupiedZones)
        {
            // zones 0 and 3 occupied: zone 0's right side to 8000 crosses the empty zones 1, 2 and pushes zone 3
            const auto four = OneRow({ 2500, 2500, 2500, 2500 });
            ZoneRectModel model(four);
            auto outcome = model.MoveEdge({ 0 }, Side::Right, 8000, 260, 104, Only({ 0, 3 }));
            AssertMoved(outcome);
            Assert::IsFalse(outcome.clamped);
            Assert::AreEqual(8000, model.Current().at(0).right);
            AssertRect({ 8000, 0, 10000, 10000 }, model.Current().at(3));
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);

            // 9990: zone 3 keeps min(260, 2000, 2500) = 260: limit 10000 - 260 = 9740
            outcome = model.MoveEdge({ 0 }, Side::Right, 9990, 260, 104, Only({ 0, 3 }));
            Assert::IsTrue(outcome.clamped);
            Assert::AreEqual(9740, model.Current().at(0).right);
            Assert::AreEqual(9740, model.Current().at(3).left);

            // zones 0, 2, 3 occupied: zone 2 (beyond the empty zone 1) is pushed once the side passes 5000
            ZoneRectModel gap(four);
            outcome = gap.MoveEdge({ 0 }, Side::Right, 6000, 260, 104, Only({ 0, 2, 3 }));
            AssertMoved(outcome);
            Assert::AreEqual(6000, gap.Current().at(0).right);
            AssertRect({ 6000, 0, 7500, 10000 }, gap.Current().at(2));
            AssertAtBase(gap, 1);
            AssertAtBase(gap, 3);
        }

        TEST_METHOD (ExpandIntoOccupiedNeighbourClampsAtItsMinimum)
        {
            // zones 0 and 1 occupied (2 and 3 empty): zone 1 follows as before
            const auto four = OneRow({ 2500, 2500, 2500, 2500 });
            ZoneRectModel model(four);
            auto outcome = model.MoveEdge({ 0 }, Side::Right, 4000, 260, 104, Only({ 0, 1 }));
            AssertMoved(outcome);
            Assert::AreEqual(4000, model.Current().at(0).right);
            AssertRect({ 4000, 0, 5000, 10000 }, model.Current().at(1));

            // past zone 1's right side: zone 1 is squeezed to 260 (its right side 5000 does not move: zone 2 is empty
            // and transparent): limit 5000 - 260 = 4740
            outcome = model.MoveEdge({ 0 }, Side::Right, 6000, 260, 104, Only({ 0, 1 }));
            AssertMoved(outcome);
            Assert::IsTrue(outcome.clamped);
            Assert::AreEqual(4740, outcome.position);
            AssertRect({ 4740, 0, 5000, 10000 }, model.Current().at(1));
            AssertAtBase(model, 2);
        }

        TEST_METHOD (ContractPullsOnlyOccupiedNeighbours)
        {
            const auto halves = OneRow({ 5000, 5000 });

            // zone 1 occupied: it follows the wall
            ZoneRectModel model(halves);
            auto outcome = model.MoveEdge({ 0 }, Side::Right, 2500, 260, 104, Only({ 0, 1 }));
            AssertMoved(outcome);
            Assert::AreEqual(2500, model.Current().at(0).right);
            Assert::AreEqual(2500, model.Current().at(1).left);

            // zone 1 empty: only the dragged side moves
            ZoneRectModel alone(halves);
            outcome = alone.MoveEdge({ 0 }, Side::Right, 2500, 260, 104, Only({ 0 }));
            AssertMoved(outcome);
            Assert::AreEqual(2500, alone.Current().at(0).right);
            AssertAtBase(alone, 1);

            // contracting below the dragged zone's own minimum: clamped at 0 + 260
            outcome = alone.MoveEdge({ 0 }, Side::Right, 100, 260, 104, Only({ 0 }));
            Assert::IsTrue(outcome.clamped);
            Assert::AreEqual(260, alone.Current().at(0).right);
        }

        TEST_METHOD (MergedMiddleZoneAllOccupiedKeepsTheWallClosure)
        {
            // P5 with every zone occupied (explicit always-true predicate): as ClampedSnapIsNotReported, zone 0 right to
            // 7400 snaps to 7500 but zone 1 keeps 500: 7000, and zone 3 (touching zone 1 on the same wall) follows.
            ZoneRectModel model(P5());
            const auto outcome = model.MoveEdge({ 0 }, Side::Right, 7400, 500, 200, [](ZoneIndex) { return true; });
            AssertMoved(outcome);
            Assert::IsFalse(outcome.snapped);
            Assert::IsTrue(outcome.clamped);
            Assert::AreEqual(7000, model.Current().at(0).right);
            Assert::AreEqual(7000, model.Current().at(3).right);
            Assert::AreEqual(7000, model.Current().at(1).left);

            // zone 3 empty: zone 1 is pushed, zone 3 stays (a gap is left where nobody is)
            ZoneRectModel sparse(P5());
            sparse.MoveEdge({ 0 }, Side::Right, 4000, 500, 200, Only({ 0, 1, 2, 4 }));
            Assert::AreEqual(4000, sparse.Current().at(0).right);
            Assert::AreEqual(4000, sparse.Current().at(1).left);
            AssertAtBase(sparse, 3);
        }

        TEST_METHOD (ClampedIsReportedOnlyBeyondTheLimit)
        {
            // G22 all occupied: zone 0 right, zone 1 keeps 500: limit 9500
            ZoneRectModel model(G22());
            auto outcome = model.MoveEdge({ 0 }, Side::Right, 9000, 500, 200);
            Assert::IsFalse(outcome.clamped);
            outcome = model.MoveEdge({ 0 }, Side::Right, 9800, 500, 200);
            Assert::IsTrue(outcome.clamped);
            Assert::AreEqual(9500, outcome.position);
            // already at the limit: Unchanged, still clamped (the live drag keeps being held there)
            outcome = model.MoveEdge({ 0 }, Side::Right, 9900, 500, 200);
            AssertUnchanged(outcome);
            Assert::IsTrue(outcome.clamped);
            Assert::AreEqual(9500, outcome.position);
        }

        // ---- User snap (wave 9): only windows that conflict with the snapped zones go back to their base rects ----
        // Four 2500 columns: zone 0 0..2500, zone 1 2500..5000, zone 2 5000..7500, zone 3 7500..10000.

        static void AssertChanged(const std::vector<ZoneIndex>& expected, const std::vector<ZoneIndex>& actual)
        {
            Assert::AreEqual(expected.size(), actual.size());
            for (size_t i = 0; i < expected.size(); ++i)
            {
                Assert::AreEqual(expected[i], actual[i]);
            }
        }

        // Windows in zones 0 and 3 (the others empty): zone 0 expanded to 4000 (40 %), zone 3 to 6000..10000 (60 %).
        static ZoneRectModel TwoExpandedWindows()
        {
            ZoneRectModel model(OneRow({ 2500, 2500, 2500, 2500 }));
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 4000, 260, 0, Only({ 0, 3 })));
            AssertMoved(model.MoveEdge({ 3 }, Side::Left, 6000, 260, 0, Only({ 0, 3 })));
            AssertRect({ 0, 0, 4000, 10000 }, model.Current().at(0));
            AssertRect({ 6000, 0, 10000, 10000 }, model.Current().at(3));
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);
            return model;
        }

        TEST_METHOD (SnapIntoZoneOneResetsOnlyTheWindowItOverlaps)
        {
            // zone 1's base 2500..5000 overlaps zone 0 (0..4000): zone 0 resets; zone 3 (6000..10000) does not overlap
            auto model = TwoExpandedWindows();
            AssertChanged({ 0 }, model.ResolveSnap({ 1 }, Only({ 0, 3 })));
            AssertRect({ 0, 0, 2500, 10000 }, model.Current().at(0));
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);
            AssertRect({ 6000, 0, 10000, 10000 }, model.Current().at(3));
            Assert::IsTrue(model.IsModified());
        }

        TEST_METHOD (SnapIntoZoneTwoResetsOnlyTheWindowItOverlaps)
        {
            // zone 2's base 5000..7500 overlaps zone 3 (6000..10000) but not zone 0 (0..4000)
            auto model = TwoExpandedWindows();
            AssertChanged({ 3 }, model.ResolveSnap({ 2 }, Only({ 0, 3 })));
            AssertRect({ 0, 0, 4000, 10000 }, model.Current().at(0));
            AssertAtBase(model, 3);
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);
            Assert::IsTrue(model.IsModified());
        }

        TEST_METHOD (SnapFarFromTheExpandedWindowsMovesNothing)
        {
            // five 2000 columns: zone 0 expanded to 3000, zone 4 to 7000..10000; zone 2 (4000..6000) overlaps neither
            ZoneRectModel model(OneRow({ 2000, 2000, 2000, 2000, 2000 }));
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 3000, 260, 0, Only({ 0, 4 })));
            AssertMoved(model.MoveEdge({ 4 }, Side::Left, 7000, 260, 0, Only({ 0, 4 })));
            const UnitRects before = model.Current();

            Assert::IsTrue(model.ResolveSnap({ 2 }, Only({ 0, 4 })).empty());
            Assert::IsTrue(before == model.Current());
            AssertRect({ 0, 0, 3000, 10000 }, model.Current().at(0));
            AssertRect({ 7000, 0, 10000, 10000 }, model.Current().at(4));
        }

        TEST_METHOD (SnapConflictsChainThroughResetZones)
        {
            // windows in zones 1, 2 and 3: zone 1's left to 1500 (over the empty zone 0), then zone 2's left to 4500
            // (pushing zone 1's right): zone 1 = 1500..4500, zone 2 = 4500..7500.
            ZoneRectModel model(OneRow({ 2500, 2500, 2500, 2500 }));
            AssertMoved(model.MoveEdge({ 1 }, Side::Left, 1500, 260, 0, Only({ 1, 2, 3 })));
            AssertMoved(model.MoveEdge({ 2 }, Side::Left, 4500, 260, 0, Only({ 1, 2, 3 })));
            AssertRect({ 1500, 0, 4500, 10000 }, model.Current().at(1));
            AssertRect({ 4500, 0, 7500, 10000 }, model.Current().at(2));

            // snap into zone 0 (0..2500): zone 1 (1500..4500) overlaps it and resets to 2500..5000, whose base overlaps
            // zone 2 (4500..7500): zone 2 resets too; zone 3 (7500..10000) only touches zone 2's base: kept
            AssertChanged({ 1, 2 }, model.ResolveSnap({ 0 }, Only({ 1, 2, 3 })));
            AssertAtBase(model, 0);
            AssertAtBase(model, 1);
            AssertAtBase(model, 2);
            AssertAtBase(model, 3);
            Assert::IsFalse(model.IsModified());
        }

        TEST_METHOD (SnapConflictWithAMultiZoneWindowResetsAllOfItsZones)
        {
            // 2 x 2: one window spans the right column (zones 1 and 3), its left side expanded to 4000
            ZoneRectModel model(G22());
            AssertMoved(model.MoveEdge({ 1, 3 }, Side::Left, 4000, 260, 0, Only({ 1, 3 })));
            AssertRect({ 4000, 0, 10000, 5000 }, model.Current().at(1));
            AssertRect({ 4000, 5000, 10000, 10000 }, model.Current().at(3));

            // snap into zone 0: zone 1 overlaps it; zone 3 does not, but it belongs to the same window: both reset
            const std::vector<ZoneIndexSet> windows{ { 1, 3 } };
            AssertChanged({ 1, 3 }, model.ResolveSnap({ 0 }, Only({ 1, 3 }), windows));
            AssertAtBase(model, 1);
            AssertAtBase(model, 3);
            AssertAtBase(model, 2);
            Assert::IsFalse(model.IsModified());

            // without the window list each zone is its own unit: only zone 1 overlaps
            ZoneRectModel perZone(G22());
            perZone.MoveEdge({ 1, 3 }, Side::Left, 4000, 260, 0, Only({ 1, 3 }));
            AssertChanged({ 1 }, perZone.ResolveSnap({ 0 }, Only({ 1, 3 })));
            AssertRect({ 4000, 5000, 10000, 10000 }, perZone.Current().at(3));

            // the window list alone marks its zones as occupied
            ZoneRectModel listed(G22());
            listed.MoveEdge({ 1, 3 }, Side::Left, 4000, 260, 0, Only({ 1, 3 }));
            AssertChanged({ 1, 3 }, listed.ResolveSnap({ 0 }, Only({}), windows));
        }

        TEST_METHOD (SnapTouchingAnExpandedWindowIsNoConflict)
        {
            // zone 0 expanded to 5000 (over the empty zone 1); zone 2 (5000..7500) only touches it
            ZoneRectModel model(OneRow({ 2500, 2500, 2500, 2500 }));
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 5000, 260, 0, Only({ 0 })));
            Assert::IsTrue(model.ResolveSnap({ 2 }, Only({ 0 })).empty());
            AssertRect({ 0, 0, 5000, 10000 }, model.Current().at(0));

            // the same in the other direction (zone 3 expanded to 5000..10000, snap into zone 1)
            ZoneRectModel other(OneRow({ 2500, 2500, 2500, 2500 }));
            AssertMoved(other.MoveEdge({ 3 }, Side::Left, 5000, 260, 0, Only({ 3 })));
            Assert::IsTrue(other.ResolveSnap({ 1 }, Only({ 3 })).empty());
            AssertRect({ 5000, 0, 10000, 10000 }, other.Current().at(3));
        }

        TEST_METHOD (SnapLeavesEmptyZonesUntouched)
        {
            // an earlier window expanded zone 2 to 4000..7500 and left (zone 2 is empty now); zone 0's window expanded
            // to 3000. Snap into zone 1 (2500..5000): zone 0 conflicts and resets; zone 2 overlaps too but is empty
            ZoneRectModel model(OneRow({ 2500, 2500, 2500, 2500 }));
            AssertMoved(model.MoveEdge({ 2 }, Side::Left, 4000, 260, 0, Only({ 2 })));
            AssertMoved(model.MoveEdge({ 0 }, Side::Right, 3000, 260, 0, Only({ 0 })));
            AssertRect({ 4000, 0, 7500, 10000 }, model.Current().at(2));

            AssertChanged({ 0 }, model.ResolveSnap({ 1 }, Only({ 0 })));
            AssertAtBase(model, 0);
            AssertRect({ 4000, 0, 7500, 10000 }, model.Current().at(2));
            AssertAtBase(model, 3);
        }

        TEST_METHOD (SnapResetsTheTargetZoneModifiedByAWindowThatLeft)
        {
            // an earlier window expanded zone 1 to 2500..6000 and left; zone 3's window is expanded to 7000..10000
            ZoneRectModel model(OneRow({ 2500, 2500, 2500, 2500 }));
            AssertMoved(model.MoveEdge({ 1 }, Side::Right, 6000, 260, 0, Only({ 1 })));
            AssertMoved(model.MoveEdge({ 3 }, Side::Left, 7000, 260, 0, Only({ 3 })));
            AssertRect({ 2500, 0, 6000, 10000 }, model.Current().at(1));

            // snap into zone 1 (nobody occupies it): it goes back to 2500..5000; zone 3 does not overlap it
            AssertChanged({ 1 }, model.ResolveSnap({ 1 }, Only({ 3 })));
            AssertAtBase(model, 1);
            AssertRect({ 7000, 0, 10000, 10000 }, model.Current().at(3));
            Assert::IsTrue(model.IsModified());
        }

        TEST_METHOD (SnapIntoUnknownOrBaseZonesChangesNothing)
        {
            auto model = TwoExpandedWindows();
            const UnitRects before = model.Current();
            Assert::IsTrue(model.ResolveSnap({}, Only({ 0, 3 })).empty());
            Assert::IsTrue(model.ResolveSnap({ 1, 9 }, Only({ 0, 3 })).empty());
            Assert::IsTrue(before == model.Current());

            // an unmodified model: nothing to reset
            ZoneRectModel fresh(OneRow({ 2500, 2500, 2500, 2500 }));
            Assert::IsTrue(fresh.ResolveSnap({ 1 }, Only({ 0, 2, 3 })).empty());
            Assert::IsFalse(fresh.IsModified());
        }
    };
}
