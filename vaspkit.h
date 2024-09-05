#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <memory>

struct PcloseDeleter {
    void operator()(FILE* ptr) const {
        if (ptr) {
            pclose(ptr);
        }
    }
};

class VaspkitManager {
public:
    static VaspkitManager& getInstance() {
        static VaspkitManager instance;
        return instance;
    }

    void startVaspkit(const std::string &log_mark = "") {
        if (!vaspkitPipe) {
            std::string output_filename = "vaspkit_output_" + log_mark + ".log";
            std::string command = "vaspkit > " + output_filename + " 2>&1";
            this->output_filename = output_filename;
            vaspkitPipe = std::unique_ptr<FILE, PcloseDeleter>(popen(command.c_str(), "w"));
            if (!vaspkitPipe) {
                std::cerr << "Error: Failed to start VASPKIT process." << std::endl;
                exit(EXIT_FAILURE);
            }
        } else {
            std::cerr << "VASPKIT is already running." << std::endl;
        }
    }

    void stopVaspkit() {
        if (vaspkitPipe) {
            vaspkitPipe.reset();
        }
    }

    void sendInputToVaspkit(const std::string& input) {
        if (vaspkitPipe) {
            fputs(input.c_str(), vaspkitPipe.get());
            fflush(vaspkitPipe.get());
        } else {
            std::cerr << "VASPKIT process is not running." << std::endl;
        }
    }

    void singleCommand(const std::string& command) {
        startVaspkit();
        sendInputToVaspkit(command);
        stopVaspkit();
    }

    const std::string& getOutputFilename() const {
        return output_filename;
    }

private:
    VaspkitManager() = default;
    ~VaspkitManager() = default;
    VaspkitManager(const VaspkitManager&) = delete;
    VaspkitManager& operator=(const VaspkitManager&) = delete;
    std::string output_filename;
    std::unique_ptr<FILE, PcloseDeleter> vaspkitPipe;
};
