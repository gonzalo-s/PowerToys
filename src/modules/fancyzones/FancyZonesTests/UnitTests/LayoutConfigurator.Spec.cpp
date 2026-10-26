#include "pch.h"

#include <FancyZonesLib/LayoutConfigurator.h>
#include <FancyZonesLib/FancyZonesData/LayoutData.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesDataTypes;

namespace FancyZonesUnitTests
{
    TEST_CLASS (LayoutConfiguratorGridUnitTests)
    {
        static LayoutData Data(ZoneSetLayoutType type, int zoneCount)
        {
            return LayoutData{ .type = type, .showSpacing = false, .spacing = 0, .zoneCount = zoneCount };
        }

        static int Sum(const std::vector<int>& values)
        {
            int sum = 0;
            for (int v : values)
            {
                sum += v;
            }
            return sum;
        }

    public:
        TEST_METHOD (GridInfoForGridLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, 4));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(2, info->rows());
            Assert::AreEqual(2, info->columns());
            Assert::AreEqual(10000, Sum(info->rowsPercents()));
            Assert::AreEqual(10000, Sum(info->columnsPercents()));
            Assert::AreEqual(4, info->zoneCount());
        }

        TEST_METHOD (GridInfoForColumnsLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Columns, 3));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(1, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::AreEqual(10000, Sum(info->columnsPercents()));
            Assert::AreEqual(3, info->zoneCount());
        }

        TEST_METHOD (GridInfoForRowsLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Rows, 3));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(3, info->rows());
            Assert::AreEqual(1, info->columns());
            Assert::AreEqual(10000, Sum(info->rowsPercents()));
            Assert::AreEqual(3, info->zoneCount());
        }

        TEST_METHOD (GridInfoForPriorityGridLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::PriorityGrid, 3));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(1, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::AreEqual(2500, info->columnsPercents()[0]);
            Assert::AreEqual(5000, info->columnsPercents()[1]);
            Assert::AreEqual(2500, info->columnsPercents()[2]);
        }

        TEST_METHOD (GridInfoUnsupportedTypes)
        {
            Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Focus, 3)).has_value());
            Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Blank, 0)).has_value());
        }

        TEST_METHOD (GridZonesMatchesGridLayout)
        {
            const FancyZonesUtils::Rect workArea{ RECT{ 0, 0, 1920, 1080 } };
            const auto data = Data(ZoneSetLayoutType::Grid, 4);
            const auto expected = LayoutConfigurator::Grid(workArea, 4, 0);
            const auto actual = LayoutConfigurator::GridZones(workArea, LayoutConfigurator::GridInfo(data).value(), 0);

            Assert::AreEqual(expected.size(), actual.size());
            for (const auto& [id, zone] : expected)
            {
                const RECT e = zone.GetZoneRect();
                const RECT a = actual.at(id).GetZoneRect();
                Assert::AreEqual(e.left, a.left);
                Assert::AreEqual(e.top, a.top);
                Assert::AreEqual(e.right, a.right);
                Assert::AreEqual(e.bottom, a.bottom);
            }
        }

        TEST_METHOD (GridZonesMatchesColumnsLayoutWithinOnePixel)
        {
            const FancyZonesUtils::Rect workArea{ RECT{ 0, 0, 1920, 1080 } };
            const auto expected = LayoutConfigurator::Columns(workArea, 3, 0);
            const auto actual = LayoutConfigurator::GridZones(workArea, LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Columns, 3)).value(), 0);

            Assert::AreEqual(expected.size(), actual.size());
            for (const auto& [id, zone] : expected)
            {
                const RECT e = zone.GetZoneRect();
                const RECT a = actual.at(id).GetZoneRect();
                Assert::IsTrue(std::abs(e.left - a.left) <= 1);
                Assert::IsTrue(std::abs(e.right - a.right) <= 1);
                Assert::AreEqual(e.top, a.top);
                Assert::AreEqual(e.bottom, a.bottom);
            }
        }
    };
}
