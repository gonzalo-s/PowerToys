#include "pch.h"

#include <FancyZonesLib/WorkingLayouts.h>
#include <FancyZonesLib/util.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FancyZonesZoneRects;
using FancyZonesDataTypes::GridLayoutInfo;
using FancyZonesDataTypes::WorkAreaId;

namespace FancyZonesUnitTests
{
    TEST_CLASS (WorkingLayoutsUnitTests)
    {
        // Base zone rectangles of a one-row grid with the given column percents.
        static UnitRects MakeGrid(const std::vector<int>& cols)
        {
            const std::vector<int> rows{ 10000 };
            std::vector<std::vector<int>> map(1);
            for (size_t c = 0; c < cols.size(); ++c)
            {
                map[0].push_back(static_cast<int>(c));
            }

            return RectsFromGrid(GridLayoutInfo(GridLayoutInfo::Full{
                .rows = 1,
                .columns = static_cast<int>(cols.size()),
                .rowsPercents = rows,
                .columnsPercents = cols,
                .cellChildMap = map,
                .showSpacing = false,
                .spacing = 0,
                .sensitivityRadius = 0 }));
        }

        static WorkAreaId Id(const wchar_t* monitor, const wchar_t* desktop)
        {
            WorkAreaId id;
            id.monitorId.deviceId.id = monitor;
            id.monitorId.deviceId.instanceId = L"instance";
            id.monitorId.serialNumber = L"serial";
            id.virtualDesktopId = FancyZonesUtils::GuidFromString(desktop).value();
            return id;
        }

        const GUID m_layout = FancyZonesUtils::GuidFromString(L"{F762BAD6-DAA1-4997-9497-E11DFEB72F21}").value();
        const GUID m_otherLayout = FancyZonesUtils::GuidFromString(L"{61FA9FC0-26A6-4B37-A834-491C148DFC58}").value();
        const wchar_t* m_desktop = L"{39B25DD2-130D-4B5D-8851-4791D66B1539}";

        TEST_METHOD_INITIALIZE(Init)
        {
            WorkingLayouts::instance().Clear();
        }

        TEST_METHOD_CLEANUP(CleanUp)
        {
            WorkingLayouts::instance().Clear();
        }

    public:
        TEST_METHOD (FindOnEmptyStoreReturnsNull)
        {
            Assert::IsNull(WorkingLayouts::instance().Find(Id(L"A", m_desktop), m_layout, MakeGrid({ 5000, 5000 })));
        }

        TEST_METHOD (GetOrCreateThenFind)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });

            auto& model = WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            Assert::IsTrue(model.MoveEdge({ 0 }, Side::Right, 6000, 500, 0) == MoveResult::Moved);

            auto* found = WorkingLayouts::instance().Find(id, m_layout, base);
            Assert::IsNotNull(found);
            Assert::AreEqual(6000, found->Current().at(0).right);
            Assert::IsTrue(found->IsModified());
        }

        TEST_METHOD (GetOrCreateReturnsSameEntry)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });

            WorkingLayouts::instance().GetOrCreate(id, m_layout, base).MoveEdge({ 0 }, Side::Right, 6000, 500, 0);
            auto& again = WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            Assert::AreEqual(6000, again.Current().at(1).left);
        }

        TEST_METHOD (EntriesAreKeyedByWorkArea)
        {
            const auto base = MakeGrid({ 5000, 5000 });
            WorkingLayouts::instance().GetOrCreate(Id(L"A", m_desktop), m_layout, base).MoveEdge({ 0 }, Side::Right, 6000, 500, 0);

            Assert::IsNull(WorkingLayouts::instance().Find(Id(L"B", m_desktop), m_layout, base));
            Assert::IsNull(WorkingLayouts::instance().Find(Id(L"A", L"{61FA9FC0-26A6-4B37-A834-491C148DFC57}"), m_layout, base));
            Assert::IsNotNull(WorkingLayouts::instance().Find(Id(L"A", m_desktop), m_layout, base));
        }

        TEST_METHOD (EntryIgnoresHMonitor)
        {
            const auto base = MakeGrid({ 5000, 5000 });
            auto before = Id(L"A", m_desktop);
            before.monitorId.monitor = reinterpret_cast<HMONITOR>(0x1111);
            WorkingLayouts::instance().GetOrCreate(before, m_layout, base).MoveEdge({ 0 }, Side::Right, 6000, 500, 0);

            auto after = Id(L"A", m_desktop);
            after.monitorId.monitor = reinterpret_cast<HMONITOR>(0x2222); // handle changed after resume
            Assert::IsNotNull(WorkingLayouts::instance().Find(after, m_layout, base));
        }

        TEST_METHOD (DifferentLayoutIdDiscardsEntry)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });
            WorkingLayouts::instance().GetOrCreate(id, m_layout, base).MoveEdge({ 0 }, Side::Right, 6000, 500, 0);

            Assert::IsNull(WorkingLayouts::instance().Find(id, m_otherLayout, base));
            // the stale entry is gone, even for the original layout id
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, base));
        }

        TEST_METHOD (DifferentBaseDiscardsEntry)
        {
            const auto id = Id(L"A", m_desktop);
            WorkingLayouts::instance().GetOrCreate(id, m_layout, MakeGrid({ 5000, 5000 })).MoveEdge({ 0 }, Side::Right, 6000, 500, 0);

            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, MakeGrid({ 4000, 6000 })));
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, MakeGrid({ 5000, 5000 })));
        }

        TEST_METHOD (GetOrCreateReplacesStaleEntry)
        {
            const auto id = Id(L"A", m_desktop);
            WorkingLayouts::instance().GetOrCreate(id, m_layout, MakeGrid({ 5000, 5000 })).MoveEdge({ 0 }, Side::Right, 6000, 500, 0);

            auto& fresh = WorkingLayouts::instance().GetOrCreate(id, m_otherLayout, MakeGrid({ 3000, 3000, 4000 }));
            Assert::IsFalse(fresh.IsModified());
            Assert::AreEqual(size_t{ 3 }, fresh.Current().size());
        }

        TEST_METHOD (RemoveAndClear)
        {
            const auto id = Id(L"A", m_desktop);
            const auto base = MakeGrid({ 5000, 5000 });
            WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            WorkingLayouts::instance().Remove(id);
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, base));

            WorkingLayouts::instance().GetOrCreate(id, m_layout, base);
            WorkingLayouts::instance().Clear();
            Assert::IsNull(WorkingLayouts::instance().Find(id, m_layout, base));
        }
    };
}