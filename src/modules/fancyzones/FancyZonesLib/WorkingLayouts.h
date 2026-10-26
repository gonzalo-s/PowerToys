#pragma once

#include <map>
#include <string>

#include <guiddef.h>

#include <FancyZonesLib/DividerModel.h>
#include <FancyZonesLib/FancyZonesDataTypes.h>

// In-memory, per work area (monitor + virtual desktop) store of modified grid layouts.
// Entries are never written to disk. They outlive WorkArea objects, which are recreated on display changes.
// Used only from the FancyZones main (message-loop) thread; not thread-safe.
class WorkingLayouts
{
public:
    static WorkingLayouts& instance();

    // Returns the model for the work area if it exists and still belongs to this layout and base grid.
    // A stale entry (different layout id or base grid) is erased and nullptr is returned.
    FancyZonesDividers::DividerModel* Find(const FancyZonesDataTypes::WorkAreaId& id, const GUID& layoutId, const FancyZonesDataTypes::GridLayoutInfo& base);

    // Returns the matching model, or replaces a stale/missing entry with a fresh unmodified one.
    FancyZonesDividers::DividerModel& GetOrCreate(const FancyZonesDataTypes::WorkAreaId& id, const GUID& layoutId, const FancyZonesDataTypes::GridLayoutInfo& base);

    void Remove(const FancyZonesDataTypes::WorkAreaId& id);
    void Clear();

private:
    struct Entry
    {
        GUID layoutId;
        FancyZonesDividers::DividerModel model;
    };

    WorkingLayouts() = default;

    std::map<std::wstring, Entry> m_entries;
};