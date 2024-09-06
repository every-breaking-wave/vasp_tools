#include "vasp.h"

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cerr << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

std::map<std::string, std::string> units = {
    {DENSITY, "g/cm^3"},
    {DIELECTRIC, ""},
    {THERMAL_EXPANSION, "1/K"},
    {THERMAL_CONDUCTIVITY, "W/mK"},
    {CONDUCTIVITY, "S/m"},
    {MOBILITY, "cm^2/Vs"},
    {BANDGAP, "eV"}};

    
////////////////////////// Utility Functions //////////////////////////

void CopyFiles(const std::vector<fs::path> &source_files, const fs::path &destination_dir)
{
    try
    {
        if (!fs::exists(destination_dir))
        {
            fs::create_directory(destination_dir);
        }
        for (const auto &source_file : source_files)
        {
            if (fs::exists(source_file) && fs::is_regular_file(source_file))
            {
                fs::path dest_path = destination_dir / source_file.filename();
                fs::copy_file(source_file, dest_path, fs::copy_option::overwrite_if_exists);
            }
            else
            {
                std::cerr << "File " << source_file << " does not exist or is not a regular file." << std::endl;
            }
        }
    }
    catch (fs::filesystem_error &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void CopyAllFiles(const fs::path &source_dir, const fs::path &destination_dir)
{
    try
    {
        if (!fs::exists(destination_dir))
        {
            fs::create_directory(destination_dir);
        }
        for (fs::directory_iterator it(source_dir); it != fs::directory_iterator(); ++it)
        {
            if (fs::is_regular_file(it->path()))
            {
                fs::path dest_path = destination_dir / it->path().filename();
                fs::copy_file(it->path(), dest_path, fs::copy_option::overwrite_if_exists);
            }
        }
    }
    catch (fs::filesystem_error &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void RunCommand(const std::string &command)
{
    int result = system(command.c_str());
    if (result != 0)
    {
        std::cerr << "Error: Command failed -> " << command << std::endl;
        exit(EXIT_FAILURE);
    }
}

std::string ExtractKmeshValue(const std::string &filename)
{
    std::ifstream infile(filename);
    std::string line;
    std::string kmeshValue;

    while (std::getline(infile, line))
    {
        if (line.find("is Generally Precise Enough!") != std::string::npos)
        {
            std::istringstream iss(line);
            // 忽略前面的部分，提取 Kmesh-Resolved Value
            std::string token;
            while (iss >> token)
            {
                if (token.find('-') != std::string::npos)
                {
                    // 取 - 号前面的部分作为 Kmesh 值
                    kmeshValue = token.substr(0, token.find('-'));
                    break;
                }
            }
            break;
        }
    }

    return kmeshValue;
}

void ModifyINCAR(fs::path filepath, const std::map<std::string, std::string> &options)
{
    std::ifstream infile(filepath.string());
    std::stringstream buffer;
    std::string line;

    // Flag to track if a key has been modified
    std::map<std::string, bool> modifiedKeys;
    for (const auto &option : options)
    {
        modifiedKeys[option.first] = false;
    }

    // Read the file line by line
    while (std::getline(infile, line))
    {
        std::string key, value;
        std::size_t pos = line.find('=');

        if (pos != std::string::npos)
        {
            key = line.substr(0, pos);
            value = line.substr(pos + 1);

            // Trim whitespace around key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // If the key exists in options, modify it
            if (options.find(key) != options.end())
            {
                value = options.at(key);
                modifiedKeys[key] = true; // Mark as modified
            }

            buffer << key << " = " << value << std::endl;
        }
        else
        {
            buffer << line << std::endl; // Keep the line unchanged if it does not contain '='
        }
    }

    infile.close();

    // Append new key-value pairs that were not found in the file
    for (const auto &option : options)
    {
        if (!modifiedKeys[option.first])
        {
            buffer << option.first << " = " << option.second << std::endl;
        }
    }

    // Write the modified content back to the file
    std::ofstream outfile(filepath.string());
    outfile << buffer.str();
    outfile.close();
}

// Function to read band.dat file and extract k-points and energies
void ReadBandDat(const std::string &file_path, std::vector<std::vector<double>> &energies)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line[0] != '#')
        { // Skip empty lines and comments
            std::istringstream iss(line);
            double k_point;
            iss >> k_point;

            std::vector<double> energy_levels;
            double energy;
            while (iss >> energy)
            {
                energy_levels.push_back(energy);
            }
            energies.push_back(energy_levels);
        }
    }
    file.close();
}

// Function to calculate the band gap, VBM, and CBM
void CalculateBandgap(const std::vector<std::vector<double>> &energies, double &bandgap, double &VBM, double &CBM)
{
    std::vector<double> VBM_list;
    std::vector<double> CBM_list;

    for (const auto &energy : energies)
    {
        std::vector<double> VBM_candidates;
        std::vector<double> CBM_candidates;

        for (double band : energy)
        {
            if (band < 0)
            {
                VBM_candidates.push_back(band);
            }
            else if (band > 0)
            {
                CBM_candidates.push_back(band);
            }
        }

        if (!VBM_candidates.empty())
        {
            VBM_list.push_back(*std::max_element(VBM_candidates.begin(), VBM_candidates.end()));
        }
        if (!CBM_candidates.empty())
        {
            CBM_list.push_back(*std::min_element(CBM_candidates.begin(), CBM_candidates.end()));
        }
    }

    if (VBM_list.empty())
    {
        throw std::runtime_error("VBM not found: no energy values below 0 eV.");
    }
    if (CBM_list.empty())
    {
        throw std::runtime_error("CBM not found: no energy values above 0 eV.");
    }

    VBM = *std::max_element(VBM_list.begin(), VBM_list.end());
    CBM = *std::min_element(CBM_list.begin(), CBM_list.end());

    bandgap = CBM - VBM;
}

////////////////////////// Vasp Class//////////////////////////
std::string Vasp::PrepareDirectory(const std::string &computeTask = "")
{
    try
    {
        fs::current_path(root_dir_);
        std::string retDir;
        std::string timestamp = std::to_string(std::time(nullptr));
        if (computeTask.empty())
        {
            if (compute_dir_.empty())
            {
                compute_dir_ = "vasp_" + timestamp;
                DEBUG_PRINT("computeDir: " << compute_dir_);
                fs::create_directory(compute_dir_);
                compute_dir_ = root_dir_ / compute_dir_;
                retDir = compute_dir_.string();
            }
            else
            {
                throw std::runtime_error("Error: computeDir is not empty.");
            }
        }
        else
        {
            // 在现有的 computeDir 中创建子任务目录
            if (fs::is_directory(compute_dir_))
            {
                fs::path taskDir = fs::path(compute_dir_) / computeTask;
                if (!fs::is_directory(taskDir))
                {
                    std::cout << "Creating sub-directory: " << taskDir.string() << std::endl;
                    fs::create_directory(taskDir);
                }
                retDir = taskDir.string();
            }
            else
            {
                throw std::runtime_error("Error: computeDir is not a valid directory.");
            }
        }
        fs::current_path(compute_dir_);
        return retDir;
    }
    catch (const fs::filesystem_error &ex)
    {
        std::cerr << "Filesystem error: " << ex.what() << std::endl;
        return "";
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return "";
    }
}

int Vasp::GenerateKPOINTS()
{
    // 启动 VASPKIT 进程
    VaspkitManager &vaspkit = VaspkitManager::getInstance();

    vaspkit.startVaspkit((*compute_dir_.end()).string());

    vaspkit.sendInputToVaspkit("1\n");
    vaspkit.sendInputToVaspkit("102\n");
    vaspkit.sendInputToVaspkit("1\n");

    // 等待 VASPKIT 产生输出（模拟延迟）
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 从输出文件中提取 Kmesh 值
    std::string kmeshValue = ExtractKmeshValue(vaspkit.getOutputFilename());
    if (!kmeshValue.empty())
    {
        vaspkit.sendInputToVaspkit(kmeshValue + "\n");
    }
    else
    {
        std::cerr << "Error: Could not extract Kmesh value from VASPKIT output." << std::endl;
        vaspkit.stopVaspkit();
        return 1;
    }
    // 关闭 VASPKIT 进程
    vaspkit.stopVaspkit();
    return 0;
}

void Vasp::GenerateINCAR(const std::string &INCAROptions, bool isSCF = false, int dimension = 0)
{
    VaspkitManager &vaspkit = VaspkitManager::getInstance();
    vaspkit.startVaspkit((*compute_dir_.end()).string());
    if (isSCF) // SCF calculation(静态自洽计算)
    {
        switch (dimension)
        {
        case 1:
            vaspkit.sendInputToVaspkit("301\n");
            break;
        case 2:
            vaspkit.sendInputToVaspkit("302\n");
            break;
        case 3:
            vaspkit.sendInputToVaspkit("303\n");
            break;
        default:
            std::cerr << "Error: Invalid dimension for SCF calculation." << std::endl;
            break;
        }
    }
    else
    {
        vaspkit.sendInputToVaspkit("101\n");
        vaspkit.sendInputToVaspkit(INCAROptions + "\n");
    }

    // 等待 VASPKIT 产生输出（模拟延迟）
    std::this_thread::sleep_for(std::chrono::seconds(1));
    vaspkit.stopVaspkit();
}

void Vasp::GenerateInputFiles(const std::string &poscarPath)
{
    fs::current_path(compute_dir_);
    // Copy the provided POSCAR file
    fs::copy_file(poscarPath, POSCAR, fs::copy_option::overwrite_if_exists);
    // Generate INCAR, POTCAR, and KPOINTS using VASPKIT
    RunCommand("vaspkit -task 103 2>&1");
    if (GenerateKPOINTS() != 0)
    {
        exit(EXIT_FAILURE);
    }
    GenerateINCAR("SR");
}

void Vasp::PerformStructureOptimization()
{
    fs::current_path(compute_dir_);
    // 准备一个结构优化专门的目录
    DEBUG_PRINT("Preparing structure optimization directory...");
    opt_dir_ = PrepareDirectory(OPT_DIR);

    // Copy the input files to the structure optimization directory
    //
    CopyFiles({POSCAR, INCAR, POTCAR, KPOINTS}, opt_dir_);

    fs::current_path(opt_dir_);

    // Run VASP for structure optimization
    RunCommand("mpirun -np 4 vasp_std > vasp_opt.log"); // Assuming MPI version of VASP
}

void Vasp::PerformStaticCalculation()
{
    fs::current_path(compute_dir_);
    // Prepare a directory for the static calculation
    static_dir_ = PrepareDirectory(STATIC_DIR);

    CopyFiles({POTCAR, KPOINTS}, static_dir_);

    // Copy the CONTCAR file from the OPTdir
    fs::copy_file(fs::path(opt_dir_) / CONTCAR, static_dir_ / POSCAR, fs::copy_option::overwrite_if_exists);

    fs::current_path(static_dir_);

    GenerateINCAR("ST");
    // Modify INCAR for static calculation
    // std::ofstream incar(INCAR);
    // incar << "ISTART = 0\n";
    // incar << "ICHARG = 2\n";
    // incar << "ISMEAR = 0\n";
    // incar << "SIGMA = 0.01\n";
    // incar << "LREAL = .FALSE.\n";
    // incar.close();

    // Run VASP for static calculation
    RunCommand("mpirun -np 4 vasp_std > vasp_static.log"); // Assuming MPI version of VASP
}

void Vasp::PerformDielectricCalculation()
{
    fs::current_path(compute_dir_);
    // Prepare a directory for the dielectric calculation
    dielectric_dir_ = PrepareDirectory(DIELECTRIC_DIR);

    // Copy the input files to the dielectric calculation directory
    CopyFiles({POTCAR, KPOINTS}, dielectric_dir_);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(opt_dir_) / CONTCAR, dielectric_dir_ / POSCAR, fs::copy_option::overwrite_if_exists);

    // Copy the WAVECAR file and CHGCAR file from the static calculation directory
    CopyFiles({static_dir_ / WAVECAR, static_dir_ / CHGCAR}, dielectric_dir_);

    fs::current_path(dielectric_dir_);

    // GenerateINCAR("EC");

    // ModifyINCAR(INCAR, {{"IBRION", "8"}});
    // // Modify INCAR for dielectric calculation
    // // std::ofstream incar(INCAR);
    // // incar << "LEPSILON = .TRUE.\n";
    // // incar.close();

    // // Run VASP for dielectric calculation
    // RunCommand("mpirun -np 4 vasp_std > vasp_dielectric.log");

    // Extract the dielectric constant from the OUTCAR file
    std::ifstream outcar("OUTCAR");
    std::string line;
    double sum = 0.0;
    bool found = false;
    // 写一个lambda表达式，用于提取张量的值
    // 实际上，这里应该提取张量的值，此处将三个方向的值简单相加并平均
    //  MACROSCOPIC STATIC DIELECTRIC TENSOR (including local field effects in DFT)
    //  ------------------------------------------------------
    //            2.481425    -0.000000     0.000000
    //           -0.000000     2.481425    -0.000000
    //           -0.000000     0.000000     2.517204
    //  ------------------------------------------------------
    //  格式如上，提取三个对角线上的值，相加并除以3
    auto extractTensorValue = [&line, &outcar]() -> double
    {
        double value = 0.0;
        std::string token;
        // Skip the first line
        std::getline(outcar, line);
        for (int i = 0; i < 3; i++)
        {
            std::getline(outcar, line);
            std::istringstream iss(line);
            for (int j = 0; j < 3; j++)
            {
                iss >> token;
                if (i == j)
                {
                    value += std::stod(token);
                }
            }
        }
        return value / 3.0;
    };

    while (std::getline(outcar, line))
    {
        // 如果是第一次碰到" MACROSCOPIC STATIC DIELECTRIC TENSOR (including local field effects in DFT)"
        // 那么接下来的三行就是我们需要的数据
        if (line.find("MACROSCOPIC STATIC DIELECTRIC TENSOR (including local field effects in DFT)") != std::string::npos)
        {
            if (!found)
            {
                sum += extractTensorValue();
            }
            found = true;
        }
        else if (line.find(" MACROSCOPIC STATIC DIELECTRIC TENSOR IONIC CONTRIBUTION") != std::string::npos)
        {
            sum += extractTensorValue();
        }
    }
    results_[DIELECTRIC] = std::to_string(sum);
    std::cout << "Dielectric constant: " << sum << std::endl;
}

void Vasp::PerformBandStructureCalculation()
{
    fs::current_path(compute_dir_);

    band_dir_ = PrepareDirectory(BAND_DIR);
    scf_dir_ = PrepareDirectory(SCF_DIR);

    //
    //  1：进行scf计算
    //      重要参数： NSW = 0
    //      IBRION = -1
    //      LWAVE = .T.
    //      LCHARG = .T.
    //      KPONITS 该参数需要比结构优化要更精确一点，如果在结构优化时取得比较粗糙，那么在这里建议要取得更密一点
    //

    CopyFiles({POTCAR, KPOINTS}, scf_dir_);

    // Copy the CONTCAR file from OPTdir
    fs::copy_file(fs::path(opt_dir_) / CONTCAR, scf_dir_ / POSCAR, fs::copy_option::overwrite_if_exists);

    // NOTE: 注意这里是从结构优化目录中复制文件而非静态计算目录
    CopyFiles({opt_dir_ / WAVECAR, opt_dir_ / CHGCAR, opt_dir_ / INCAR}, scf_dir_);

    std::map<std::string, std::string> incarOptions = {{"NSW", "0"}, {"IBRION", "-1"}, {"LWAVE", ".T."}, {"LCHARG", ".T."}};

    ModifyINCAR(scf_dir_ / INCAR, incarOptions);

    fs::current_path(scf_dir_);

    // Run VASP for SCF calculation
    RunCommand("mpirun -np 4 vasp_std > vasp_band_scf.log");

    // 2：进行带结构计算
    //      重要参数： NSW = 0
    //      IBRION = -1
    //      LWAVE = .F.
    //      LCHARG = .F.
    //      LORBIT = 11 # 11表示计算带结构
    //      加SOC,需要设置LSORBIT = .T.
    //      GGA_COMPAT = .FALSE. 加速收敛
    //      LMAXMIX = 4 有d轨道电子设4，f轨道电子设6
    //      SAXIS = 0 0 1 控制磁矩方向
    //      MAGMOM = 按坐标填磁矩

    CopyAllFiles(scf_dir_, band_dir_);

    band_dir_ = compute_dir_ / BAND_DIR;
    fs::current_path(band_dir_);

    // TODO: 实际上维度需要根据材料决定，这里先默认为3
    GenerateINCAR("BS", true, 3);

    // 修改 INCAR 文件
    incarOptions = {{"NSW", "0"}, {"IBRION", "-1"}, {"LWAVE", ".F."}, {"LCHARG", ".F."}, {"LORBIT", "11"}, {"LSORBIT", ".T."}, {"GGA_COMPAT", ".FALSE."}, {"LMAXMIX", "4"}, {"SAXIS", "0 0 1"}};
    ModifyINCAR(scf_dir_ / INCAR, incarOptions);

    // 修改K-PATH为KPOINTS
    fs::rename("KPATH.in", KPOINTS);

    // Run VASP for band structure calculation
    RunCommand("mpirun -np 4 vasp_std > vasp_band_nscf.log");

    // Extract the band gap from the OUTCAR file
    std::vector<std::vector<double>> energies;
    ReadBandDat("BAND.dat", energies);
    double bandgap, VBM, CBM;
    CalculateBandgap(energies, bandgap, VBM, CBM);

    results_[BANDGAP] = std::to_string(bandgap);
}

void Vasp::PerformThermalExpansionCalculation()
{
    fs::current_path(compute_dir_);
    thermal_expansion_dir_ = PrepareDirectory(THERMAL_EXPANSION_DIR);

    CopyFiles({POTCAR, KPOINTS}, thermal_expansion_dir_);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(opt_dir_) / CONTCAR, thermal_expansion_dir_ / POSCAR, fs::copy_option::overwrite_if_exists);

    // use prepared INCAR file
    fs::copy_file(root_dir_ / CONFIG_DIR / "thermalExpansion.INCAR", thermal_expansion_dir_ / INCAR, fs::copy_option::overwrite_if_exists);

    fs::copy_file(root_dir_ / CONFIG_DIR / "mesh.conf", thermal_expansion_dir_ / "mesh.conf", fs::copy_option::overwrite_if_exists);

    fs::copy_file(root_dir_ / SCRIPT_DIR / "thermalExpansionAnalysis.sh", thermal_expansion_dir_ / "thermalExpansionAnalysis.sh", fs::copy_option::overwrite_if_exists);

    fs::copy_file(root_dir_ / SCRIPT_DIR / "computeThermalExpansion.py", thermal_expansion_dir_ / "computeThermalExpansion.py", fs::copy_option::overwrite_if_exists);

    fs::current_path(thermal_expansion_dir_);

    std::string command = "bash ./thermalExpansionAnalysis.sh";
    
    RunCommand(command);

    // Extract the thermal expansion coefficient from the output file
    std::ifstream outfile("thermal_expansion_result.txt");
    std::string line;
    std::getline(outfile, line);
    results_[THERMAL_EXPANSION] = line;
    std::cout << "Thermal expansion coefficient: " << line << std::endl;
}

void Vasp::PerformConductivityCalculation()
{
    fs::current_path(compute_dir_);
    fs::path conductivity_dir_ = PrepareDirectory(CONDUCTIVITY_DIR);

    CopyFiles({POTCAR, INCAR}, conductivity_dir_);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(opt_dir_) / CONTCAR, conductivity_dir_ / POSCAR, fs::copy_option::overwrite_if_exists);

    CopyFiles({opt_dir_ / WAVECAR, opt_dir_ / CHGCAR, opt_dir_ / INCAR}, conductivity_dir_);

    fs::current_path(conductivity_dir_);

    ModifyINCAR(INCAR, {{"NSW", "0"}, {"SIGMA", "0.05"}, {"LWAVE", ".TRUE."}, {"LCHARG", ".TRUE."}, {"NEDOS", "2001"}});

    // Generate KPOINTS file
    VaspkitManager &vaspkit = VaspkitManager::getInstance();
    vaspkit.singleCommand("681\n");

    RunCommand("mpirun -np 4 vasp_std > vasp_conductivity.log");

    //Perfrom BoltzTraP calculation
    fs::copy_file( root_dir_ / CONFIG_DIR / "VPKIT.in", conductivity_dir_ / "VPKIT.in", fs::copy_option::overwrite_if_exists);

    vaspkit.singleCommand("682\n");

    // TODO: 处理电导率和载流子浓度的输出文件
}

void Vasp::UseHistoryOptDir()
{
    std::cout << "Looking for previous structure optimization directory..." << std::endl;
    fs::current_path(root_dir_);
    if (fs::is_directory(opt_dir_))
    {
        fs::current_path(opt_dir_);
    }
    else
    {
        // 查找当前目录下最新的以vasp_开头的目录
        fs::directory_iterator end;
        fs::path latestDir;
        std::time_t latestTime = 0;
        for (fs::directory_iterator it(root_dir_); it != end; ++it)
        {
            if (fs::is_directory(it->path()) && it->path().filename().string().find("vasp_") == 0)
            {
                std::time_t lastWrite = fs::last_write_time(it->path());
                if (lastWrite > latestTime)
                {
                    latestTime = lastWrite;
                    latestDir = it->path();
                }
            }
        }
        if (!latestDir.empty())
        {
            std::cout << "Found previous structure optimization directory: " << latestDir << std::endl;
            compute_dir_ = latestDir;
            opt_dir_ = latestDir / OPT_DIR;
            static_dir_ = latestDir / STATIC_DIR;
            fs::current_path(latestDir);
            std::cout << "current computeDir: " << compute_dir_ << std::endl;
            std::cout << "current optDir: " << opt_dir_ << std::endl;
            std::cout << "current staticDir: " << static_dir_ << std::endl;
        }
        else
        {
            std::cerr << "Error: No previous structure optimization directory found." << std::endl;
        }
    }
}

fs::path Vasp::StoreResults()
{
    fs::current_path(compute_dir_);
    std::string result_file_name = "results_" + std::to_string(std::time(nullptr)) + ".txt";
    std::ofstream result_file(result_file_name);
    for (const auto &result : results_)
    {
        result_file << result.first << " = " << result.second << " " << units[result.first] << std::endl;
    }
    result_file.close();
    fs::path resultsPath = fs::current_path() / result_file_name;
    return resultsPath;
}
