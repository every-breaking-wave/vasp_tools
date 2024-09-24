#include "tool.h"
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

using boost::asio::ip::tcp;

void send_file_content(std::shared_ptr<std::ifstream> file, tcp::socket* socket)
{
    auto buffer = std::make_shared<std::array<char, 1024>>();

    if (file->read(buffer->data(), buffer->size()) || file->gcount() > 0)
    {
        boost::asio::async_write(*socket, boost::asio::buffer(*buffer, file->gcount()),
                                 [file, buffer, socket](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         // 继续发送剩余文件内容
                                         send_file_content(file, socket);
                                     }
                                     else
                                     {
                                         std::cerr << "Failed to send file content: " << ec.message() << std::endl;
                                         socket->close();  // 处理错误时关闭连接
                                     }
                                 });
    }
    else
    {
        std::cout << "File sent successfully." << std::endl;
        socket->shutdown(tcp::socket::shutdown_send);  // 发送完毕后关闭发送通道
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
