#include "vasp.h"


#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cerr << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif


namespace fs = boost::filesystem;

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


void runCommand(const std::string &command)
{
    int result = system(command.c_str());
    if (result != 0)
    {
        std::cerr << "Error: Command failed -> " << command << std::endl;
        exit(EXIT_FAILURE);
    }
}


std::string extractKmeshValue(const std::string& filename) {
    std::ifstream infile(filename);
    std::string line;
    std::string kmeshValue;

    while (std::getline(infile, line)) {
        if (line.find("is Generally Precise Enough!") != std::string::npos) {
            std::istringstream iss(line);
            // 忽略前面的部分，提取 Kmesh-Resolved Value
            std::string token;
            while (iss >> token) {
                if (token.find('-') != std::string::npos) {
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

////////////////////////// Vasp //////////////////////////
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
    // 发送初始输入
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

void Vasp::generateINCAR(const std::string &INCAROptions)
{
    VaspkitManager &vaspkit = VaspkitManager::getInstance();
    vaspkit.startVaspkit((*computeDir.end()).string());
    vaspkit.sendInputToVaspkit("101\n");
    vaspkit.sendInputToVaspkit(INCAROptions + "\n");
    vaspkit.stopVaspkit();
}

void Vasp::generateInputFiles(const std::string &poscarPath)
{
    fs::current_path(computeDir);
    // Copy the provided POSCAR file
    fs::copy_file(poscarPath, "POSCAR", fs::copy_option::overwrite_if_exists);
    // Generate INCAR, POTCAR, and KPOINTS using VASPKIT
    runCommand("vaspkit -task 103"); // Generate POTCAR
    if (generateKPOINTS() != 0)
    {
        exit(EXIT_FAILURE);
    }
    generateINCAR("SR");
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void Vasp::performStructureOptimization()
{
    fs::current_path(computeDir);
    // 准备一个结构优化专门的目录
    DEBUG_PRINT ("Preparing structure optimization directory...");
    optDir = prepareDirectory(OPT_DIR);

    // Copy the input files to the structure optimization directory    
    //
    fs_copy_files({"POSCAR", "INCAR", "POTCAR", "KPOINTS"}, optDir);

    fs::current_path(optDir);

    // Run VASP for structure optimization
    runCommand("mpirun -np 4 vasp_std > vasp_opt.log"); // Assuming MPI version of VASP
}

void Vasp::performStaticCalculation()
{
    fs::current_path(computeDir);
    // Prepare a directory for the static calculation
    staticDir = prepareDirectory(STATIC_DIR);

    // Copy the input files to the static calculation directory
    fs_copy_files({"POTCAR", "KPOINTS"}, staticDir);

    // Copy the CONTCAR file from the structure optimization directory
    fs::copy_file(fs::path(optDir) / "CONTCAR", fs::path(staticDir) / "POSCAR", fs::copy_option::overwrite_if_exists);

    // Change to the static calculation directory
    fs::current_path(staticDir);
    generateINCAR("ST");

    // Modify INCAR for static calculation
    // std::ofstream incar("INCAR");
    // incar << "ISTART = 0\n";
    // incar << "ICHARG = 2\n";
    // incar << "ISMEAR = 0\n";
    // incar << "SIGMA = 0.01\n";
    // incar << "LREAL = .FALSE.\n";
    // incar.close();

    // Run VASP for static calculation
    runCommand("mpirun -np 4 vasp_std > vasp_static.log"); // Assuming MPI version of VASP
}

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

    vasp.prepareDirectory();

    std::cout << "Generating input files..." << std::endl;
    vasp.generateInputFiles(poscarPath);

    std::cout << "Performing structure optimization..." << std::endl;
    vasp.performStructureOptimization();

    std::cout << "Performing static calculation..." << std::endl;
    vasp.performStaticCalculation();

    std::cout << "VASP calculation complete." << std::endl;
    return EXIT_SUCCESS;
}
