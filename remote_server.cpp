
#include "remote_server.h"
#include "time.h"
using boost::asio::ip::tcp;

void RemoteServer::StartAccept()
{
    auto socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec)
                           {
            if (!ec) {
                HandleConnection(socket);
            }
            StartAccept(); });
}

void RemoteServer::HandleConnection(std::shared_ptr<tcp::socket> socket)
{
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [this, socket, buffer](boost::system::error_code ec, std::size_t length)
                            {
        if (!ec) {
            std::string data(buffer->data(), length);
            if (data.substr(0, 4) == "FILE") {
                std::string::size_type pos = data.find('\n');
                std::string posfile;
                if (pos != std::string::npos) {
                    posfile = data.substr(4, pos - 4) + "_" + std::to_string(time(nullptr));
                    auto file = std::make_shared<std::ofstream>(posfile, std::ios::binary);
                    if (!file->is_open()) {
                        std::cerr << "Failed to open file: " << posfile << std::endl;
                        return;
                    }
                    // 保存首个数据块（去掉文件名部分）
                    file->write(data.data() + pos + 1, length - pos - 1);

                    // 继续接收并保存文件内容
                    ReceiveFileContent(socket, file);
                }
                if(fs::is_regular_file(posfile) && posfile.substr(0, 6) == "POSCAR")
                {
                    PerformVaspCompute(posfile);
                }
            } else if (data.substr(0, 4) == "COMPUTE ") {
                    std::string command = data.substr(4);
                    std::string output = ExecuteCommand(command);
                    boost::asio::async_write(*socket, boost::asio::buffer(output), 
                        [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
                            if (!ec) {
                                std::cout << "Command executed and result sent back." << std::endl;
                            } else {
                                std::cerr << "Failed to send command output: " << ec.message() << std::endl;
                            }
                        });
                }
            } else {
                std::cerr << "Failed to read from socket: " << ec.message() << std::endl;
            } });
}

void RemoteServer::ReceiveFileContent(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file)
{
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [this, socket, buffer, file](boost::system::error_code ec, std::size_t length)
                            {
        if (!ec) {
            file->write(buffer->data(), length);
            ReceiveFileContent(socket, file); // 继续接收剩余内容
        } else if (ec == boost::asio::error::eof) {
            file->close(); // 完成传输
            std::cout << "File received and saved successfully." << std::endl;
        } else {
            std::cerr << "Failed to receive file content: " << ec.message() << std::endl;
            if (file->is_open()) {
                file->close(); // 确保文件被正确关闭
            }
        } });
}

std::string RemoteServer::ExecuteCommand(const std::string &command)
{
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    return result;
}

void RemoteServer::PerformVaspCompute(const std::string &poscarPath)
{
    std::string cwd = get_current_dir_name();
    // 为每次 VASP 计算准备一个新的目录、vasp对象
    Vasp *vasp = new Vasp(cwd);

    vasp->PrepareDirectory("");

    std::cout << "Generating input files..." << std::endl;
    vasp->GenerateInputFiles(poscarPath);

    std::cout << "Performing structure optimization..." << std::endl;
    vasp->PerformStructureOptimization();

    std::cout << "Performing static calculation..." << std::endl;
    vasp->PerformStaticCalculation();

    std::cout << "Performing dielectric calculation..." << std::endl;
    vasp->PerformDielectricCalculation();

    std::cout << "Performing band structure calculation..." << std::endl;
    vasp->PerformBandStructureCalculation();
    
    std::cout << "Performing thermal expansion calculation..." << std::endl;
    std::cout << "This could take a long time." << std::endl;
    vasp->PerformThermalExpansionCalculation();

    std::cout << "VASP calculation complete." << std::endl;
}