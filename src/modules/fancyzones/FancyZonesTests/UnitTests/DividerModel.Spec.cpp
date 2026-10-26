#include "pch.h"

#include <FancyZonesLib/DividerModel.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesDividers;
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
        GridLayoutInfo Grid2x2() { return MakeGrid({ 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } }); }

        // 2 rows x 3 columns, zone 1 spans both rows in the middle column
        GridLayoutInfo Priority5() { return MakeGrid({ 5000, 5000 }, { 2500, 5000, 2500 }, { { 0, 1, 2 }, { 3, 1, 4 } }); }
    }

    TEST_CLASS (DividerModelUnitTests)
    {
    public:
        TEST_METHOD (SpanOfSingleZone)
        {
            DividerModel model(Grid2x2());
            const auto span = model.SpanOf({ 3 });
            Assert::IsTrue(span.has_value());
            Assert::AreEqual(1, span->firstRow);
            Assert::AreEqual(1, span->lastRow);
            Assert::AreEqual(1, span->firstCol);
            Assert::AreEqual(1, span->lastCol);
        }

        TEST_METHOD (SpanOfMergedZone)
        {
            DividerModel model(Priority5());
            const auto span = model.SpanOf({ 1 });
            Assert::IsTrue(span.has_value());
            Assert::AreEqual(0, span->firstRow);
            Assert::AreEqual(1, span->lastRow);
            Assert::AreEqual(1, span->firstCol);
            Assert::AreEqual(1, span->lastCol);
        }

        TEST_METHOD (SpanOfUnknownZone)
        {
            DividerModel model(Grid2x2());
            Assert::IsFalse(model.SpanOf({ 42 }).has_value());
        }

        TEST_METHOD (OuterEdgesHaveNoDivider)
        {
            DividerModel model(Grid2x2());
            const auto span = model.SpanOf({ 0 }).value();
            MovedEdges moved;
            moved.left = true;
            moved.top = true;
            Assert::AreEqual(size_t{ 0 }, model.DividersFor(span, moved).size());
        }

        TEST_METHOD (InnerEdgesMapToDividers)
        {
            DividerModel model(Grid2x2());
            const auto span = model.SpanOf({ 0 }).value();
            MovedEdges moved;
            moved.right = true;
            moved.bottom = true;
            const auto dividers = model.DividersFor(span, moved);
            Assert::AreEqual(size_t{ 2 }, dividers.size());
            Assert::IsTrue(dividers[0].axis == Axis::Column);
            Assert::AreEqual(1, dividers[0].index);
            Assert::IsTrue(dividers[1].axis == Axis::Row);
            Assert::AreEqual(1, dividers[1].index);
        }

        TEST_METHOD (MergedZoneUsesItsOuterDividers)
        {
            DividerModel model(Priority5());
            const auto span = model.SpanOf({ 1 }).value();
            MovedEdges moved;
            moved.left = true;
            moved.right = true;
            const auto dividers = model.DividersFor(span, moved);
            Assert::AreEqual(size_t{ 2 }, dividers.size());
            Assert::AreEqual(1, dividers[0].index); // left edge of column 1
            Assert::AreEqual(2, dividers[1].index); // right edge of column 1
        }

        TEST_METHOD (MoveMovesWholeLine)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 6000, 500, 0));
            Assert::AreEqual(6000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(4000, model.Current().columnsPercents()[1]);
            // rows untouched
            Assert::AreEqual(5000, model.Current().rowsPercents()[0]);
            Assert::AreEqual(5000, model.Current().rowsPercents()[1]);
            Assert::AreEqual(6000, model.Position({ Axis::Column, 1 }));
        }

        TEST_METHOD (MoveClampsToMinimumTrack)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 9900, 1000, 0));
            Assert::AreEqual(9000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(1000, model.Current().columnsPercents()[1]);

            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 10, 1000, 0));
            Assert::AreEqual(1000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(9000, model.Current().columnsPercents()[1]);
        }

        TEST_METHOD (MoveInThreeColumnsKeepsOtherTracks)
        {
            DividerModel model(Priority5());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 3000, 500, 0));
            Assert::AreEqual(3000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(4500, model.Current().columnsPercents()[1]);
            Assert::AreEqual(2500, model.Current().columnsPercents()[2]);
        }

        TEST_METHOD (MoveSnapsToBaseWithinRadius)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 6000, 500, 50));
            Assert::IsTrue(model.IsModified());
            Assert::IsTrue(model.IsModified({ Axis::Column, 1 }));

            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 5030, 500, 50));
            Assert::AreEqual(5000, model.Position({ Axis::Column, 1 }));
            Assert::IsFalse(model.IsModified());
            Assert::IsFalse(model.IsModified({ Axis::Column, 1 }));
        }

        TEST_METHOD (MoveReportsNoChange)
        {
            DividerModel model(Grid2x2());
            Assert::IsFalse(model.Move({ Axis::Column, 1 }, 5000, 500, 0));
            Assert::IsFalse(model.Move({ Axis::Column, 1 }, 5010, 500, 50)); // snaps to the same base value
        }

        TEST_METHOD (MoveRejectsInvalidDivider)
        {
            DividerModel model(Grid2x2());
            Assert::IsFalse(model.Move({ Axis::Column, 0 }, 100, 500, 0));
            Assert::IsFalse(model.Move({ Axis::Column, 2 }, 100, 500, 0));
            Assert::IsFalse(model.Move({ Axis::Row, 5 }, 100, 500, 0));
        }

        TEST_METHOD (SmallTrackCanGrowButNotShrink)
        {
            // track 0 (500) is already below the minimum track (1000)
            DividerModel model(MakeGrid({ 10000 }, { 500, 9500 }, { { 0, 1 } }));

            // shrinking it further is clamped at its current size: no jump up to the minimum
            Assert::IsFalse(model.Move({ Axis::Column, 1 }, 300, 1000, 0));
            Assert::AreEqual(500, model.Current().columnsPercents()[0]);

            // growing is allowed
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 2000, 1000, 0));
            Assert::AreEqual(2000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(8000, model.Current().columnsPercents()[1]);

            // the other (large) track clamps normally at the minimum
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 9800, 1000, 0));
            Assert::AreEqual(9000, model.Current().columnsPercents()[0]);
            Assert::AreEqual(1000, model.Current().columnsPercents()[1]);
        }

        TEST_METHOD (MoveAllowedWhenBothTracksAreBelowTwiceTheMinimum)
        {
            // tracks 500 + 1400 < 2 * 1000: low (previous + min) used to exceed high (next - min) and block the move
            DividerModel model(MakeGrid({ 10000 }, { 500, 1400, 8100 }, { { 0, 1, 2 } }));
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 800, 1000, 0));
            Assert::AreEqual(800, model.Current().columnsPercents()[0]);
            Assert::AreEqual(1100, model.Current().columnsPercents()[1]);
            Assert::AreEqual(8100, model.Current().columnsPercents()[2]);

            // the right track (1100) may not shrink below the minimum (1000)
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, 1500, 1000, 0));
            Assert::AreEqual(900, model.Current().columnsPercents()[0]);
            Assert::AreEqual(1000, model.Current().columnsPercents()[1]);
        }

        TEST_METHOD (NegativeMinimumTrackIsTreatedAsZero)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.Move({ Axis::Column, 1 }, -500, -100, 0));
            Assert::AreEqual(0, model.Current().columnsPercents()[0]);
            Assert::AreEqual(10000, model.Current().columnsPercents()[1]);
        }

        TEST_METHOD (PositionGuardsInvalidDivider)
        {
            DividerModel model(Grid2x2());
            Assert::AreEqual(0, model.Position({ Axis::Column, 5 }));
            Assert::AreEqual(0, model.Position({ Axis::Row, -1 }));
            Assert::AreEqual(0, model.BasePosition({ Axis::Column, 5 }));
            Assert::AreEqual(0, model.BasePosition({ Axis::Row, -1 }));
            Assert::AreEqual(5000, model.BasePosition({ Axis::Row, 1 }));
        }

        TEST_METHOD (SameBaseDetectsChanges)
        {
            DividerModel model(Grid2x2());
            Assert::IsTrue(model.SameBase(Grid2x2()));
            Assert::IsFalse(model.SameBase(Priority5()));
            Assert::IsFalse(model.SameBase(MakeGrid({ 5000, 5000 }, { 4000, 6000 }, { { 0, 1 }, { 2, 3 } })));
            Assert::IsFalse(model.SameBase(MakeGrid({ 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 0, 3 } })));
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
