#include "tool.h"
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

using boost::asio::ip::tcp;


void send_file_content(std::shared_ptr<std::ifstream> file, tcp::socket* socket, std::string filename) {
    if (!file || !file->is_open()) {
        std::cerr << "file name " << filename << " is not valid or not open." << std::endl;
        std::cerr << "File stream is not valid or not open." << std::endl;
        return;
    }

    try {
        // 文件名长度
        uint32_t filename_len = filename.size();
        boost::asio::write(*socket, boost::asio::buffer(&filename_len, sizeof(filename_len)));

        // 文件名
        boost::asio::write(*socket, boost::asio::buffer(filename));

        // 获取文件大小
        file->seekg(0, std::ios::end);
        uint64_t filesize = file->tellg();
        file->seekg(0, std::ios::beg);

        // 发送文件大小
        boost::asio::write(*socket, boost::asio::buffer(&filesize, sizeof(filesize)));

        // 发送文件内容
        std::vector<char> buffer(1024);
        while (!file->eof()) {
            file->read(buffer.data(), buffer.size());
            std::streamsize bytes_read = file->gcount();

            if (bytes_read > 0) {
                boost::asio::write(*socket, boost::asio::buffer(buffer.data(), bytes_read));
            }
        }
        std::cout << "File content sent successfully: " << filename << std::endl;
    } catch (std::exception &e) {
        std::cerr << "Exception while sending file content: " << e.what() << std::endl;
    } 
}



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
        // 抛出异常
        throw std::runtime_error("Command failed: " + command);
    }
}
