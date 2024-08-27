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
#include <boost/filesystem.hpp>

#define STATIC_DIR "static_calc"
#define OPT_DIR "opt_calc"
#define BAND_DIR "band_calc"
#define DIELECTRIC_DIR "dielectric_calc"

// File names
#define POSCAR "POSCAR"
#define INCAR "INCAR"
#define POTCAR "POTCAR"
#define KPOINTS "KPOINTS"
#define CONTCAR "CONTCAR"
#define WAVECAR "WAVECAR"
#define CHGCAR "CHGCAR"

namespace fs = boost::filesystem;

class Vasp
{
public:
    Vasp(const std::string &rootDir) : rootDir(rootDir)
    {
        if (chdir(rootDir.c_str()) != 0)
        {
            std::cerr << "Error: Failed to change directory." << std::endl;
            exit(EXIT_FAILURE);
        }
        computeDir = "";
        optDir = "";
        staticDir = "";
        bandDir = "";
    }

    // prepareDirectory() 函数用于为每次 VASP 计算准备一个新的目录
    std::string prepareDirectory(const std::string &computeTask);
    int generateKPOINTS();
    void generateINCAR(const std::string &INCAROptions, bool isSCF, int dimension);
    void generateInputFiles(const std::string &poscarPath);
    void performStructureOptimization();
    void performStaticCalculation();
    void performDielectricCalculation();
    void performBandStructureCalculation();

private:
    fs::path rootDir;       // 整个计算的根目录
    fs::path computeDir;    // 一次完整vasp计算的目录
    fs::path optDir;        // 结构优化计算的目录
    fs::path staticDir;     // 静态计算的目录
    fs::path bandDir;       // 带结构计算的目录
    fs::path dielectricDir; // 介电常数计算的目录
};