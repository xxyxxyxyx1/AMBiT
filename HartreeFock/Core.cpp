#include "Include.h"
#include "Core.h"
#include "Universal/Constant.h"
#include "Universal/CoulombIntegrator.h"

Core::Core(Lattice* lat, unsigned int atomic_number, int ion_charge):
    StateManager(lat, atomic_number, ion_charge),
    NuclearRadius(0.00001), NuclearThickness(0.000001),
    NuclearInverseMass(0.0), VolumeShiftParameter(0.0), Polarisability(0.0)
{}

void Core::Initialise()
{
    Clear();
    BuildFirstApproximation();

    // Some default values for the nuclear parameters.
    UpdateNuclearPotential();
    Update();
}

void Core::Write(FILE* fp) const
{
    fwrite(&NuclearRadius, sizeof(double), 1, fp);
    fwrite(&NuclearThickness, sizeof(double), 1, fp);

    unsigned int num_states = NumStates();

    // Output core
    fwrite(&num_states, sizeof(unsigned int), 1, fp);
    ConstStateIterator it = GetConstStateIterator();
    while(!it.AtEnd())
    {
        it.GetState()->Write(fp);
        it.Next();
    }
}

void Core::Read(FILE* fp)
{
    Clear();
    
    unsigned int num_core, i;

    fread(&NuclearRadius, sizeof(double), 1, fp);
    fread(&NuclearThickness, sizeof(double), 1, fp);

    // Read core
    fread(&num_core, sizeof(unsigned int), 1, fp);
    for(i = 0; i<num_core; i++)
    {
        DiscreteState* ds = new DiscreteState(lattice);
        ds->Read(fp);
        AddState(ds);
    }

    UpdateNuclearPotential();
    Update();
}

void Core::CalculateVolumeShiftPotential(double radius_difference)
{
    std::vector<double> new_density = CalculateNuclearDensity(NuclearRadius + radius_difference, NuclearThickness);
    std::vector<double> new_potential;
    CoulombIntegrator I(*lattice);
    I.CoulombIntegrate(new_density, new_potential, 0, Z);

    unsigned int oldsize = NuclearPotential.size(),
                 newsize = new_potential.size();
    unsigned int i = 0;
    VolumeShiftPotential.resize(mmax(newsize, oldsize));
    while(i < mmin(newsize, oldsize))
    {   VolumeShiftPotential[i] = (new_potential[i] - NuclearPotential[i]);
        i++;
    }
    while(i < newsize)
    {   VolumeShiftPotential[i] = new_potential[i] - Z/lattice->R(i);
        i++;
    }
    while(i < oldsize)
    {   VolumeShiftPotential[i] = Z/lattice->R(i) - NuclearPotential[i];
        i++;
    }

    if(VolumeShiftParameter)
        Update();
}

std::vector<double> Core::GetHFPotential() const
{
    return HFPotential;
}

const std::vector<double>& Core::GetConstHFPotential() const
{
    return HFPotential;
}

std::vector<double> Core::GetLocalExchangeApproximation() const
{
    return LocalExchangeApproximation;
}

unsigned int Core::UpdateExcitedState(State* s, const SigmaPotential* sigma, double sigma_amount) const
{
    DiscreteState* ds = dynamic_cast<DiscreteState*>(s);
    if(ds != NULL)
    {
        // Number of iterations required. Zero shows that the state existed previously.
        unsigned int loop = 0;

        const DiscreteState* core_state = GetState(StateInfo(ds));
        StateSet::const_iterator it = OpenShellStorage.find(StateInfo(ds));
        if(core_state != NULL)
        {   // Try to find in core (probably open shells).
            *ds = *core_state;
        }
        else if(it != OpenShellStorage.end())
        {   // Try to find in unoccupied (but previously calculated) open shells.
            *ds = *(it->second.GetState());
        }
        else
        {   // Hartree-Fock loops
            bool debugHF = DebugOptions.LogHFIterations();

            double deltaE;
            DiscreteState* new_ds = new DiscreteState(*ds);
            double prop_new = 0.5;
            do
            {   loop++;
                CoupledFunction exchange;
                CalculateExchange(*new_ds, exchange, sigma, sigma_amount);
                deltaE = IterateDiscreteStateGreens(new_ds, &exchange);

                if(debugHF)
                    *logstream << "  " << std::setw(4) << ds->Name() 
                            << "  E = " << std::setprecision(12) << ds->Energy()
                            << "  deltaE = " << std::setprecision(3) << deltaE
                            << "  size: (" << new_ds->Size()
                            << ") " << lattice->R(new_ds->Size()) << std::endl;

                ds->Scale(1. - prop_new);
                new_ds->Scale(prop_new);
                ds->ReSize(mmax(ds->Size(), new_ds->Size()));
                new_ds->ReSize(mmax(ds->Size(), new_ds->Size()));

                for(unsigned int i = 0; i<ds->Size(); i++)
                {   ds->f[i] += new_ds->f[i];
                    ds->g[i] += new_ds->g[i];
                    ds->df[i] += new_ds->df[i];
                    ds->dg[i] += new_ds->dg[i];
                }

                // Renormalise core states (should be close already) and update energy.
                ds->ReNormalise();
                ds->SetEnergy((1. - prop_new) * ds->Energy() + prop_new * new_ds->Energy());
                *new_ds = *ds;
                deltaE = fabs(deltaE/new_ds->Energy());

            }while((deltaE > StateParameters::EnergyTolerance) && (loop < StateParameters::MaxHFIterations));

            delete new_ds;

            if(loop >= StateParameters::MaxHFIterations)
                *errstream << "Core: Failed to converge excited HF state " << ds->Name() << std::endl;

            if(DebugOptions.OutputHFExcited())
            {
                *outstream << std::setprecision(12);
                if(DebugOptions.HartreeEnergyUnits() || DebugOptions.InvCmEnergyUnits())
                {
                    double energy = ds->Energy();
                    if(DebugOptions.InvCmEnergyUnits())
                        energy *= Constant::HartreeEnergy_cm;
                    *outstream << ds->Name() << "  E = " << energy << "  loops: " << loop << "  size: " << ds->Size() << std::endl;
                }
                else
                    *outstream << ds->Name() << "  nu = " << ds->Nu() << "  loops: " << loop << "  size: " << ds->Size() << std::endl;
            }
        }
        return loop;
    }
    else
    {   ContinuumState* cs = dynamic_cast<ContinuumState*>(s);
        return CalculateContinuumState(cs);
    }
}
