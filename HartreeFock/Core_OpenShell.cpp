#include "Include.h"
#include "Core.h"

void Core::Ionise(OrbitalInfo removed_electron)
{
    pOrbital from_state = GetState(removed_electron);
    if(from_state != NULL)
    {
        if(OpenShellStates.find(removed_electron) == OpenShellStates.end())
        {   OpenShellStates[removed_electron] = from_state->Occupancy();
        }
        if(from_state->Occupancy() > 1.)
            from_state->SetOccupancy(from_state->Occupancy() - 1.);
        else
        {   OpenShellStorage[removed_electron] = from_state;
            AllStates.erase(AllStates.find(removed_electron));
        }
        
        Charge++;
        UpdateHFPotential();
    }
}

void Core::ToggleClosedShellCore()
{
    std::map<OrbitalInfo, double>::iterator it = OpenShellStates.begin();
    while(it != OpenShellStates.end())
    {
        pOrbital ds = GetState(it->first);
        if(ds)
        {   StateSet::iterator kill_it = AllStates.find(it->first);
            if(kill_it != AllStates.end())
            {   Charge = Charge + ds->Occupancy();
                AllStates.erase(kill_it);
            }
            OpenShellStorage[it->first] = ds;
        }
        it++;
    }
    UpdateHFPotential();
}

void Core::ToggleOpenShellCore()
{
    std::map<OrbitalInfo, double>::iterator it = OpenShellStates.begin();
    while(it != OpenShellStates.end())
    {
        double new_occupancy = it->second;
        pOrbital ds;

        if(AllStates.find(it->first) == AllStates.end())
        {   Charge = Charge - new_occupancy;
            ds = OpenShellStorage[it->first];
            ds->SetOccupancy(new_occupancy);
            AddState(ds);
        }
        else
        {   ds = AllStates.find(it->first)->second;
            if(ds->Occupancy() != new_occupancy)
            {   Charge = Charge + ds->Occupancy() - new_occupancy;
                ds->SetOccupancy(new_occupancy);
            }
        }
        it++;
    }
    UpdateHFPotential();
    OpenShellStorage.clear();
}

bool Core::IsOpenShellState(const OrbitalInfo& info) const
{
    return (OpenShellStates.find(info) != OpenShellStates.end());
}

bool Core::IsOpenShellCore() const
{
    return (OpenShellStates.size() != 0);
}

void Core::SetOpenShellState(const OrbitalInfo& info, double occupancy)
{
    OpenShellStates[info] = occupancy;
}
