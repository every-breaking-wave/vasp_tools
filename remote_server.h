#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include "vasp.h"

using boost::asio::ip::tcp;
namespace fs = boost::filesystem;

class RemoteServer
{
public:
    RemoteServer(boost::asio::io_context &io_context, int port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        StartAccept();
        std::string vasp_dir = "~/vaspcompute";
        if (!fs::exists(vasp_dir))
        {
            fs::create_directory(vasp_dir);
        }
        root_dir_ = fs::canonical(vasp_dir);
        fs::current_path(root_dir_);
    }

private:
    void StartAccept();

    void HandleConnection(std::shared_ptr<tcp::socket> socket);

    void ReceiveFileContent(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file);

    std::string ExecuteCommand(const std::string &command);

    void PerformVaspCompute(const std::string &poscarPath);

    tcp::acceptor acceptor_;

    // 记录vasp计算的根目录
    fs::path root_dir_;
};
