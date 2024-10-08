#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <time.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#include "vasp_tool.h"
#include <vector>

namespace fs = boost::filesystem;

class Vasp
{
public:
    Vasp(const std::string &rootDir, const std::string &dataDir) : root_dir_(rootDir), data_dir_(dataDir)
    {
        if (chdir(rootDir.c_str()) != 0)
        {
            std::cerr << "Error: Failed to change directory." << std::endl;
            exit(EXIT_FAILURE);
        }
        results_.emplace(BANDGAP, "");
        results_.emplace(DIELECTRIC, "");
        results_.emplace(THERMAL_CONDUCTIVITY, "");
        results_.emplace(THERMAL_EXPANSION, "");
        results_.emplace(CONDUCTIVITY, "");
        results_.emplace(MOBILITY, "");
    }

    // PrepareDirectory() 函数用于为每次 VASP 计算准备一个新的目录
    std::string PrepareDirectory(const std::string &computeTask);
    int GenerateKPOINTS();
    void GenerateINCAR(const std::string &INCAROptions, bool isSCF, int dimension);
    void GenerateInputFiles(const std::string &poscarPath);
    void PerformStructureOptimization();
    void PerformStaticCalculation();
    void PerformDielectricCalculation();
    void PerformBandStructureCalculation();
    void PerformThermalExpansionCalculation();
    void PerformConductivityCalculation();
    void UseHistoryOptDir();
    void GetDensity();
    std::vector<std::string>  StoreResults();

private:
    fs::path data_dir_;             // 数据目录
    fs::path root_dir_;             // 整个计算的根目录
    fs::path compute_dir_;          // 一次完整vasp计算的目录
    fs::path opt_dir_;              // 结构优化计算的目录
    fs::path static_dir_;           // 静态计算的目录
    fs::path band_dir_;             // 带结构计算的目录
    fs::path scf_dir_;              // 自洽计算的目录
    fs::path dielectric_dir_;       // 介电常数计算的目录
    fs::path thermal_expansion_dir_; // 热膨胀计算的目录
    fs::path conductivity_dir_;    // 电导率计算的目录

    std::map<std::string, std::string> results_;
};