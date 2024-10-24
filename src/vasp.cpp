#include "vasp.h"
#include "tool.h"

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cerr << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

////////////////////////// Vasp Class//////////////////////////
std::string Vasp::PrepareDirectory(const std::string &computeTask = "")
{
    try
    {
        fs::current_path(data_dir_);
        std::string retDir;
        std::string timestamp = std::to_string(std::time(nullptr));
        if (computeTask.empty())
        {
            if (compute_dir_.empty())
            {
                compute_dir_ = "vasp_" + timestamp;
                DEBUG_PRINT("computeDir: " << compute_dir_);
                fs::create_directory(compute_dir_);
                compute_dir_ = data_dir_ / compute_dir_;
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
    fs::copy_file(data_dir_ / poscarPath, POSCAR, fs::copy_option::overwrite_if_exists);
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
    RunCommand("mpirun -np 12 vasp_std > vasp_opt.log"); // Assuming MPI version of VASP
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
    RunCommand("mpirun -np 12 vasp_std > vasp_static.log"); // Assuming MPI version of VASP
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

    GenerateINCAR("EC");

    ModifyINCAR(INCAR, {{"IBRION", "8"}});
    // Modify INCAR for dielectric calculation
    // std::ofstream incar(INCAR);
    // incar << "LEPSILON = .TRUE.\n";
    // incar.close();

    // Run VASP for dielectric calculation
    try
    {
        RunCommand("mpirun -np 12 vasp_std > vasp_dielectric.log");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

    // Extract the dielectric constant from the OUTCAR file
    std::ifstream outcar(OUTCAR);
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
    try
    {
        RunCommand("mpirun -np 12 vasp_std > vasp_band_scf.log");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

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
    ModifyINCAR(band_dir_ / INCAR, incarOptions);

    // 修改K-PATH为KPOINTS
    fs::rename("KPATH.in", KPOINTS);

    try
    {
        // Run VASP for band structure calculation
        RunCommand("mpirun -np 12 vasp_std > vasp_band_nscf.log");
        // Extract the band gap from the OUTCAR file
        std::vector<std::vector<double>> energies;
        ReadBandDat("BAND.dat", energies);
        double bandgap, VBM, CBM;
        CalculateBandgap(energies, bandgap, VBM, CBM);
        results_[BANDGAP] = std::to_string(bandgap);
    }
    catch (const std::exception &e)
    {
        // std::cerr << e.what() << '\n';
        // 直接对OPT目录进行带结构计算
        fs::current_path(opt_dir_);
        try
        {
            fs::path read_band_script = root_dir_ / SCRIPT_DIR / "readbandgap.sh";
            RunCommand("bash " + read_band_script.string() + " ./OUTCAR > band_gap_result.txt");
            std::ifstream bandgap_file("band_gap_result.txt");
            std::string line;
            while (std::getline(bandgap_file, line))
            {
                results_[BANDGAP] = line;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            return;
        }
        return;
    }
}

void Vasp::PerformThermalExpansionCalculation()
{
    fs::current_path(compute_dir_);
    thermal_expansion_dir_ = PrepareDirectory(THERMAL_EXPANSION_DIR);

    CopyFiles({POTCAR, KPOINTS}, thermal_expansion_dir_);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(opt_dir_) / CONTCAR, thermal_expansion_dir_ / POSCAR, fs::copy_option::overwrite_if_exists);

    // use prepared INCAR file
    fs::current_path(root_dir_ / CONFIG_DIR);

    fs::copy_file("thermalExpansion.INCAR", thermal_expansion_dir_ / INCAR, fs::copy_option::overwrite_if_exists);

    fs::copy_file("mesh.conf", thermal_expansion_dir_ / "mesh.conf", fs::copy_option::overwrite_if_exists);

    fs::current_path(root_dir_ / SCRIPT_DIR);

    CopyFiles({"thermalExpansionAnalysis.sh", "computeThermalExpansion.py", "computeSpecifiedHeat.py", "getDensity.py"}, thermal_expansion_dir_);

    fs::current_path(thermal_expansion_dir_);

    try
    {
        std::string command = "bash ./thermalExpansionAnalysis.sh";
        RunCommand(command);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

    // Extract the thermal expansion coefficient from the output file
    std::ifstream outfile("thermal_expansion_result.txt");
    std::string line;
    std::getline(outfile, line);
    results_[THERMAL_EXPANSION] = line;
    std::cout << "Thermal expansion coefficient: " << line << std::endl;

    // Extract the specific heat capacity from the output file
    std::ifstream outfile2("specified_heat_result.txt");
    std::getline(outfile2, line);
    results_[SPECIFIC_HEAT] = line;
    std::cout << "Specific heat capacity: " << line << std::endl;

    // Extract the thermal conductivity from the output file
    // NOTE: 这部分可算，但是耗时太长，暂时不用
    // std::ifstream outfile3("thermal_conductivity_result.txt");
    // std::getline(outfile3, line);
    // results_[THERMAL_CONDUCTIVITY] = line;
    // std::cout << "Thermal conductivity: " << line << std::endl;
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

    ModifyINCAR(INCAR, {{"ISTART", "1"}, {"ISPIN", "1"}, {"LREAL", ".FALSE."}, {"LWAVE", ".TRUE."}, {"LCHARG", ".TRUE."}, {"ADDGRID", ".TRUE."}, {"ISMEAR", "0"}, {"SIGMA", "0.05"}, {"LORBIT", "11"}, {"NEDOS", "2001"}, {"NELM", "60"}, {"EDIFF", "1E-08"}}, true);

    // Generate KPOINTS file
    VaspkitManager &vaspkit = VaspkitManager::getInstance();
    vaspkit.startVaspkit((*compute_dir_.end()).string());

    vaspkit.sendInputToVaspkit("681\n");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    vaspkit.sendInputToVaspkit("0.005\n");
    vaspkit.stopVaspkit();
    // std::string kspacingValue = ExtractKSpacingValue(vaspkit.getOutputFilename());
    // if (!kspacingValue.empty())
    // {
    //     vaspkit.sendInputToVaspkit(kspacingValue + "\n");
    //     vaspkit.stopVaspkit();
    // }
    // else
    // {
    //     std::cerr << "Error: Could not Extract K Spacing value from VASPKIT output." << std::endl;
    //     vaspkit.stopVaspkit();
    //     return;
    // }
    try
    {
        RunCommand("mpirun -np 12 vasp_std > vasp_conductivity.log");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

    // Perfrom BoltzTraP calculation
    fs::copy_file(root_dir_ / CONFIG_DIR / "VPKIT.in", conductivity_dir_ / "VPKIT.in", fs::copy_option::overwrite_if_exists);

    std::cout << "Running BoltzTraP calculation..." << std::endl;
    vaspkit.singleCommand("682\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::string conductivity_file = "ELECTRONIC_CONDUCTIVITY.dat";
    std::string carrier_concentration_file = "CARRIER_CONCENTRATION.dat";

    auto conductivity = readElectronicConductivity(conductivity_file);
    auto concentration = readCarrierConcentration(carrier_concentration_file);

    // 将结果保存到results_中, 保留三位有效数字
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(2) << conductivity.average_conductivity;
    std::string scientific_notation = oss.str();
    results_[CONDUCTIVITY] = scientific_notation;

    std::ostringstream oss2;
    oss2 << std::scientific << std::setprecision(2) << concentration.concentration;
    std::string scientific_notation2 = oss2.str();
    results_[MOBILITY] = scientific_notation2;

    vaspkit.stopVaspkit();
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
        for (fs::directory_iterator it(data_dir_); it != end; ++it)
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

void Vasp::GetDensity()
{
    fs::current_path(opt_dir_);

    // 执行 getDensity.py 脚本并捕获其输出
    fs::copy_file(root_dir_ / SCRIPT_DIR / "getDensity.py", "getDensity.py", fs::copy_option::overwrite_if_exists);

    std::string command = "python3 getDensity.py > density_result.txt";

    try
    {
        RunCommand(command);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

    // 从输出文件中提取密度值
    std::ifstream density_file("density_result.txt");
    std::string line;
    std::getline(density_file, line);
    results_[DENSITY] = line;
}

std::vector<std::string> Vasp::StoreResults()
{
    fs::current_path(compute_dir_);
    std::string result_file_name = "results_" + std::to_string(std::time(nullptr)) + ".txt";
    std::ofstream result_file(result_file_name);
    for (const auto &result : results_)
    {
        if (result.second.empty())
        {
            continue;
        }
        result_file << result.first << " = " << result.second << " " << units[result.first] << std::endl;
        std::cout << result.first << " = " << result.second << " " << units[result.first] << std::endl;
    }
    result_file.close();
    fs::path resultsPath = fs::current_path() / result_file_name;

    // 将各个目录下面的日志整合到一个文件中
    std::string log_file_name = "vasp_log.txt";
    std::ofstream log_file(log_file_name);
    std::vector<fs::path> logFiles = {opt_dir_ / "vasp_opt.log", static_dir_ / "vasp_static.log", dielectric_dir_ / "vasp_dielectric.log", band_dir_ / "vasp_band_scf.log", thermal_expansion_dir_ / "vasp_thermal_expansion.log", conductivity_dir_ / "vasp_conductivity.log"};
    for (const auto &logFile : logFiles)
    {
        // 首先判断文件是否存在
        if (!fs::exists(logFile))
        {
            continue;
        }
        log_file << "======================" << logFile << "======================\n";
        std::ifstream log(logFile.string());
        log_file << log.rdbuf();
        log_file << "=====================================================\n";
    }

    std::vector<std::string> resultsPathVec;



    resultsPathVec.push_back(resultsPath.string());
    resultsPathVec.push_back(static_dir_.string() + "/CHGCAR");
    resultsPathVec.push_back(static_dir_.string() + "/CONTCAR");
    resultsPathVec.push_back(compute_dir_.string() + "/" + log_file_name);
    return resultsPathVec;
}
