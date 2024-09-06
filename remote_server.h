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
        const char *home = getenv("HOME");
        root_dir_ = fs::current_path();
        data_dir_ = home;
        std::string vasp_dir = "vasp_calculations";
        data_dir_ /= vasp_dir;
        if (!fs::exists(vasp_dir))
        {
            fs::create_directory(data_dir_);
        }
        std::cout << "Data directory: " << data_dir_ << std::endl;
    }

private:
    void StartAccept();

    void HandleConnection(std::shared_ptr<tcp::socket> socket);

    void ReceiveFileContent(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file, const std::string& file_name, std::function<void(const std::string&, std::shared_ptr<tcp::socket>)> on_complete);

    std::string ExecuteCommand(const std::string &command);

    fs::path PerformVaspCompute(const std::string &poscarPath);

    void HandleFileCompletion(const std::string &filename, std::shared_ptr<tcp::socket> socket);

    tcp::acceptor acceptor_;

    fs::path data_dir_;
    fs::path root_dir_;
};
