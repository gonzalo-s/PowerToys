#include "pch.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include <FancyZonesLib/WorkArea.h>
#include <FancyZonesLib/FancyZonesData/AppliedLayouts.h>
#include <FancyZonesLib/FancyZonesData/AppZoneHistory.h>
#include <FancyZonesLib/FancyZonesData/CustomLayouts.h>
#include <FancyZonesLib/FancyZonesData/DefaultLayouts.h>
#include <FancyZonesLib/FancyZonesWindowProperties.h>
#include <FancyZonesLib/LayoutAssignedWindows.h>
#include <FancyZonesLib/WorkingLayouts.h>
#include <FancyZonesLib/ZoneRectModel.h>
#include "Util.h"

#include <common/utils/process_path.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FancyZonesUnitTests
{
    const std::wstring m_deviceId = L"\\\\?\\DISPLAY#DELA026#5&10a58c63&0&UID16777488#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}";
    const std::wstring m_virtualDesktopId = L"MyVirtualDesktopId";

    TEST_CLASS (WorkAreaCreationUnitTests)
    {
        FancyZonesDataTypes::WorkAreaId m_workAreaId;
        FancyZonesDataTypes::WorkAreaId m_emptyUniqueId;
        FancyZonesUtils::Rect m_workAreaRect{ RECT(0,0,1920,1080) };

        HINSTANCE m_hInst{};
        HMONITOR m_monitor{};

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            m_workAreaId.monitorId.deviceId.id = L"DELA026";
            m_workAreaId.monitorId.deviceId.instanceId = L"5&10a58c63&0&UID16777488";
            m_workAreaId.monitorId.serialNumber = L"serial-number";
            m_workAreaId.virtualDesktopId = FancyZonesUtils::GuidFromString(L"{39B25DD2-130D-4B5D-8851-4791D66B1539}").value();

            AppZoneHistory::instance().LoadData();
            AppliedLayouts::instance().LoadData();
            CustomLayouts::instance().LoadData();
            DefaultLayouts::instance().LoadData();
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            std::filesystem::remove(AppliedLayouts::AppliedLayoutsFileName());
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            std::filesystem::remove(CustomLayouts::CustomLayoutsFileName());
            std::filesystem::remove(DefaultLayouts::DefaultLayoutsFileName());
        }

        TEST_METHOD (CreateWorkArea)
        {
            const auto defaultLayout = DefaultLayouts::instance().GetDefaultLayout();

            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            const auto& layout = workArea->GetLayout();
            Assert::IsNotNull(layout.get());
            Assert::AreEqual(static_cast<int>(defaultLayout.type), static_cast<int>(layout->Type()));
            Assert::AreEqual(defaultLayout.zoneCount, static_cast<int>(layout->Zones().size()));
        }

        TEST_METHOD (CreateCombinedWorkArea)
        {
            const auto defaultLayout = DefaultLayouts::instance().GetDefaultLayout();

            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            const auto& layout = workArea->GetLayout();
            Assert::IsNotNull(layout.get());
            Assert::AreEqual(static_cast<int>(defaultLayout.type), static_cast<int>(layout->Type()));
            Assert::AreEqual(defaultLayout.zoneCount, static_cast<int>(layout->Zones().size()));
        }

        TEST_METHOD (CreateWorkAreaClonedFromParent)
        {
            using namespace FancyZonesDataTypes;
            
            FancyZonesDataTypes::WorkAreaId parentUniqueId;
            parentUniqueId.monitorId.deviceId.id = L"DELA026";
            parentUniqueId.monitorId.deviceId.instanceId = L"5&10a58c63&0&UID16777488";
            parentUniqueId.monitorId.serialNumber = L"serial-number";
            parentUniqueId.virtualDesktopId = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC57}").value();

            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = ZoneSetLayoutType::Rows,
                .showSpacing = true,
                .spacing = 10,
                .zoneCount = 10,
                .sensitivityRadius = 20,
            };

            auto parentWorkArea = WorkArea::Create(m_hInst, parentUniqueId, m_emptyUniqueId, m_workAreaRect);
            AppliedLayouts::instance().ApplyLayout(parentUniqueId, layout);

            auto actualWorkArea = WorkArea::Create(m_hInst, m_workAreaId, parentUniqueId, m_workAreaRect);
            Assert::IsNotNull(actualWorkArea->GetLayout().get());

            Assert::IsTrue(AppliedLayouts::instance().GetAppliedLayoutMap().contains(m_workAreaId));
            const auto& actualLayout = AppliedLayouts::instance().GetAppliedLayoutMap().at(m_workAreaId);

            Assert::AreEqual(static_cast<int>(layout.type), static_cast<int>(actualLayout.type));
            Assert::AreEqual(FancyZonesUtils::GuidToString(layout.uuid).value(), FancyZonesUtils::GuidToString(actualLayout.uuid).value());
            Assert::AreEqual(layout.sensitivityRadius, actualLayout.sensitivityRadius);
            Assert::AreEqual(layout.showSpacing, actualLayout.showSpacing);
            Assert::AreEqual(layout.spacing, actualLayout.spacing);
            Assert::AreEqual(layout.zoneCount, actualLayout.zoneCount);
        }

        TEST_METHOD (CreateWorkAreaWithCustomDefault)
        {
            // prepare
            json::JsonObject root{};
            json::JsonArray layoutsArray{};
            json::JsonObject layout{};
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::UuidID, json::value(L"{ACE817FD-2C51-4E13-903A-84CAB86FD17C}"));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::TypeID, json::value(L"custom"));
            json::JsonObject item{};
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::MonitorConfigurationTypeID, json::value(L"horizontal"));
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::LayoutID, layout);
            layoutsArray.Append(item);
            root.SetNamedValue(NonLocalizable::DefaultLayoutsIds::DefaultLayoutsArrayID, layoutsArray);
            
            json::to_file(DefaultLayouts::DefaultLayoutsFileName(), root);
            DefaultLayouts::instance().LoadData();

            // test
            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            Assert::IsNotNull(workArea->GetLayout().get());

            const auto& actualLayout = workArea->GetLayout();
            Assert::AreEqual(static_cast<int>(FancyZonesDataTypes::ZoneSetLayoutType::Custom), static_cast<int>(actualLayout->Type()));
            Assert::IsTrue(FancyZonesUtils::GuidFromString(L"{ACE817FD-2C51-4E13-903A-84CAB86FD17C}").value() == actualLayout->Id());
        }

        TEST_METHOD (CreateWorkAreaWithTemplateDefault)
        {
            // prepare
            json::JsonObject root{};
            json::JsonArray layoutsArray{};
            json::JsonObject layout{};
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::TypeID, json::value(L"grid"));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::ShowSpacingID, json::value(true));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::SpacingID, json::value(1));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::ZoneCountID, json::value(4));
            layout.SetNamedValue(NonLocalizable::DefaultLayoutsIds::SensitivityRadiusID, json::value(30));

            json::JsonObject item{};
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::MonitorConfigurationTypeID, json::value(L"horizontal"));
            item.SetNamedValue(NonLocalizable::DefaultLayoutsIds::LayoutID, layout);
            layoutsArray.Append(item);
            root.SetNamedValue(NonLocalizable::DefaultLayoutsIds::DefaultLayoutsArrayID, layoutsArray);

            json::to_file(DefaultLayouts::DefaultLayoutsFileName(), root);
            DefaultLayouts::instance().LoadData();

            // test
            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsTrue(m_workAreaId == workArea->UniqueId());

            Assert::IsNotNull(workArea->GetLayout().get());
            
            const auto& actualLayout = workArea->GetLayout();
            Assert::AreEqual(static_cast<int>(FancyZonesDataTypes::ZoneSetLayoutType::Grid), static_cast<int>(actualLayout->Type()));
            Assert::AreEqual(static_cast<size_t>(4), actualLayout->Zones().size());
            Assert::IsTrue(GUID_NULL == actualLayout->Id());
        }

        // Saves a 1x2 custom grid layout to custom-layouts.json so it can be resolved by uuid.
        void SaveCustomGridLayout(const GUID& uuid, bool showSpacing, int spacing, int sensitivityRadius)
        {
            json::JsonObject root{};
            json::JsonArray layoutsArray{};

            json::JsonObject gridLayoutJson{};
            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::UuidID, json::value(FancyZonesUtils::GuidToString(uuid).value()));
            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::NameID, json::value(L"Custom grid layout"));
            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::TypeID, json::value(NonLocalizable::CustomLayoutsIds::GridID));

            json::JsonArray rowsPercentage{};
            rowsPercentage.Append(json::value(10000));

            json::JsonArray columnsPercentage{};
            columnsPercentage.Append(json::value(5000));
            columnsPercentage.Append(json::value(5000));

            json::JsonArray cells{};
            {
                json::JsonArray cellsRow{};
                cellsRow.Append(json::value(0));
                cellsRow.Append(json::value(1));
                cells.Append(cellsRow);
            }

            json::JsonObject info{};
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RowsID, json::value(1));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ColumnsID, json::value(2));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::RowsPercentageID, rowsPercentage);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ColumnsPercentageID, columnsPercentage);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::CellChildMapID, cells);
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::ShowSpacingID, json::value(showSpacing));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::SpacingID, json::value(spacing));
            info.SetNamedValue(NonLocalizable::CustomLayoutsIds::SensitivityRadiusID, json::value(sensitivityRadius));

            gridLayoutJson.SetNamedValue(NonLocalizable::CustomLayoutsIds::InfoID, info);
            layoutsArray.Append(gridLayoutJson);
            root.SetNamedValue(NonLocalizable::CustomLayoutsIds::CustomLayoutsArrayID, layoutsArray);

            json::to_file(CustomLayouts::CustomLayoutsFileName(), root);
            CustomLayouts::instance().LoadData();
        }

        // Regression test for GH #44058: editing a custom layout's spacing only rewrites
        // custom-layouts.json, while applied-layouts.json keeps a snapshot taken at apply time.
        // An already-created WorkArea must pick up the edited spacing when it is refreshed
        // (WorkArea::InitLayout, which is what the custom-layouts file-update handler calls via
        // RefreshLayouts). Without re-deriving the scalar properties from the current custom
        // layout the stale snapshot is used and the edit has no effect until re-apply.
        TEST_METHOD (EditedCustomLayoutSpacingRefreshesExistingWorkArea)
        {
            const auto uuid = FancyZonesUtils::GuidFromString(L"{5A9D6A0F-4C6E-4C0C-8B1B-9F3E7C2D1A11}").value();

            auto zoneGap = [](const auto& zones) -> LONG {
                std::vector<RECT> rects;
                for (const auto& [id, zone] : zones)
                {
                    rects.push_back(zone.GetZoneRect());
                }
                std::sort(rects.begin(), rects.end(), [](const RECT& a, const RECT& b) { return a.left < b.left; });
                return rects[1].left - rects[0].right;
            };

            // The layout and its applied snapshot initially have no spacing.
            SaveCustomGridLayout(uuid, /*showSpacing*/ false, /*spacing*/ 0, /*sensitivityRadius*/ 5);
            LayoutData snapshot{
                .uuid = uuid,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Custom,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 2,
                .sensitivityRadius = 5,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, snapshot);

            auto workArea = WorkArea::Create({}, m_workAreaId, m_emptyUniqueId, m_workAreaRect);
            Assert::IsFalse(workArea == nullptr);
            Assert::IsNotNull(workArea->GetLayout().get());
            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            Assert::AreEqual(0L, zoneGap(workArea->GetLayout()->Zones()), L"Zones should be adjacent before the edit");

            // User edits the layout to add spacing; only custom-layouts.json is rewritten, the
            // applied-layouts snapshot stays stale.
            SaveCustomGridLayout(uuid, /*showSpacing*/ true, /*spacing*/ 100, /*sensitivityRadius*/ 30);

            // The custom-layouts file-update handler refreshes active work areas via InitLayout.
            workArea->InitLayout();

            Assert::AreEqual(static_cast<size_t>(2), workArea->GetLayout()->Zones().size());
            Assert::IsTrue(zoneGap(workArea->GetLayout()->Zones()) > 20, L"Edited spacing was not applied to the existing work area");
        }
    };

    TEST_CLASS (WorkAreaSnapUnitTests)
    {
        HINSTANCE m_hInst{};
        const HMONITOR m_monitor = Mocks::Monitor();
        const FancyZonesUtils::Rect m_workAreaRect{ RECT(0, 0, 1920, 1080) };
        const FancyZonesDataTypes::WorkAreaId m_parentUniqueId = {};
        const FancyZonesDataTypes::WorkAreaId m_workAreaId = {
            .monitorId = {
                .monitor = m_monitor,
                .deviceId = {
                    .id = L"device-id-1",
                    .instanceId = L"5&10a58c63&0&UID16777488",
                    .number = 1,
                },
                .serialNumber = L"serial-number-1" },
            .virtualDesktopId = FancyZonesUtils::GuidFromString(L"{310F2924-B587-4D87-97C2-90031BDBE3F1}").value()
        };

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            AppZoneHistory::instance().LoadData();
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
        }

        TEST_METHOD (WhenWindowIsNotResizablePlacingItIntoTheZoneShouldNotResizeIt)
        {
            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 4,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);

            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            constexpr int originalWidth = 450;
            constexpr int originalHeight = 550;

            SetWindowPos(window, nullptr, 150, 150, originalWidth, originalHeight, SWP_SHOWWINDOW);
            SetWindowLong(window, GWL_STYLE, GetWindowLong(window, GWL_STYLE) & ~WS_SIZEBOX);

            Assert::IsTrue(workArea->Snap(window, { 1 }, true));

            // wait for the window to be resized
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            RECT inZoneRect;
            GetWindowRect(window, &inZoneRect);

            Assert::AreEqual(originalWidth, (int)inZoneRect.right - (int)inZoneRect.left);
            Assert::AreEqual(originalHeight, (int)inZoneRect.bottom - (int)inZoneRect.top);
        }

        TEST_METHOD (WhenWindowIsResizablePlacingItIntoTheZoneShouldResizeIt)
        {
            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 4,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);

            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst, L"", L"", 0, WS_THICKFRAME);

            SetWindowPos(window, nullptr, 150, 150, 450, 550, SWP_SHOWWINDOW);
            
            Assert::IsTrue(workArea->Snap(window, { 1 }, true));

            // wait for the window to be resized
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            RECT zonedWindowRect;
            GetWindowRect(window, &zonedWindowRect);

            RECT zoneRect = workArea->GetLayout()->Zones().at(1).GetZoneRect();

            Assert::AreEqual(zoneRect.left, zonedWindowRect.left);
            Assert::AreEqual(zoneRect.right, zonedWindowRect.right);
            Assert::AreEqual(zoneRect.top, zonedWindowRect.top);
            Assert::AreEqual(zoneRect.bottom, zonedWindowRect.bottom);
        }

        TEST_METHOD (SnapWindowPropertyTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet expected = { 1, 2 };
            Assert::IsTrue(workArea->Snap(window, expected));

            const auto actual = FancyZonesWindowProperties::RetrieveZoneIndexProperty(window);
            Assert::AreEqual(expected.size(), actual.size());
            for (int i = 0; i < expected.size(); i++)
            {
                Assert::AreEqual(expected.at(i), actual.at(i));
            }
        }

        TEST_METHOD (SnapAppZoneHistoryTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet expected = { 1, 2 };
            Assert::IsTrue(workArea->Snap(window, expected));

            const auto processPath = get_process_path(window);
            const auto history = AppZoneHistory::instance().GetZoneHistory(processPath, m_workAreaId);

            Assert::IsTrue(history.has_value());
            Assert::AreEqual(expected.size(), history->zoneIndexSet.size());
            for (int i = 0; i < expected.size(); i++)
            {
                Assert::AreEqual(expected.at(i), history->zoneIndexSet.at(i));
            }
        }

        TEST_METHOD (SnapLayoutAssignedWindowsTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet expected = { 1, 2 };
            Assert::IsTrue(workArea->Snap(window, expected));

            const auto& layoutWindows = workArea->GetLayoutWindows();
            Assert::IsTrue(expected == layoutWindows.GetZoneIndexSetFromWindow(window));
        }

        TEST_METHOD(SnapEmptyZones)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsFalse(workArea->Snap(window, {}));
        }

        TEST_METHOD(SnapToIncorrectZone)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            const ZoneIndexSet zones = { 10 };
            Assert::IsFalse(workArea->Snap(window, zones));

            const auto processPath = get_process_path(window);
            const auto history = AppZoneHistory::instance().GetZoneHistory(processPath, m_workAreaId);

            Assert::IsFalse(history.has_value());
        }

        TEST_METHOD (UnsnapPropertyTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsTrue(workArea->Snap(window, { 1, 2 }));
            Assert::IsTrue(workArea->Unsnap(window));

            const auto actual = FancyZonesWindowProperties::RetrieveZoneIndexProperty(window);
            Assert::IsTrue(actual.empty());
        }

        TEST_METHOD (UnsnapAppZoneHistoryTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsTrue(workArea->Snap(window, { 1, 2 }));
            Assert::IsTrue(workArea->Unsnap(window));

            const auto processPath = get_process_path(window);
            const auto history = AppZoneHistory::instance().GetZoneHistory(processPath, m_workAreaId);

            Assert::IsFalse(history.has_value());
        }

        TEST_METHOD (UnsnapLayoutAssignedWindowsTest)
        {
            const auto workArea = WorkArea::Create(m_hInst, m_workAreaId, m_parentUniqueId, m_workAreaRect);
            const auto window = Mocks::WindowCreate(m_hInst);

            Assert::IsTrue(workArea->Snap(window, { 1, 2 }));
            Assert::IsTrue(workArea->Unsnap(window));

            const auto& layoutWindows = workArea->GetLayoutWindows();
            Assert::IsTrue(layoutWindows.GetZoneIndexSetFromWindow(window).empty());
        }
    };

    TEST_CLASS (WorkAreaEdgeResizeUnitTests)
    {
        FancyZonesDataTypes::WorkAreaId m_workAreaId;
        FancyZonesUtils::Rect m_workAreaRect{ RECT(0, 0, 1920, 1080) };
        std::unique_ptr<WorkArea> m_workArea;

        TEST_METHOD_INITIALIZE(Init) noexcept
        {
            m_workAreaId.monitorId.deviceId.id = L"DELA026";
            m_workAreaId.monitorId.deviceId.instanceId = L"5&10a58c63&0&UID16777488";
            m_workAreaId.monitorId.serialNumber = L"serial-number";
            m_workAreaId.virtualDesktopId = FancyZonesUtils::GuidFromString(L"{39B25DD2-130D-4B5D-8851-4791D66B1539}").value();

            AppZoneHistory::instance().LoadData();
            AppliedLayouts::instance().LoadData();
            CustomLayouts::instance().LoadData();
            DefaultLayouts::instance().LoadData();
            WorkingLayouts::instance().Clear();

            LayoutData layout{
                .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 4,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);

            m_workArea = WorkArea::Create({}, m_workAreaId, {}, m_workAreaRect);
        }

        TEST_METHOD_CLEANUP(CleanUp) noexcept
        {
            m_workArea.reset();
            WorkingLayouts::instance().Clear();
            std::filesystem::remove(AppliedLayouts::AppliedLayoutsFileName());
            std::filesystem::remove(AppZoneHistory::AppZoneHistoryFileName());
            std::filesystem::remove(CustomLayouts::CustomLayoutsFileName());
            std::filesystem::remove(DefaultLayouts::DefaultLayoutsFileName());

            // drop the in-memory applied layouts too, so they do not leak into other test classes
            AppliedLayouts::instance().LoadData();
        }

        static constexpr FancyZonesZoneRects::MovedEdges Right()
        {
            FancyZonesZoneRects::MovedEdges moved;
            moved.right = true;
            return moved;
        }

        static constexpr FancyZonesZoneRects::MovedEdges Left()
        {
            FancyZonesZoneRects::MovedEdges moved;
            moved.left = true;
            return moved;
        }

        static constexpr FancyZonesZoneRects::MovedEdges Bottom()
        {
            FancyZonesZoneRects::MovedEdges moved;
            moved.bottom = true;
            return moved;
        }

        FancyZonesZoneRects::UnitRects BaseRects() const
        {
            return FancyZonesZoneRects::RectsFromGrid(LayoutConfigurator::GridInfo(AppliedLayouts::instance().GetDeviceLayout(m_workAreaId).value(), m_workAreaRect).value());
        }

        void ApplyTemplate(FancyZonesDataTypes::ZoneSetLayoutType type, int zoneCount, int spacing)
        {
            LayoutData layout{
                .uuid = GUID_NULL,
                .type = type,
                .showSpacing = spacing > 0,
                .spacing = spacing,
                .zoneCount = zoneCount,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, layout);
            m_workArea->InitLayout();
        }

    public:
        TEST_METHOD (GridLayoutSupportsEdgeResize)
        {
            Assert::IsTrue(m_workArea->SupportsEdgeResize());
        }

        TEST_METHOD (MovingRightEdgeMovesOnlyTopRow)
        {
            // zone 0 is the top-left zone, its visible frame is 0,0,960,540; drag the right edge to x = 1160
            // position = ceil(1160 * 10000 / 1920) = ceil(6041.67) = 6042 units -> floor(1920 * 6042 / 10000) = 1160 px
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsFalse(result.atBase);

            const auto& zones = m_workArea->GetActiveZones();
            Assert::AreEqual(1160L, zones.at(0).GetZoneRect().right);
            Assert::AreEqual(1160L, zones.at(1).GetZoneRect().left);
            // only the zones touching the dragged edge move: the bottom row keeps its wall at 960
            Assert::IsTrue(std::abs(zones.at(2).GetZoneRect().right - 960L) <= 1);
            Assert::IsTrue(std::abs(zones.at(3).GetZoneRect().left - 960L) <= 1);
            // zones still tile the work area
            Assert::AreEqual(1920L, zones.at(1).GetZoneRect().right);
            Assert::AreEqual(zones.at(0).GetZoneRect().right, zones.at(1).GetZoneRect().left);
            Assert::AreEqual(540L, zones.at(0).GetZoneRect().bottom);
            Assert::AreEqual(540L, zones.at(1).GetZoneRect().bottom);
        }

        TEST_METHOD (MovingBottomEdgeMovesOnlyLeftColumn)
        {
            // drag zone 0's bottom edge from 540 to 740: ceil(740 * 10000 / 1080) = ceil(6851.85) = 6852 units
            // -> floor(1080 * 6852 / 10000) = floor(740.02) = 740 px
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 960, 740 }, Bottom(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsFalse(result.atBase);

            const auto& zones = m_workArea->GetActiveZones();
            Assert::AreEqual(740L, zones.at(0).GetZoneRect().bottom);
            Assert::AreEqual(740L, zones.at(2).GetZoneRect().top);
            Assert::AreEqual(1080L, zones.at(2).GetZoneRect().bottom);
            // the right column keeps its wall at 540
            Assert::IsTrue(std::abs(zones.at(1).GetZoneRect().bottom - 540L) <= 1);
            Assert::IsTrue(std::abs(zones.at(3).GetZoneRect().top - 540L) <= 1);
            // the vertical wall did not move
            Assert::AreEqual(960L, zones.at(0).GetZoneRect().right);
            Assert::AreEqual(960L, zones.at(2).GetZoneRect().right);
        }

        TEST_METHOD (SnapsBackToBaseNearOriginalLine)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);

            // 960 + 5 px is within the snap radius, so the divider returns exactly to the base line
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 965, 540 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsTrue(result.atBase);
            Assert::AreEqual(960L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (OuterEdgeDoesNothing)
        {
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ -100, 0, 960, 540 }, Left(), nullptr);
            Assert::IsFalse(result.changed);
            Assert::IsFalse(result.dividerDragged);
            Assert::IsTrue(result.outerEdgeMoved);
            Assert::AreEqual(0L, m_workArea->GetActiveZones().at(0).GetZoneRect().left);

            // an outer-edge-only resize is the natural FancyZones resize: no working layout entry is created
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), BaseRects()));
        }

        TEST_METHOD (DividerEdgeReportsDividerDragged)
        {
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            Assert::IsTrue(result.dividerDragged);
            Assert::IsFalse(result.outerEdgeMoved);
        }

        TEST_METHOD (CornerWithOuterEdgeReportsBoth)
        {
            // zone 0: the right edge is on a divider, the top edge is on the work-area border
            FancyZonesZoneRects::MovedEdges moved;
            moved.right = true;
            moved.top = true;
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, -50, 1160, 540 }, moved, nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsTrue(result.dividerDragged);
            Assert::IsTrue(result.outerEdgeMoved);
            // the outer edge is not a divider: the top row keeps its position
            Assert::AreEqual(0L, m_workArea->GetActiveZones().at(0).GetZoneRect().top);
        }

        TEST_METHOD (FinishWithOnlyOuterEdgeLeavesLayoutAlone)
        {
            const ZonesMap before = m_workArea->GetActiveZones();
            m_workArea->FinishEdgeResize({ 0 }, RECT{ -100, 0, 960, 540 }, Left());

            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), BaseRects()));
            const auto& after = m_workArea->GetActiveZones();
            Assert::AreEqual(before.size(), after.size());
            for (const auto& [id, zone] : before)
            {
                const RECT a = zone.GetZoneRect();
                const RECT b = after.at(id).GetZoneRect();
                Assert::IsTrue(EqualRect(&a, &b) != FALSE);
            }
        }

        TEST_METHOD (ZonesRectChangedDetectsMovedDivider)
        {
            const ZonesMap before = m_workArea->GetActiveZones();
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            const ZonesMap after = m_workArea->GetActiveZones();

            Assert::IsTrue(WorkArea::ZonesRectChanged(before, after, { 0 }));
            Assert::IsTrue(WorkArea::ZonesRectChanged(before, after, { 1 }));
            // the bottom row does not touch the dragged edge: those windows are not moved
            Assert::IsFalse(WorkArea::ZonesRectChanged(before, after, { 2 }));
            Assert::IsFalse(WorkArea::ZonesRectChanged(before, after, { 3 }));
            // a window spanning the whole top row keeps its combined rect
            Assert::IsFalse(WorkArea::ZonesRectChanged(before, after, { 0, 1 }));
        }

        TEST_METHOD (ZonesRectChangedFalseWhenNothingMoved)
        {
            const ZonesMap before = m_workArea->GetActiveZones();
            // left edge of zone 0 is on the work-area border: nothing moves
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ -100, 0, 960, 540 }, Left(), nullptr);
            const ZonesMap after = m_workArea->GetActiveZones();

            Assert::IsFalse(WorkArea::ZonesRectChanged(before, after, { 0 }));
            Assert::IsFalse(WorkArea::ZonesRectChanged(before, after, { 3 }));
            Assert::IsFalse(WorkArea::ZonesRectChanged(before, after, { 1, 3 }));
            // a zone missing from either map counts as changed
            Assert::IsTrue(WorkArea::ZonesRectChanged(before, ZonesMap{}, { 0 }));
        }

        TEST_METHOD (DividerIsClampedToMinimumZoneSize)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1900, 540 }, Right(), nullptr);
            const auto& zones = m_workArea->GetActiveZones();
            // the right zone keeps at least the fixed minimum track size of 50 px (260 units of 1920 px = 49.9 px)
            const long width = zones.at(1).GetZoneRect().right - zones.at(1).GetZoneRect().left;
            Assert::IsTrue(width >= 49 && width <= 51);
        }

        TEST_METHOD (DividerIsClampedToMinimumZoneSizeOnTheLeft)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 10, 540 }, Right(), nullptr);
            const auto& zones = m_workArea->GetActiveZones();
            const long width = zones.at(0).GetZoneRect().right - zones.at(0).GetZoneRect().left;
            Assert::IsTrue(width >= 49 && width <= 51);
        }

        TEST_METHOD (MinimumZoneSizeIsFixedOnLargeWorkArea)
        {
            // the minimum does not scale with the work area: 50 px also on a 3840 px wide work area
            m_workArea.reset();
            m_workArea = WorkArea::Create({}, m_workAreaId, {}, FancyZonesUtils::Rect(RECT(0, 0, 3840, 2160)));
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 3800, 1080 }, Right(), nullptr);
            const auto& zones = m_workArea->GetActiveZones();
            const long width = zones.at(1).GetZoneRect().right - zones.at(1).GetZoneRect().left;
            Assert::IsTrue(width >= 49 && width <= 51);
        }

        TEST_METHOD (FinishRemovesEntryWhenBackAtBase)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 962, 540 }, Right());

            Assert::AreEqual(960L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
            const auto base = BaseRects();
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
        }

        TEST_METHOD (DiscardRemovesUnmodifiedWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 960, 540 }, Right(), nullptr);
            const auto base = BaseRects();
            // precondition: the resize created an (unmodified) working entry
            Assert::IsNotNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));

            m_workArea->DiscardUnmodifiedWorkingLayout();
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
        }

        TEST_METHOD (DiscardKeepsModifiedWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->DiscardUnmodifiedWorkingLayout();

            const auto base = BaseRects();
            Assert::IsNotNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
            Assert::AreEqual(1160L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (FinishKeepsModifiedLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            Assert::AreEqual(1160L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
            // the layout (overlay, drop targets) keeps the original line
            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (RecreatedWorkAreaKeepsWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            // simulates the work area being rebuilt after display changes / resume from hibernate
            m_workArea.reset();
            m_workArea = WorkArea::Create({}, m_workAreaId, {}, m_workAreaRect);

            Assert::AreEqual(1160L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
            Assert::AreEqual(1160L, m_workArea->GetActiveZones().at(1).GetZoneRect().left);
            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (ColumnsTemplateWithSpacingReturnsToTemplateGeometry)
        {
            LayoutData columns{
                .uuid = GUID_NULL,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Columns,
                .showSpacing = true,
                .spacing = 16,
                .zoneCount = 3,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, columns);
            m_workArea->InitLayout();
            Assert::IsTrue(m_workArea->SupportsEdgeResize());

            const auto assertTemplate = [](const ZonesMap& zones) {
                Assert::AreEqual(size_t{ 3 }, zones.size());
                const std::vector<std::pair<long, long>> expected{ { 16, 634 }, { 650, 1269 }, { 1285, 1904 } };
                ZoneIndex id = 0;
                for (const auto& [left, right] : expected)
                {
                    const RECT r = zones.at(id++).GetZoneRect();
                    Assert::AreEqual(left, r.left);
                    Assert::AreEqual(right, r.right);
                    Assert::AreEqual(16L, r.top);
                    Assert::AreEqual(1064L, r.bottom);
                }
            };

            // base zones are the template zones
            assertTemplate(m_workArea->GetLayout()->Zones());
            assertTemplate(m_workArea->GetActiveZones());

            // drag the right edge of zone 0 to x = 834
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 16, 16, 834, 1064 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            // boundary 834 + 8 = 842 px -> ceil(842 * 10000 / 1920) = 4386 units -> floor(842.11) = 842 -> zone right 834
            Assert::AreEqual(834L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
            // the layout itself keeps the template lines
            assertTemplate(m_workArea->GetLayout()->Zones());

            // release next to the original edge (634): within the snap radius, so the divider returns to base
            // (636 + 8 = 644 px -> ceil(3354.17) = 3355 units; base 3344; |11| <= 104 units = 20 px)
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 16, 16, 636, 1064 }, Right());

            assertTemplate(m_workArea->GetLayout()->Zones());
            assertTemplate(m_workArea->GetActiveZones());
            const auto base = FancyZonesZoneRects::RectsFromGrid(LayoutConfigurator::GridInfo(columns, m_workAreaRect).value());
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
        }

        TEST_METHOD (ChangingLayoutDiscardsWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            LayoutData other{
                .uuid = GUID_NULL,
                .type = FancyZonesDataTypes::ZoneSetLayoutType::Columns,
                .showSpacing = false,
                .spacing = 0,
                .zoneCount = 3,
                .sensitivityRadius = 20,
            };
            AppliedLayouts::instance().ApplyLayout(m_workAreaId, other);
            m_workArea->InitLayout();

            Assert::AreEqual(size_t{ 3 }, m_workArea->GetActiveZones().size());
            Assert::IsTrue(std::abs(m_workArea->GetActiveZones().at(0).GetZoneRect().right - 640L) <= 1);
            Assert::AreEqual(640L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (BaseLayoutStaysWhileWorkingZonesFollow)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);

            // the layout (overlay, drop targets, keyboard snapping) keeps the original lines
            const auto& base = m_workArea->GetLayout()->Zones();
            Assert::AreEqual(960L, base.at(0).GetZoneRect().right);
            Assert::AreEqual(960L, base.at(1).GetZoneRect().left);

            // the windows follow the working zones
            const auto& active = m_workArea->GetActiveZones();
            Assert::AreEqual(1160L, active.at(0).GetZoneRect().right);
            Assert::AreEqual(1160L, active.at(1).GetZoneRect().left);
            Assert::AreEqual(960L, active.at(2).GetZoneRect().right);
        }

        TEST_METHOD (WorkingEdgeEqualsReleasedFrameEdge)
        {
            // ceiling pixel -> unit conversion: floor(ceil(px * 10000 / W) * W / 10000) == px for W < 10000
            for (const long width : { 1920L, 3840L })
            {
                for (const int spacing : { 0, 16 })
                {
                    LayoutData grid{
                        .uuid = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value(),
                        .type = FancyZonesDataTypes::ZoneSetLayoutType::Grid,
                        .showSpacing = true,
                        .spacing = spacing,
                        .zoneCount = 4,
                        .sensitivityRadius = 20,
                    };
                    AppliedLayouts::instance().ApplyLayout(m_workAreaId, grid);

                    for (long frameRight = 1157; frameRight <= 1161; ++frameRight)
                    {
                        WorkingLayouts::instance().Clear();
                        m_workArea.reset();
                        m_workArea = WorkArea::Create({}, m_workAreaId, {}, FancyZonesUtils::Rect(RECT(0, 0, width, 1080)));

                        const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ spacing, spacing, frameRight, 500 }, Right(), nullptr);
                        Assert::IsTrue(result.changed);
                        const auto& zones = m_workArea->GetActiveZones();
                        Assert::AreEqual(frameRight, zones.at(0).GetZoneRect().right);
                        Assert::AreEqual(frameRight + spacing, zones.at(1).GetZoneRect().left);
                    }
                }
            }
        }

        TEST_METHOD (ResetWorkingLayoutRestoresBaseZones)
        {
            // no working layout: nothing happens
            m_workArea->ResetWorkingLayout();
            Assert::AreEqual(960L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);

            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());
            Assert::AreEqual(1160L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);

            m_workArea->ResetWorkingLayout();
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), BaseRects()));
            Assert::AreEqual(960L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
            Assert::AreEqual(960L, m_workArea->GetActiveZones().at(1).GetZoneRect().left);
            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (SnapRadiusIs20Px)
        {
            // 20 px = floor(20 * 10000 / 1920) = 104 units
            // 960 + 19 = 979 -> ceil(9790000 / 1920) = 5099 units, |99| <= 104: snaps to exactly 960
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 979, 540 }, Right(), nullptr);
            Assert::IsTrue(result.atBase);
            Assert::AreEqual(960L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);

            // 960 + 21 = 981 -> ceil(9810000 / 1920) = 5110 units, |110| > 104: stays at floor(5110 * 1920 / 10000) = 981
            result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 981, 540 }, Right(), nullptr);
            Assert::IsFalse(result.atBase);
            Assert::IsFalse(result.snapped);
            Assert::IsTrue(result.snaps.empty());
            Assert::AreEqual(981L, m_workArea->GetActiveZones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (SnapsToAnotherOriginalGridline)
        {
            // Grid with 5 zones: column percents 3333 | 3333 | 3334 (10000 * k / 3 boundaries), zones 0 1 2 / 3 4 4.
            // Vertical gridlines at 3333 units (floor(1920 * 3333 / 10000) = floor(639.94) = 639 px) and 6666 units
            // (floor(1279.87) = 1279 px, top row only).
            ApplyTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Grid, 5, 0);
            Assert::AreEqual(1279L, m_workArea->GetLayout()->Zones().at(1).GetZoneRect().right);
            Assert::AreEqual(639L, m_workArea->GetLayout()->Zones().at(3).GetZoneRect().right);

            // zone 3's right edge to 1270 px: ceil(12700000 / 1920) = 6615 units, |6615 - 6666| = 51 <= 104: snaps to
            // the top row's gridline, exactly 1279 px
            auto result = m_workArea->ApplyEdgeResize({ 3 }, RECT{ 0, 540, 1270, 1080 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsTrue(result.snapped);
            Assert::IsFalse(result.snappedOwnLine);
            Assert::IsFalse(result.atBase);
            Assert::IsTrue(result.snaps == std::vector<FancyZonesZoneRects::EdgeSnap>{ { true, 6666, false } });
            Assert::AreEqual(1279L, m_workArea->GetActiveZones().at(3).GetZoneRect().right);
            Assert::AreEqual(1279L, m_workArea->GetActiveZones().at(4).GetZoneRect().left);

            // 1240 px: ceil(12400000 / 1920) = 6459 units, 207 away from 6666: no snap, floor(1240.128) = 1240 px
            result = m_workArea->ApplyEdgeResize({ 3 }, RECT{ 0, 540, 1240, 1080 }, Right(), nullptr);
            Assert::IsFalse(result.snapped);
            Assert::IsTrue(result.snaps.empty());
            Assert::AreEqual(1240L, m_workArea->GetActiveZones().at(3).GetZoneRect().right);

            // released at 1290 px: ceil(12900000 / 1920) = 6719 units, 53 away: the edge lands exactly on 1279 px
            m_workArea->FinishEdgeResize({ 3 }, RECT{ 0, 540, 1290, 1080 }, Right());
            Assert::AreEqual(1279L, m_workArea->GetActiveZones().at(3).GetZoneRect().right);
            Assert::AreEqual(1279L, m_workArea->GetActiveZones().at(4).GetZoneRect().left);
            Assert::IsNotNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), BaseRects()));
        }

        TEST_METHOD (SnapsToOwnGridlineReportsOwnLine)
        {
            ApplyTemplate(FancyZonesDataTypes::ZoneSetLayoutType::Grid, 5, 0);
            m_workArea->ApplyEdgeResize({ 3 }, RECT{ 0, 540, 1000, 1080 }, Right(), nullptr);

            // 650 px: ceil(6500000 / 1920) = 3386 units, |3386 - 3333| = 53 <= 104: back on its own line (639 px)
            const auto result = m_workArea->ApplyEdgeResize({ 3 }, RECT{ 0, 540, 650, 1080 }, Right(), nullptr);
            Assert::IsTrue(result.snapped);
            Assert::IsTrue(result.snappedOwnLine);
            Assert::IsTrue(result.atBase);
            Assert::IsTrue(result.snaps == std::vector<FancyZonesZoneRects::EdgeSnap>{ { true, 3333, true } });
            Assert::AreEqual(639L, m_workArea->GetActiveZones().at(3).GetZoneRect().right);
        }

        TEST_METHOD (ResizeGuideLinesInPixels)
        {
            // PriorityGrid with 5 zones: columns 2500 | 5000 | 2500, zones 0 1 2 / 3 1 4 (zone 1 spans both rows)
            ApplyTemplate(FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid, 5, 0);
            const auto base = BaseRects();

            // 1920 x 1080: x = 1920 * 2500 / 10000 = 480, 1920 * 7500 / 10000 = 1440, y = 1080 * 5000 / 10000 = 540;
            // the horizontal line is interrupted by zone 1
            const std::vector<ResizeGuideLine> fullHd{
                { true, 2500, 480, 0, 1080 },
                { true, 7500, 1440, 0, 1080 },
                { false, 5000, 540, 0, 480 },
                { false, 5000, 540, 1440, 1920 },
            };
            Assert::IsTrue(WorkArea::ResizeGuideLines(base, 1920, 1080) == fullHd);

            // 1366 x 768: floor(341.5) = 341, floor(1024.5) = 1024, 384
            const std::vector<ResizeGuideLine> hdReady{
                { true, 2500, 341, 0, 768 },
                { true, 7500, 1024, 0, 768 },
                { false, 5000, 384, 0, 341 },
                { false, 5000, 384, 1024, 1366 },
            };
            Assert::IsTrue(WorkArea::ResizeGuideLines(base, 1366, 768) == hdReady);

            // spacing 0: the lines are the zone edges
            const auto& zones = m_workArea->GetLayout()->Zones();
            Assert::AreEqual(480L, zones.at(0).GetZoneRect().right);
            Assert::AreEqual(480L, zones.at(1).GetZoneRect().left);
            Assert::AreEqual(1440L, zones.at(1).GetZoneRect().right);
            Assert::AreEqual(540L, zones.at(0).GetZoneRect().bottom);

            // spacing 16: same lines (the base rects do not depend on spacing), now the centre of the 16 px gap
            ApplyTemplate(FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid, 5, 16);
            Assert::IsTrue(WorkArea::ResizeGuideLines(BaseRects(), 1920, 1080) == fullHd);
            const auto& spaced = m_workArea->GetLayout()->Zones();
            Assert::AreEqual(472L, spaced.at(0).GetZoneRect().right);
            Assert::AreEqual(488L, spaced.at(1).GetZoneRect().left);
            Assert::AreEqual(1432L, spaced.at(1).GetZoneRect().right);
            Assert::AreEqual(1448L, spaced.at(2).GetZoneRect().left);
            Assert::AreEqual(532L, spaced.at(0).GetZoneRect().bottom);
            Assert::AreEqual(548L, spaced.at(3).GetZoneRect().top);
        }

        TEST_METHOD (ReleasedEdgeNeedsPlacementOnlyWhenADraggedEdgeDiffers)
        {
            const RECT zone{ 0, 0, 1160, 540 };
            FancyZonesZoneRects::MovedEdges right;
            right.right = true;

            Assert::IsFalse(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 0, 0, 1160, 540 }, right));
            // a 1 px difference is tolerated (never fight the release position)
            Assert::IsFalse(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 0, 0, 1161, 540 }, right));
            Assert::IsFalse(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 0, 0, 1159, 540 }, right));
            // snapped or clamped: more than 1 px
            Assert::IsTrue(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 0, 0, 1162, 540 }, right));
            Assert::IsTrue(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 0, 0, 1140, 540 }, right));
            // edges that were not dragged on a divider are ignored
            Assert::IsFalse(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 30, 0, 1160, 560 }, right));

            FancyZonesZoneRects::MovedEdges leftAndBottom;
            leftAndBottom.left = true;
            leftAndBottom.bottom = true;
            Assert::IsTrue(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 30, 0, 1160, 540 }, leftAndBottom));
            Assert::IsTrue(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 0, 0, 1160, 560 }, leftAndBottom));
            Assert::IsFalse(WorkArea::ReleasedEdgeNeedsPlacement(zone, RECT{ 1, 0, 1160, 539 }, leftAndBottom));
        }
    };
}
