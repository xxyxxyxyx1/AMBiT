#ifndef ATOM_H
#define ATOM_H

#include "Atom/GetPot"
#include "Basis/ExcitedStates.h"
#include "Configuration/Configuration.h"
#include "Configuration/CIIntegrals.h"
#include "Configuration/CIIntegralsMBPT.h"
#include "Configuration/HamiltonianMatrix.h"
#include "Configuration/Eigenstates.h"
#include "Configuration/Solution.h"
#include "HartreeFock/Core.h"
#include "Universal/MathConstant.h"
#include "Universal/PhysicalConstant.h"

#include <map>

class SolutionMap;
class Sigma3Calculator;

class Atom
{
public:
    Atom(GetPot userInput, unsigned int atomic_number, int num_electrons, const std::string& atom_identifier);

    // Calculate or read Hartree-Fock and set up basis. Call single or multiple electron routines.
    bool Run();

public:
    Atom(unsigned int atomic_number, int charge, const std::string& atom_identifier, bool read = false);
    ~Atom();

    /** Identifier is only really used to identify this atom in filenames for storage.
        Should be altered when one of the physical parameters (eg nuclear) changes.
      */
    const std::string& GetIdentifier() { return identifier; }
    void SetIdentifier(const std::string& atom_identifier) { identifier = atom_identifier; }

    /** Write the atomic state to file, including all known electron wavefunctions. */
    void Write() const;

    /** Attempt to read core. Return success.
      */
    bool ReadCore();

    /** Read should be called immediately after initialisation of the atom, in order that
        initial Hartree-Fock values are not calculated (just to save computational time).
      */
    bool Read();

    /** Read core and excited states if core file exists, otherwise write them. */
    void ReadOrWriteBasis();

protected:
    // Main structure calculation routines
    void RunSingleElectron();
    void RunMultipleElectron();

public:
    /** Create a virtual basis, which is discrete yet takes into account parts of the continuum. */
    void CreateRBasis(bool UseMBPT = false);
    void CreateBSplineBasis(bool UseMBPT = false);
    void CreateCustomBasis(bool UseMBPT = false);
    void CreateHartreeFockBasis(bool UseMBPT = false);
    void CreateReadBasis(bool UseMBPT = false);

public:
    /** Generate integrals with MBPT. CIIntegralsMBPT will automatically store them,
        each processor with a separate file.
        Optionally include delta (cm-1). (eg: delta = E_CI - E_HF for the ground state)
     */
    void InitialiseIntegralsMBPT(bool CoreMBPT = true, bool ValenceMBPT = false);

    /** Collate integrals generated from CIIntegralsMBPT.
        num_processors is the number of stored files to collate.
        If num_processors = 0, it will assume "NumProcessors" (i.e. all processors).
     */
    void CollateIntegralsMBPT(unsigned int num_processors = 0);

    /** Read stored integrals and calculate any remaining without MBPT. */
    void GenerateIntegrals();

    /** Fill symmetries in Symmetry-Eigenstates map.*/
    void ChooseSymmetries();
    
    /** Generate configurations, projections, and JStates for a symmetry, and store on disk.
        If(save_configurations)
          - checks to see if the configurations exist on file already
          - saves them to disk if not
     */
    ConfigGenerator* GenerateConfigurations(const Symmetry& sym);

    /** Check sizes of matrices before doing full scale calculation. */
    void CheckMatrixSizes();

    /** Calculate energies for all chosen symmetries.
        PRE: Need to have generated all integrals.
     */
    void CalculateEnergies();

public:
    /** Initialise multiple run index. */
    void InitialiseRunIndex();

    /** Total number of runs selected for this calculation. */
    unsigned int NumberRunsSelected();

    /** Manipulate run index for multiple runs. */
    void RunIndexBegin(bool print = true);
    void RunIndexNext(bool print = true);
    bool RunIndexAtEnd();

    void InitialiseParameters(bool print = true);
    void SetRunParameters(bool print = true);
    void SetRunCore(bool force = false);
    void SetRunIntegrals(bool force = false);

    /** Run atomic energy code multiple times, varying some parameter like
        alpha, NuclearInverseMass (for SMS), or volume shift parameter.
     */
    void RunMultiple(bool include_mbpt = false, bool closed_shell = false);

    /** Set up core, excited states, and integrals some value of the varying parameter.
        PRE: Need to have initialised CIIntegrals* integrals, core and excited states.
     */
    void SetMultipleIntegralsAndCore(unsigned int index);

    /** Read stored integrals and calculate any remaining without MBPT.
        The MBPT_CI flag tells it whether to use CIIntegralsMBPT or not (CIIntegralsMBPT stores around
        twice as many integrals because of the loss of symmetry due to box diagrams).
     */
    void GenerateMultipleIntegrals(bool MBPT_CI);

    /** Calculate energies for all chosen symmetries. Integrals are generated as needed. */
    void CalculateMultipleEnergies();

    void CalculateMultipleClosedShell(bool include_mbpt);

public:
    /** Get previously calculated Eigenstates of Symmetry sym. */
    Eigenstates* GetEigenstates(const Symmetry& sym);
    inline const SymmetryEigenstatesMap* GetSymmetryEigenstatesMap() const
    { return &symEigenstates;
    }
    
    bool RestoreAllEigenstates();

    /** Get the set of atomic core orbitals. */
    Core* GetCore() { return core; }

    /** Get the set of valence orbitals. */
    ExcitedStates* GetBasis() { return excited; }
    
    /** Get the lattice */
    Lattice* GetLattice() { return lattice; }

    CIIntegrals* GetCIIntegrals() { return integrals; }

    SolutionMap* GetSolutionMap() { return mSolutionMap; }
    void WriteEigenstatesToSolutionMap();
    
    StateIterator GetIteratorToNextOrbitalToFill();
    std::string GetNextConfigString();
    
    unsigned int GetCurrentRunIndex() { return current_run_index; }
    std::vector<unsigned int> GetCurrentRunSelection() { return current_run_selection; }

public:
    void GenerateCowanInputFile();
    void PrintWavefunctionCowan(FILE* fp, pOrbitalConst ds);
    bool ReadGraspMCDF(const std::string& filename);
    void WriteGraspMCDF() const;
    void WriteGraspMcdfOrbital(FILE* fp, pOrbitalConst ds, unsigned int lattice_size) const;

protected:
    /** Parse basis string definition (e.g. 8spd5f) and convert to vector. */
    bool ParseBasisSize(const char* basis_def, std::vector<unsigned int>& num_states);

    /** Assumes excited object has been created (and excited_mbpt, if UseMBPT is true). */
    void CreateBasis(bool UseMBPT);

protected:
    GetPot userInput_;

    std::string identifier;
    double Z;         // Nuclear charge
    double Charge;    // Degree of ionisation

    unsigned int numValenceElectrons_;

    Lattice* lattice;
    Core* core;
    ExcitedStates* excited;
    ExcitedStates* excited_mbpt;
    CIIntegrals* integrals;
    CIIntegralsMBPT* integralsMBPT;
    CoreMBPTCalculator* mbpt;
    ValenceCalculator* valence_mbpt;
    Sigma3Calculator* sigma3;

    SymmetryEigenstatesMap symEigenstates;
    SolutionMap* mSolutionMap;

    // Operational parameters
    bool useRead;     // Read when possible from file
    bool useWrite;    // Write when possible

    // CI + MBPT parameters
    unsigned int NumSolutions;
    double MaxEnergy;
    std::string mbptBasisString;    // User input string
    std::string ciBasisString;      // User input string
    bool check_size_only;           // Check integral and CI sizes
    bool save_eigenstates;          // Save final energy eigenstates
    bool generate_mbpt_integrals;   // Generate MBPT integrals on this run
    // Which MBPT bits to include
    bool includeSigma1;
    bool includeSigma2;
    bool includeSigma3;

    // Multiple run parameters
    unsigned int multiple_length;
    std::vector<unsigned int> current_run_selection;
    unsigned int current_run_index;

    std::string original_id;
    std::vector<double> multiple_SMS;
    std::vector<double> multiple_alpha;
    std::vector<double> multiple_volume;
    std::vector<double> multiple_radius;
    std::vector<double> mbpt_delta;

    std::vector<double>* multiple_parameters;
};

#endif
