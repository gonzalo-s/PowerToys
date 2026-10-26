#include "pch.h"

#include <filesystem>

#include <FancyZonesLib/LayoutConfigurator.h>
#include <FancyZonesLib/FancyZonesData/CustomLayouts.h>
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

        static constexpr const wchar_t* GridUuid = L"{ACE817FD-2C51-4E13-903A-84CAB86FD17D}";
        static constexpr const wchar_t* CanvasUuid = L"{ACE817FD-2C51-4E13-903A-84CAB86FD17C}";

        using CellMap = std::vector<std::vector<int>>;

        static void SaveCustomLayouts(const json::JsonArray& layouts)
        {
            json::JsonObject root{};
            root.SetNamedValue(NonLocalizable::CustomLayoutsIds::CustomLayoutsArrayID, layouts);
            json::to_file(CustomLayouts::CustomLayoutsFileName(), root);
            CustomLayouts::instance().LoadData();
        }

        static json::JsonObject CustomGridJson()
        {
            json::JsonObject layout{};
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::UuidID, json::value(GridUuid));
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::NameID, json::value(L"Custom grid layout"));
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::TypeID, json::value(NonLocalizable::CustomLayoutsIds::GridID));

            json::JsonArray rowsPercentage{};
            rowsPercentage.Append(json::value(5000));
            rowsPercentage.Append(json::value(5000));

            json::JsonArray columnsPercentage{};
            columnsPercentage.Append(json::value(3333));
            columnsPercentage.Append(json::value(5000));
            columnsPercentage.Append(json::value(1667));

            json::JsonArray cells{};
            for (const auto& values : CellMap{ { 0, 1, 2 }, { 3, 1, 4 } })
            {
                json::JsonArray row{};
                for (int v : values)
                {
                    row.Append(json::value(v));
                }
                cells.Append(row);
            }

            json::JsonObject info{};
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RowsID, json::value(2));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ColumnsID, json::value(3));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RowsPercentageID, rowsPercentage);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ColumnsPercentageID, columnsPercentage);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::CellChildMapID, cells);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::SensitivityRadiusID, json::value(20));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ShowSpacingID, json::value(false));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::SpacingID, json::value(16));
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::InfoID, info);
            return layout;
        }

        static json::JsonObject CustomCanvasJson()
        {
            json::JsonObject layout{};
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::UuidID, json::value(CanvasUuid));
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::NameID, json::value(L"Custom canvas layout"));
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::TypeID, json::value(NonLocalizable::CustomLayoutsIds::CanvasID));

            json::JsonArray zonesArray{};
            json::JsonObject zone{};
            zone.SetNamedValue(NonLocalizable::CustomLayoutsIds::XAxisID, json::value(0));
            zone.SetNamedValue(NonLocalizable::CustomLayoutsIds::YAxisID, json::value(0));
            zone.SetNamedValue(NonLocalizable::CustomLayoutsIds::WidthID, json::value(100));
            zone.SetNamedValue(NonLocalizable::CustomLayoutsIds::HeightID, json::value(100));
            zonesArray.Append(zone);

            json::JsonObject info{};
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RefWidthID, json::value(1920));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RefHeightID, json::value(1080));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ZonesID, zonesArray);
            layout.SetNamedValue(NonLocalizable::CustomLayoutsIds::InfoID, info);
            return layout;
        }

        static LayoutData CustomData(const wchar_t* uuid)
        {
            LayoutData data = Data(ZoneSetLayoutType::Custom, 0);
            data.uuid = FancyZonesUtils::GuidFromString(uuid).value();
            return data;
        }

        static void AssertRect(const ZonesMap& zones, ZoneIndex id, long left, long top, long right, long bottom)
        {
            const RECT r = zones.at(id).GetZoneRect();
            Assert::AreEqual(left, r.left);
            Assert::AreEqual(top, r.top);
            Assert::AreEqual(right, r.right);
            Assert::AreEqual(bottom, r.bottom);
        }

        TEST_METHOD_CLEANUP(CleanUp)
        {
            std::filesystem::remove(CustomLayouts::CustomLayoutsFileName());
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

        TEST_METHOD (GridInfoFiveZones)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, 5));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(2, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::AreEqual(5, info->zoneCount());
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0, 1, 2 }, { 3, 4, 4 } }));
            Assert::AreEqual(10000, Sum(info->rowsPercents()));
            Assert::AreEqual(10000, Sum(info->columnsPercents()));
        }

        TEST_METHOD (GridInfoTwoZones)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, 2));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(1, info->rows());
            Assert::AreEqual(2, info->columns());
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0, 1 } }));
        }

        TEST_METHOD (GridInfoOneZone)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, 1));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(1, info->rows());
            Assert::AreEqual(1, info->columns());
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0 } }));
        }

        TEST_METHOD (GridFiveZonesHardCodedRects)
        {
            // columns: 0..639, 639..1279, 1279..1920 (3333/6666/10000 of 1920, integer division); rows: 0..540, 540..1080
            const FancyZonesUtils::Rect workArea{ RECT{ 0, 0, 1920, 1080 } };
            const ZonesMap grid = LayoutConfigurator::Grid(workArea, 5, 0);
            const ZonesMap viaInfo = LayoutConfigurator::GridZones(workArea, LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, 5)).value(), 0);

            for (const ZonesMap* zones : { &grid, &viaInfo })
            {
                Assert::AreEqual((size_t)5, zones->size());
                AssertRect(*zones, 0, 0, 0, 639, 540);
                AssertRect(*zones, 1, 639, 0, 1279, 540);
                AssertRect(*zones, 2, 1279, 0, 1920, 540);
                AssertRect(*zones, 3, 0, 540, 639, 1080);
                AssertRect(*zones, 4, 639, 540, 1920, 1080);
            }
        }

        TEST_METHOD (PriorityGridFourZonesUsesPredefinedLayout)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::PriorityGrid, 4));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(2, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::IsTrue((info->columnsPercents() == std::vector<int>{ 2500, 5000, 2500 }));
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0, 1, 2 }, { 0, 1, 3 } }));
        }

        TEST_METHOD (PriorityGridElevenZonesFallsBackToGrid)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::PriorityGrid, 11));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(3, info->rows());
            Assert::AreEqual(4, info->columns());
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10, 10 } }));
        }

        TEST_METHOD (PriorityGridTwelveZonesFallsBackToGrid)
        {
            const auto info = LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::PriorityGrid, 12));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(3, info->rows());
            Assert::AreEqual(4, info->columns());
            Assert::IsTrue((info->rowsPercents() == std::vector<int>{ 3333, 3333, 3334 }));
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10, 11 } }));
        }

        TEST_METHOD (NonPositiveZoneCountReturnsNullopt)
        {
            for (int count : { 0, -1, -5 })
            {
                Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Columns, count)).has_value());
                Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Rows, count)).has_value());
                Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::Grid, count)).has_value());
                Assert::IsFalse(LayoutConfigurator::GridInfo(Data(ZoneSetLayoutType::PriorityGrid, count)).has_value());
            }
        }

        TEST_METHOD (GridInfoCustomGridLayout)
        {
            json::JsonArray layouts{};
            layouts.Append(CustomGridJson());
            SaveCustomLayouts(layouts);

            const auto info = LayoutConfigurator::GridInfo(CustomData(GridUuid));
            Assert::IsTrue(info.has_value());
            Assert::AreEqual(2, info->rows());
            Assert::AreEqual(3, info->columns());
            Assert::IsTrue((info->rowsPercents() == std::vector<int>{ 5000, 5000 }));
            Assert::IsTrue((info->columnsPercents() == std::vector<int>{ 3333, 5000, 1667 }));
            Assert::IsTrue((info->cellChildMap() == CellMap{ { 0, 1, 2 }, { 3, 1, 4 } }));
        }

        TEST_METHOD (GridInfoCustomCanvasLayoutReturnsNullopt)
        {
            json::JsonArray layouts{};
            layouts.Append(CustomCanvasJson());
            SaveCustomLayouts(layouts);

            Assert::IsFalse(LayoutConfigurator::GridInfo(CustomData(CanvasUuid)).has_value());
        }

        TEST_METHOD (GridInfoUnknownCustomLayoutReturnsNullopt)
        {
            json::JsonArray layouts{};
            layouts.Append(CustomGridJson());
            SaveCustomLayouts(layouts);

            Assert::IsFalse(LayoutConfigurator::GridInfo(CustomData(L"{11111111-2222-3333-4444-555555555555}")).has_value());
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
