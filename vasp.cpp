#include "vasp.h"

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cerr << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

////////////////////////// Utility Functions //////////////////////////

void fs_copy_files(const std::vector<fs::path> &source_files, const fs::path &destination_dir)
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

void fs_copy_all_files(const fs::path &source_dir, const fs::path &destination_dir)
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

void runCommand(const std::string &command)
{
    int result = system(command.c_str());
    if (result != 0)
    {
        std::cerr << "Error: Command failed -> " << command << std::endl;
        exit(EXIT_FAILURE);
    }
}

std::string extractKmeshValue(const std::string &filename)
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

void modifyINCAR(fs::path filepath, const std::map<std::string, std::string> &options)
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

////////////////////////// Vasp Class//////////////////////////
std::string Vasp::prepareDirectory(const std::string &computeTask = "")
{
    try
    {
        fs::current_path(rootDir);
        std::string retDir;
        std::string timestamp = std::to_string(std::time(nullptr));
        if (computeTask.empty())
        {
            if (computeDir.empty())
            {
                computeDir = "vasp_" + timestamp;
                DEBUG_PRINT("computeDir: " << computeDir);
                fs::create_directory(computeDir);
                computeDir = rootDir / computeDir;
                retDir = computeDir.string();
            }
            else
            {
                throw std::runtime_error("Error: computeDir is not empty.");
            }
        }
        else
        {
            // 在现有的 computeDir 中创建子任务目录
            if (fs::is_directory(computeDir))
            {
                fs::path taskDir = fs::path(computeDir) / computeTask;
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
        fs::current_path(computeDir);
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

int Vasp::generateKPOINTS()
{
    // 启动 VASPKIT 进程
    VaspkitManager &vaspkit = VaspkitManager::getInstance();

    vaspkit.startVaspkit((*computeDir.end()).string());

    vaspkit.sendInputToVaspkit("1\n");
    vaspkit.sendInputToVaspkit("102\n");
    vaspkit.sendInputToVaspkit("1\n");

    // 等待 VASPKIT 产生输出（模拟延迟）
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 从输出文件中提取 Kmesh 值
    std::string kmeshValue = extractKmeshValue(vaspkit.getOutputFilename());
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

void Vasp::generateINCAR(const std::string &INCAROptions, bool isSCF = false, int dimension = 0)
{
    VaspkitManager &vaspkit = VaspkitManager::getInstance();
    vaspkit.startVaspkit((*computeDir.end()).string());
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

void Vasp::generateInputFiles(const std::string &poscarPath)
{
    fs::current_path(computeDir);
    // Copy the provided POSCAR file
    fs::copy_file(poscarPath, POSCAR, fs::copy_option::overwrite_if_exists);
    // Generate INCAR, POTCAR, and KPOINTS using VASPKIT
    runCommand("vaspkit -task 103 2>&1");
    if (generateKPOINTS() != 0)
    {
        exit(EXIT_FAILURE);
    }
    generateINCAR("SR");
}

void Vasp::performStructureOptimization()
{
    fs::current_path(computeDir);
    // 准备一个结构优化专门的目录
    DEBUG_PRINT("Preparing structure optimization directory...");
    optDir = prepareDirectory(OPT_DIR);

    // Copy the input files to the structure optimization directory
    //
    fs_copy_files({POSCAR, INCAR, POTCAR, KPOINTS}, optDir);

    fs::current_path(optDir);

    // Run VASP for structure optimization
    runCommand("mpirun -np 4 vasp_std > vasp_opt.log"); // Assuming MPI version of VASP
}

void Vasp::performStaticCalculation()
{
    fs::current_path(computeDir);
    // Prepare a directory for the static calculation
    staticDir = prepareDirectory(STATIC_DIR);

    fs_copy_files({POTCAR, KPOINTS}, staticDir);

    // Copy the CONTCAR file from the OPTdir
    fs::copy_file(fs::path(optDir) / CONTCAR, staticDir / POSCAR, fs::copy_option::overwrite_if_exists);

    fs::current_path(staticDir);

    generateINCAR("ST");
    // Modify INCAR for static calculation
    // std::ofstream incar(INCAR);
    // incar << "ISTART = 0\n";
    // incar << "ICHARG = 2\n";
    // incar << "ISMEAR = 0\n";
    // incar << "SIGMA = 0.01\n";
    // incar << "LREAL = .FALSE.\n";
    // incar.close();

    // Run VASP for static calculation
    runCommand("mpirun -np 4 vasp_std > vasp_static.log"); // Assuming MPI version of VASP
}

void Vasp::performDielectricCalculation()
{
    fs::current_path(computeDir);
    // Prepare a directory for the dielectric calculation
    permittivityDir = prepareDirectory(PERMITTIVITY_DIR);

    // Copy the input files to the dielectric calculation directory
    fs_copy_files({POTCAR, KPOINTS}, permittivityDir);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(optDir) / CONTCAR, permittivityDir / POSCAR, fs::copy_option::overwrite_if_exists);

    // Copy the WAVECAR file and CHGCAR file from the static calculation directory
    fs_copy_files({staticDir / WAVECAR, staticDir / CHGCAR}, permittivityDir);

    fs::current_path(permittivityDir);

    generateINCAR("EC");
    // Modify INCAR for dielectric calculation
    // std::ofstream incar(INCAR);
    // incar << "LEPSILON = .TRUE.\n";
    // incar.close();

    // Run VASP for dielectric calculation
    runCommand("mpirun -np 4 vasp_std > vasp_dielectric.log");
}

void Vasp::performBandStructureCalculation()
{
    fs::current_path(computeDir);

    bandDir = prepareDirectory(BAND_DIR);
    scfDir = prepareDirectory(SCF_DIR);

    //
    //  1：进行scf计算
    //      重要参数： NSW = 0
    //      IBRION = -1
    //      LWAVE = .T.
    //      LCHARG = .T.
    //      KPONITS 该参数需要比结构优化要更精确一点，如果在结构优化时取得比较粗糙，那么在这里建议要取得更密一点
    //

    fs_copy_files({POTCAR, KPOINTS}, scfDir);

    // Copy the CONTCAR file from OPTdir
    fs::copy_file(fs::path(optDir) / CONTCAR, scfDir / POSCAR, fs::copy_option::overwrite_if_exists);

    // NOTE: 注意这里是从结构优化目录中复制文件而非静态计算目录
    fs_copy_files({optDir / WAVECAR, optDir / CHGCAR, optDir / INCAR}, scfDir);

    std::map<std::string, std::string> incarOptions = {{"NSW", "0"}, {"IBRION", "-1"}, {"LWAVE", ".T."}, {"LCHARG", ".T."}};

    fs::current_path(scfDir);

    modifyINCAR(INCAR, incarOptions);

    // Run VASP for SCF calculation
    runCommand("mpirun -np 4 vasp_std > vasp_band_scf.log");

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

    fs_copy_all_files(scfDir, bandDir);

    fs::current_path(bandDir);

    // TODO: 实际上维度需要根据材料决定，这里先默认为3
    generateINCAR("BS", true, 3);

    // 修改 INCAR 文件
    incarOptions = {{"NSW", "0"}, {"IBRION", "-1"}, {"LWAVE", ".F."}, {"LCHARG", ".F."}, {"LORBIT", "11"}, {"LSORBIT", ".T."}, {"GGA_COMPAT", ".FALSE."}, {"LMAXMIX", "4"}, {"SAXIS", "0 0 1"}};
    modifyINCAR(scfDir / INCAR, incarOptions);

    // 修改K-PATH为KPOINTS
    fs::rename("KPATH.in", KPOINTS);

    // Run VASP for band structure calculation
    runCommand("mpirun -np 4 vasp_std > vasp_band_nscf.log");
}

void Vasp::performThermalExpansionCalculation()
{
    fs::current_path(computeDir);
    thermalExpansionDir = prepareDirectory(THERMAL_EXPANSION_DIR);

    fs_copy_files({POTCAR, KPOINTS}, thermalExpansionDir);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(optDir) / CONTCAR, thermalExpansionDir / POSCAR, fs::copy_option::overwrite_if_exists);

    // Use prepared INCAR file
    fs::copy_file(rootDir / CONFIG_DIR / "thermalExpansion.INCAR", thermalExpansionDir / INCAR, fs::copy_option::overwrite_if_exists);

    fs::copy_file(rootDir / CONFIG_DIR / "mesh.conf", thermalExpansionDir / "mesh.conf", fs::copy_option::overwrite_if_exists);

    fs::copy_file(rootDir / SCRIPT_DIR / "thermalExpansionAnalysis.sh", thermalExpansionDir / "thermalExpansionAnalysis.sh", fs::copy_option::overwrite_if_exists);

    fs::current_path(thermalExpansionDir);

    std::string command = "bash ./thermalExpansionAnalysis.sh";
    runCommand(command);
}

void Vasp::performConductivityCalculation()
{
    fs::current_path(computeDir);
    fs::path conductivityDir = prepareDirectory(CONDUCTIVITY_DIR);

    fs_copy_files({POTCAR, INCAR}, conductivityDir);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(optDir) / CONTCAR, conductivityDir / POSCAR, fs::copy_option::overwrite_if_exists);

    fs_copy_files({optDir / WAVECAR, optDir / CHGCAR, optDir / INCAR}, conductivityDir);

    fs::current_path(conductivityDir);

    modifyINCAR(INCAR, {{"NSW", "0"}, {"SIGMA", "0.05"}, {"LWAVE", ".TRUE."}, {"LCHARG", ".TRUE."}, {"NEDOS", "2001"}});

    // Generate KPOINTS file
    VaspkitManager &vaspkit = VaspkitManager::getInstance();
    vaspkit.singleCommand("681\n");

    runCommand("mpirun -np 4 vasp_std > vasp_conductivity.log");

    //Perfrom BoltzTraP calculation
    fs::copy_file( rootDir / CONFIG_DIR / "VPKIT.in", conductivityDir / "VPKIT.in", fs::copy_option::overwrite_if_exists);

    vaspkit.singleCommand("682\n");

    // TODO: 处理电导率和载流子浓度的输出文件
}

void Vasp::useHistoryOptDir()
{
    std::cout << "Looking for previous structure optimization directory..." << std::endl;
    fs::current_path(rootDir);
    if (fs::is_directory(optDir))
    {
        fs::current_path(optDir);
    }
    else
    {
        // 查找当前目录下最新的以vasp_开头的目录
        fs::directory_iterator end;
        fs::path latestDir;
        std::time_t latestTime = 0;
        for (fs::directory_iterator it(rootDir); it != end; ++it)
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
            computeDir = latestDir;
            optDir = latestDir / OPT_DIR;
            fs::current_path(latestDir);
            std::cout << "current computeDir: " << computeDir << std::endl;
            std::cout << "current optDir: " << optDir << std::endl;
        }
        else
        {
            std::cerr << "Error: No previous structure optimization directory found." << std::endl;
        }
    }
}

