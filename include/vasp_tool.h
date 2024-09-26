#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <time.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <sstream>
#include "vaspkit.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

#define STATIC_DIR "static_calc"
#define OPT_DIR "opt_calc"
#define SCF_DIR "scf_calc"
#define BAND_DIR "band_calc"
#define DIELECTRIC_DIR "dielectric_calc"
#define THERMAL_EXPANSION_DIR "thermal_expansion_calc"
#define CONDUCTIVITY_DIR "conductivity_calc"


#define CONFIG_DIR "config"
#define SCRIPT_DIR "scripts"

// File names
#define POSCAR "POSCAR"
#define INCAR "INCAR"
#define POTCAR "POTCAR"
#define KPOINTS "KPOINTS"
#define CONTCAR "CONTCAR"
#define WAVECAR "WAVECAR"
#define CHGCAR "CHGCAR"
#define OUTCAR "OUTCAR"

// output name
#define DENSITY "DENSITY"
#define DIELECTRIC "DIELECTRIC"
#define THERMAL_EXPANSION "THERMAL_EXPANSION"
#define THERMAL_CONDUCTIVITY "THERMAL_CONDUCTIVITY"
#define SPECIFIC_HEAT "SPECIFIC_HEAT"
#define CONDUCTIVITY "CONDUCTIVITY"
#define MOBILITY "MOBILITY"
#define BANDGAP "BANDGAP"


static std::map<std::string, std::string> units = {
    {DENSITY, "KG/m^3"},
    {DIELECTRIC, ""},
    {THERMAL_EXPANSION, "1/K"},
    {THERMAL_CONDUCTIVITY, "W/mK"},
    {CONDUCTIVITY, "S/m"},
    {MOBILITY, "cm^2/Vs"},
    {BANDGAP, "eV"},
    {SPECIFIC_HEAT, "J/kgK"}
    };


////////////////////////// Utility Functions //////////////////////////

std::string ExtractKmeshValue(const std::string &filename);

std::string ExtractKSpacingValue(const std::string &filename);

void ModifyINCAR(fs::path filepath, const std::map<std::string, std::string> &options);

// Function to read band.dat file and extract k-points and energies
void ReadBandDat(const std::string &file_path, std::vector<std::vector<double>> &energies);

// Function to calculate the band gap, VBM, and CBM
void CalculateBandgap(const std::vector<std::vector<double>> &energies, double &bandgap, double &VBM, double &CBM);

struct ConductivityData
{
    double energy;
    double average_conductivity;
};

struct CarrierConcentrationData
{
    double energy;
    double concentration; // in 1/cm^3
};


std::vector<ConductivityData> readElectronicConductivity(const std::string &file_path);

std::vector<CarrierConcentrationData> readCarrierConcentration(const std::string &file_path);