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
#include <FancyZonesLib/DividerModel.h>
#include <FancyZonesLib/WorkingLayouts.h>
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

        static constexpr FancyZonesDividers::MovedEdges Right()
        {
            FancyZonesDividers::MovedEdges moved;
            moved.right = true;
            return moved;
        }

        static constexpr FancyZonesDividers::MovedEdges Left()
        {
            FancyZonesDividers::MovedEdges moved;
            moved.left = true;
            return moved;
        }

    public:
        TEST_METHOD (GridLayoutSupportsEdgeResize)
        {
            Assert::IsTrue(m_workArea->SupportsEdgeResize());
        }

        TEST_METHOD (MovingRightEdgeMovesWholeDivider)
        {
            // zone 0 is the top-left zone, its visible frame is 0,0,960,540; drag the right edge to x = 1160
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsFalse(result.atBase);

            const auto& zones = m_workArea->GetLayout()->Zones();
            Assert::IsTrue(std::abs(zones.at(0).GetZoneRect().right - 1160L) <= 1);
            Assert::IsTrue(std::abs(zones.at(1).GetZoneRect().left - 1160L) <= 1);
            // whole line: the bottom row follows
            Assert::IsTrue(std::abs(zones.at(2).GetZoneRect().right - 1160L) <= 1);
            Assert::IsTrue(std::abs(zones.at(3).GetZoneRect().left - 1160L) <= 1);
            // zones still tile the work area
            Assert::AreEqual(1920L, zones.at(1).GetZoneRect().right);
            Assert::AreEqual(zones.at(0).GetZoneRect().right, zones.at(1).GetZoneRect().left);
        }

        TEST_METHOD (SnapsBackToBaseNearOriginalLine)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);

            // 960 + 5 px is within the snap radius, so the divider returns exactly to the base line
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 965, 540 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsTrue(result.atBase);
            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
        }

        TEST_METHOD (OuterEdgeDoesNothing)
        {
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ -100, 0, 960, 540 }, Left(), nullptr);
            Assert::IsFalse(result.changed);
            Assert::AreEqual(0L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().left);
        }

        TEST_METHOD (DividerIsClampedToMinimumZoneSize)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1900, 540 }, Right(), nullptr);
            const auto& zones = m_workArea->GetLayout()->Zones();
            // the right zone keeps at least the minimum track size: max(200 px, 10% of 1920) = 200 px
            Assert::IsTrue(zones.at(1).GetZoneRect().right - zones.at(1).GetZoneRect().left >= 199);
            Assert::IsTrue(zones.at(1).GetZoneRect().right - zones.at(1).GetZoneRect().left <= 201);
        }

        TEST_METHOD (MinimumZoneSizeScalesWithWorkArea)
        {
            // 10% of a 3840 px wide work area (384 px) is above the 200 px floor
            m_workArea.reset();
            m_workArea = WorkArea::Create({}, m_workAreaId, {}, FancyZonesUtils::Rect(RECT(0, 0, 3840, 2160)));
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 3800, 1080 }, Right(), nullptr);
            const auto& zones = m_workArea->GetLayout()->Zones();
            const long width = zones.at(1).GetZoneRect().right - zones.at(1).GetZoneRect().left;
            Assert::IsTrue(width >= 383 && width <= 385);
        }

        TEST_METHOD (FinishRemovesEntryWhenBackAtBase)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 962, 540 }, Right());

            Assert::AreEqual(960L, m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right);
            const auto base = LayoutConfigurator::GridInfo(AppliedLayouts::instance().GetDeviceLayout(m_workAreaId).value(), m_workAreaRect).value();
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
        }

        TEST_METHOD (DiscardRemovesUnmodifiedWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 960, 540 }, Right(), nullptr);
            m_workArea->DiscardUnmodifiedWorkingLayout();

            const auto base = LayoutConfigurator::GridInfo(AppliedLayouts::instance().GetDeviceLayout(m_workAreaId).value(), m_workAreaRect).value();
            Assert::IsNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
        }

        TEST_METHOD (DiscardKeepsModifiedWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->DiscardUnmodifiedWorkingLayout();

            const auto base = LayoutConfigurator::GridInfo(AppliedLayouts::instance().GetDeviceLayout(m_workAreaId).value(), m_workAreaRect).value();
            Assert::IsNotNull(WorkingLayouts::instance().Find(m_workAreaId, m_workArea->GetLayoutId(), base));
            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 1160L) <= 1);
        }

        TEST_METHOD (FinishKeepsModifiedLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 1160L) <= 1);
        }

        TEST_METHOD (RecreatedWorkAreaKeepsWorkingLayout)
        {
            m_workArea->ApplyEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right(), nullptr);
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 0, 0, 1160, 540 }, Right());

            // simulates the work area being rebuilt after display changes / resume from hibernate
            m_workArea.reset();
            m_workArea = WorkArea::Create({}, m_workAreaId, {}, m_workAreaRect);

            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 1160L) <= 1);
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

            const auto assertTemplate = [this]() {
                const auto& zones = m_workArea->GetLayout()->Zones();
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
            assertTemplate();

            // drag the right edge of zone 0 to x = 834
            const auto result = m_workArea->ApplyEdgeResize({ 0 }, RECT{ 16, 16, 834, 1064 }, Right(), nullptr);
            Assert::IsTrue(result.changed);
            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 834L) <= 1);

            // release next to the original edge (634): within the snap radius, so the divider returns to base
            m_workArea->FinishEdgeResize({ 0 }, RECT{ 16, 16, 636, 1064 }, Right());

            assertTemplate();
            const auto base = LayoutConfigurator::GridInfo(columns, m_workAreaRect).value();
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

            Assert::AreEqual(size_t{ 3 }, m_workArea->GetLayout()->Zones().size());
            Assert::IsTrue(std::abs(m_workArea->GetLayout()->Zones().at(0).GetZoneRect().right - 640L) <= 1);
        }
    };
}
