#ifdef _MPI
#include <mpi.h>
#endif
#include "Include.h"
#include "OutStreams.h"
#include "Atom.h"
#include "RateCalculator.h"
#include "Universal/Enums.h"

#include <Atom/GetPot>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _MPI
    #ifdef _SCALAPACK
    #if !(_FUS)
        #define blacs_exit_ blacs_exit
    #endif
    extern "C"{
    void blacs_exit_(const int*);
    }
    #endif
#endif

// MPI details (if not used, we can have NumProcessors == 1)
unsigned int NumProcessors;
unsigned int ProcessorRank;

// The debug options for the whole program.
Debug DebugOptions;

void PrintHelp(const std::string& ApplicationName);

int main(int argc, char* argv[])
{
    #ifdef _MPI
        MPI::Init(argc, argv);
        MPI::Intracomm& comm_world = MPI::COMM_WORLD; // Alias
        NumProcessors = comm_world.Get_size();
        ProcessorRank = comm_world.Get_rank();
    #else
        NumProcessors = 1;
        ProcessorRank = 0;
    #endif

    OutStreams::InitialiseStreams();

    try
    {
        GetPot lineInput(argc, argv, ",");

        // Check for help message
        if(lineInput.size() == 1 || lineInput.search(2, "--help", "-h"))
            PrintHelp(lineInput[0]);

        // Find .input file
        std::string inputFileName = "";
        const std::string extension = ".input";

        if(lineInput.search("-f"))
        {   inputFileName = lineInput.next("");
            if(inputFileName == "")
                PrintHelp(lineInput[0]);
        }
        else
        {   // Else look for .input arguments
            bool foundFile = false;
            bool endOfInput = false;
            while(!foundFile && !endOfInput)
            {
                inputFileName = lineInput.next_nominus();
                if(inputFileName == "")
                    endOfInput = true;
                else if(inputFileName.find(extension) != std::string::npos)
                    foundFile = true;
            }
            
            if(endOfInput && !lineInput.search("--recursive-build"))
            {   *errstream << "Input file not found" << std::endl;
                PrintHelp(lineInput[0]);
            }
        }

        GetPot fileInput(inputFileName.c_str(), "//", "\n", ",");
        fileInput.absorb(lineInput);

        if(fileInput.search("--recursive-build"))
        {
            int Z = fileInput("Z", 0);
            std::ofstream outfile;
            outfile.open("temp.out", std::ios_base::trunc);
            if(!outfile.is_open())
            {
                *errstream << "Error: Could not open file 'temp.out' for use in recursive building." << std::endl;
                exit(1);
            }
            std::string configstring = fileInput("HF/Configuration", "1s1:");
            outfile << "ID = " << fileInput("ID", "") << "N" << fileInput("N", 1) << std::endl << "Z = " << Z << std::endl << "N = " << fileInput("N", 1) << std::endl << "NumValenceElectrons = " << 1 << std::endl;
            outfile << "-c" << std::endl << "-d" << std::endl;
            outfile << "--recursive-build" << std::endl << std::endl;
            outfile << "NuclearRadius = " << fileInput("NuclearRadius", 0) << std::endl << "NuclearThickness = " << fileInput("NuclearThickness", 0) << std::endl << std::endl;
            outfile << "[Lattice]" << std::endl << "NumPoints = " << fileInput("Lattice/NumPoints", 1000) << std::endl;
            outfile << "StartPoint = " << fileInput("Lattice/StartPoint", 1.e-6) << std::endl << "EndPoint = " << fileInput("Lattice/EndPoint", 50.) << std::endl << std::endl;
            outfile << "[HF]" << std::endl << "Configuration = '" << configstring << "'" << std::endl << std::endl;
            outfile << "[Basis]" << std::endl << "--bspline-basis" << std::endl << "ValenceBasis = " << fileInput("Basis/ValenceBasis", "4spdf") << std::endl << std::endl;
            outfile << "BSpline/N = " << fileInput("Basis/BSpline/N", 40) << std::endl << "BSpline/K = " << fileInput("Basis/BSpline/K", 7) << std::endl;
            outfile << "BSpline/Rmax = " << fileInput("Basis/BSpline/Rmax", 50.) << std::endl << std::endl;
            outfile.close();
            for(int i = fileInput("N", 1) - 1; i < Z; i++)
            {
                GetPot tempInput("temp.out", "//", "\n", ",");
                Atom* A;
                A = new Atom(tempInput, Z, tempInput("N", 1), tempInput("ID", ""));
                A->Run();

                if(i + 2 <= Z)
                {
                    configstring = A->GetNextConfigString();
                    *outstream << "+ " <<A->GetIteratorToNextOrbitalToFill().GetState()->Name() << " = " <<  std::endl;
                    outfile.open("temp.out", std::ios_base::trunc);
                    outfile << "ID = " << fileInput("ID", "") << "N" << i + 2 << std::endl << "Z = " << Z << std::endl << "N = " << i + 2 << std::endl << "NumValenceElectrons = " << 1 << std::endl;
                    outfile << "-c" << std::endl << "-d" << std::endl;
                    outfile << "--recursive-build" << std::endl << std::endl;
                    outfile << "NuclearRadius = " << fileInput("NuclearRadius", 0) << std::endl << "NuclearThickness = " << fileInput("NuclearThickness", 0) << std::endl << std::endl;
                    outfile << "[Lattice]" << std::endl << "NumPoints = " << fileInput("Lattice/NumPoints", 1000) << std::endl;
                    outfile << "StartPoint = " << fileInput("Lattice/StartPoint", 1.e-6) << std::endl << "EndPoint = " << fileInput("Lattice/EndPoint", 50.) << std::endl << std::endl;
                    outfile << "[HF]" << std::endl << "Configuration = '" << configstring << "'" << std::endl << std::endl;
                    outfile << "[Basis]" << std::endl << "--bspline-basis" << std::endl << "ValenceBasis = " << fileInput("Basis/ValenceBasis", "4spdf") << std::endl << std::endl;
                    outfile << "BSpline/N = " << fileInput("Basis/BSpline/N", 40) << std::endl << "BSpline/K = " << fileInput("Basis/BSpline/K", 7) << std::endl;
                    outfile << "BSpline/Rmax = " << fileInput("Basis/BSpline/Rmax", 50.) << std::endl << std::endl;
                    outfile.close();
                }
                delete A;
            }
            
            *outstream << std::endl << std::endl << "Final configuration: " << configstring;
        }
        else
        {
            // Must-have arguments. This also checks on the file's existence.
            int Z, ZIterations, N, Charge;
            Z = fileInput("Z", 0);
            N = fileInput("HF/N", -1);  // Number of electrons
            ZIterations = fileInput("ZIterations", 1);
            if(Z != 0 && N == -1)
            {   Charge = fileInput("HF/Charge", -1);
                N = Z - Charge;
            }
            if(Z == 0 || N == -1 || N > Z)
            {   *errstream << inputFileName << " must specify Z, and either N (number of electrons) or Charge.\n"
                           << "    Furthermore Z >= N." << std::endl;
                exit(1);
            }
    
            // Identifier
            std::string identifier = fileInput("ID", "");
            if(identifier == "")
            {
                if(inputFileName.find(extension) != std::string::npos)
                    identifier = inputFileName.substr(0, inputFileName.find(extension));
                else
                {   *errstream << "No ID could be found." << std::endl;
                    exit(1);
                }
            }
    
            if(ZIterations > 1 && fileInput("NumValenceElectrons", 1) > 1)
            {
                *errstream << "Configuration interaction for multiple atoms with differing charge is not supported yet." << std::endl;
                exit(1);
            }
    
            // That's all we need to start the Atom class
            Atom A(fileInput, Z, N, identifier);
            A.Run();
            if(ZIterations > 1)
            {
                int ZCounter = 1;
                while(ZCounter < ZIterations) {
                    Atom aA(fileInput, Z + ZCounter, N, identifier);
                    aA.Run();
                    ZCounter++;
                }
            }
    
            // At this stage, we are ready to run calculations with the
            // calculated energy levels
    
            A.WriteEigenstatesToSolutionMap();
            // Print the solutions currently available after CI calculation
            DisplayOutputType::Enum OutputType;
            std::string TypeString = fileInput("CI/Output/Type", "");
            if(TypeString == "Standard")
            {
                OutputType = DisplayOutputType::Standard;
            }
            else if(TypeString == "CommaSeparated")
            {
                OutputType = DisplayOutputType::CommaSeparated;
            }
            else if(TypeString == "SpaceSeparated")
            {
                OutputType = DisplayOutputType::SpaceSeparated;
            }
            else if(TypeString == "TabSeparated")
            {
                OutputType = DisplayOutputType::TabSeparated;
            }
    
            if(TypeString != "" && fileInput("NumValenceElectrons", 1) > 1)
            {
                A.GetSolutionMap()->Print(OutputType);
            }
    
            A.GetSolutionMap()->PrintID();
    
            // Interactive mode
            if(fileInput.search("--interactive"))
            {
                std::string input;
                while(input != "quit" && input != "q")
                {
                    std::cin >> input;
                    if(input != "quit" && input != "q")
		    {
                        if(strcspn(input.c_str(), "eo") > 0 && strlen(input.c_str()) > strcspn(input.c_str(), "eo") + 1 && strchr(input.c_str(), '>') == NULL)
			{
			    if(A.GetSolutionMap()->FindByIdentifier(input) != A.GetSolutionMap()->end())
			    {
			        A.GetSolutionMap()->PrintSolution(A.GetSolutionMap()->FindByIdentifier(input));
			    }
			    else
			    {
			        *outstream << "Level " << input << " not found." << std::endl;
			    }
			}
			else if(strchr(input.c_str(), '>') != NULL)
			{
			    char input_cstring[strlen(input.c_str()) + 1];
			    strcpy(input_cstring, input.c_str());
			    char* token1 = strtok(input_cstring, " ->");
			    char* token2 = strtok(NULL, " ->");
			    A.GetSolutionMap()->FindByIdentifier(token1)->second.GetTransitionSet()->insert(Transition(&A, TransitionType(MultipolarityType::E, 1), A.GetSolutionMap()->FindByIdentifier(token1)->first, A.GetSolutionMap()->FindByIdentifier(token2)->first));
			}
		    }
                }
            }
        }
        
        // Transition calculations should be performed here!
        //A.GetSolutionMap()->FindByIdentifier("0e0")->second.GetTransitionSet()->insert(Transition(&A, TransitionType(MultipolarityType::E, 1), A.GetSolutionMap()->FindByIdentifier("0e0")->first, A.GetSolutionMap()->FindByIdentifier("2o0")->first));
        //A.GetSolutionMap()->FindByIdentifier("0e0")->second.GetTransitionSet()->insert(Transition(&A, TransitionType(MultipolarityType::E, 1), A.GetSolutionMap()->FindByIdentifier("0e0")->first, A.GetSolutionMap()->FindByIdentifier("2o1")->first));

//        RateCalculator RC(A.GetBasis());
//        RC.CalculateAllDipoleStrengths(&A, Symmetry(6, odd), 0, false, true);
//        *outstream << std::endl;
//        RC.CalculateAllDipoleStrengths(&A, Symmetry(6, odd), 1, false, true);
//        *outstream << std::endl;
//        RC.CalculateAllDipoleStrengths(&A, Symmetry(8, odd), 0, false, true);
//        *outstream << std::endl;
    }
    catch(std::bad_alloc& ba)
    {   *errstream << ba.what() << std::endl;
        exit(1);
    }

    #ifdef _MPI
        comm_world.Barrier();
        #ifdef _SCALAPACK
            // Continue should be non-zero, otherwise MPI_Finalise is called.
            int cont = 1;
            blacs_exit_(&cont);
        #endif

        MPI::Finalize();
    #endif

    *outstream << "\nFinished" << std::endl;
    OutStreams::FinaliseStreams();

    PAUSE
    return 0;
}

void PrintHelp(const std::string& ApplicationName)
{
    *outstream << std::endl;
    *outstream << ApplicationName << " Usage Message" << std::endl;
    exit(0);
}
