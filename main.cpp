#include "vasp.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_POSCAR>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string cwd = get_current_dir_name();
    Vasp vasp(cwd);

    std::string poscarPath = argv[1];

    // 第二个参数，是否使用历史优化目录
    if (argc > 2)
    {
        if (std::string(argv[2]) == "history")
        {
            vasp.useHistoryOptDir();
        }
    }
    else
    {
        vasp.prepareDirectory();
        std::cout << "Generating input files..." << std::endl;
        vasp.generateInputFiles(poscarPath);
    }

    // std::cout << "Performing structure optimization..." << std::endl;
    // vasp.performStructureOptimization();

    // std::cout << "Performing static calculation..." << std::endl;
    // vasp.performStaticCalculation();

    // std::cout << "Performing dielectric calculation..." << std::endl;
    // vasp.performDielectricCalculation();

    // std::cout << "Performing band structure calculation..." << std::endl;
    // vasp.performBandStructureCalculation();

    std::cout << "Performing thermal expansion calculation..." << std::endl;
    vasp.performThermalExpansionCalculation();

    std::cout << "VASP calculation complete." << std::endl;

    return EXIT_SUCCESS;
}
