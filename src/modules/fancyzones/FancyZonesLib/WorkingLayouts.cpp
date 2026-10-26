#include "pch.h"
#include "WorkingLayouts.h"

using FancyZonesDataTypes::GridLayoutInfo;
using FancyZonesDataTypes::WorkAreaId;

WorkingLayouts& WorkingLayouts::instance()
{
    static WorkingLayouts self;
    return self;
}

FancyZonesDividers::DividerModel* WorkingLayouts::Find(const WorkAreaId& id, const GUID& layoutId, const GridLayoutInfo& base)
{
    const auto key = id.toString();
    auto it = m_entries.find(key);
    if (it == m_entries.end())
    {
        return nullptr;
    }

    // Template layouts share GUID_NULL, so the base grid comparison is what tells them apart.
    if (!(it->second.layoutId == layoutId) || !it->second.model.SameBase(base))
    {
        m_entries.erase(it);
        return nullptr;
    }

    return &it->second.model;
}

FancyZonesDividers::DividerModel& WorkingLayouts::GetOrCreate(const WorkAreaId& id, const GUID& layoutId, const GridLayoutInfo& base)
{
    if (auto* existing = Find(id, layoutId, base))
    {
        return *existing;
    }

    auto [it, inserted] = m_entries.insert_or_assign(id.toString(), Entry{ layoutId, FancyZonesDividers::DividerModel(base) });
    return it->second.model;
}

void WorkingLayouts::Remove(const WorkAreaId& id)
{
    m_entries.erase(id.toString());
}

void WorkingLayouts::Clear()
{
    m_entries.clear();
}