
#include "remote_server.h"
#include "time.h"
#include "tool.h"

using boost::asio::ip::tcp;

void RemoteServer::HandleFileCompletion(const std::string &filename, std::shared_ptr<tcp::socket> socket)
{
    if (fs::is_regular_file(filename) && filename.substr(0, 6) == "POSCAR")
    {
        fs::path result_file = PerformVaspCompute(filename);

        // 通知客户端计算完成，并将结果文件发送回客户端
        boost::asio::async_write(*socket, boost::asio::buffer("COMPUTE finished \n"),
                                 [socket, result_file](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         auto result_file_stream = std::make_shared<std::ifstream>(result_file.filename().string(), std::ios::binary);
                                         send_file_content(result_file_stream, socket.get());
                                     }
                                     else
                                     {
                                         std::cerr << "Failed to send command output: " << ec.message() << std::endl;
                                     }
                                 });
    }
}

void RemoteServer::SendFileToServer(const std::string &filename)
{
    try
    {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve("localhost", "12345"));

        std::cout << "Sending file: " << filename << std::endl;
        std::shared_ptr<std::ifstream> file = std::make_shared<std::ifstream>(filename, std::ios::binary);
        if (!file->is_open())
        {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        // 发送文件名
        std::string file_name = "FILE " + filename + "\n";
        boost::asio::write(socket, boost::asio::buffer(file_name));

        // 发送文件内容
        send_file_content(file, &socket);
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

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
    fs::current_path(data_dir_);
    socket->async_read_some(boost::asio::buffer(*buffer), [this, socket, buffer](boost::system::error_code ec, std::size_t length)
                            {
        if (!ec) {
            std::string data(buffer->data(), length);
            if (data.substr(0, 4) == "FILE") {
                std::string::size_type pos = data.find('\n');
                std::string posfile;
                if (pos != std::string::npos) {
                    posfile = data.substr(4, pos - 4) + "_" + std::to_string(time(nullptr));
                    posfile = posfile.substr(posfile.find_last_of("/") + 1);
                    auto file = std::make_shared<std::ofstream>(posfile, std::ios::binary);
                    if (!file->is_open()) {
                        std::cerr << "Failed to open file: " << posfile << std::endl;
                        return;
                    }
                    // 保存首个数据块（去掉文件名部分）
                    file->write(data.data() + pos + 1, length - pos - 1);

                    // 继续接收并保存文件内容
                    ReceiveFileContent(socket, file, posfile, [this](const std::string &filename, std::shared_ptr<tcp::socket> socket)
                                       {
                        HandleFileCompletion(filename, socket);
                    });
                }
            } else if (data.substr(0, 4) == "CMD ") {  // TODO: 目前这个功能不需要
                    std::string command = data.substr(4);
                    std::string output = ExecuteCommand(command);
                    std::cout << "Command executed: " << command << std::endl;
		            std::cout << "Output: " << output << std::endl;
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

void RemoteServer::ReceiveFileContent(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file, const std::string &file_name, std::function<void(const std::string &, std::shared_ptr<tcp::socket>)> on_complete)
{
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [this, socket, buffer, file, on_complete, file_name](boost::system::error_code ec, std::size_t length)
                            {
        if (!ec)
        {
            std::string received_data(buffer->data(), length);
            // 检查是否接收到结束信号
            if (received_data.find("END_OF_FILE\n") != std::string::npos)
            {
                file->write(buffer->data(), length - 12);
                file->close(); // 完成传输
                on_complete(file_name, socket);
            } else {
                file->write(buffer->data(), length);
            }
            ReceiveFileContent(socket, file, file_name, on_complete); // 继续接收剩余内容
        
        }
        else if (ec == boost::asio::error::eof)
        {
            file->close(); // 完成传输
            std::cout << "File received and saved successfully." << std::endl;
            on_complete(file_name, socket);
        }
        else
        {
            std::cerr << "Failed to receive file content: " << ec.message() << std::endl;
            if (file->is_open())
            {
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

fs::path RemoteServer::PerformVaspCompute(const std::string &poscar_name)
{
    std::string cwd = get_current_dir_name();
    // 为每次 VASP 计算准备一个新的目录、vasp对象
    Vasp *vasp = new Vasp(root_dir_.string(), data_dir_.string());

    vasp->PrepareDirectory("");

    std::cout << "Generating input files..." << std::endl;
    // this will move the POSCAR file to the compute directory and generate the input files
    vasp->GenerateInputFiles(poscar_name);

    std::cout << "Performing structure optimization..." << std::endl;
    vasp->PerformStructureOptimization();

    std::cout << "Performing static calculation..." << std::endl;
    vasp->PerformStaticCalculation();

    std::cout << "Performing dielectric calculation..." << std::endl;
    vasp->PerformDielectricCalculation();

    // std::cout << "Performing band structure calculation..." << std::endl;
    // vasp->PerformBandStructureCalculation();

    // std::cout << "Performing conductivity calculation..." << std::endl;
    // vasp->PerformConductivityCalculation();

    // std::cout << "Performing thermal expansion calculation..." << std::endl;
    // std::cout << "This could take a long time." << std::endl;
    // vasp->PerformThermalExpansionCalculation();

    std::cout << "VASP calculation complete." << std::endl;
    return vasp->StoreResults();
}
