
#include "remote_server.h"
#include "time.h"
#include "tool.h"

using boost::asio::ip::tcp;

#define HISTORY 1

void RemoteServer::HandleFileCompletion(const std::string &filename, std::shared_ptr<tcp::socket> socket)
{
    if (fs::is_regular_file(filename) && filename.substr(0, 6) == "POSCAR")
    {
        std::vector<std::string> result_files = PerformVaspCompute(filename);

        for (auto result_file : result_files)
        {
            // 发送文件名
            //  std::string file_name = "FILE " + result_file.filename().string() + "\n";
            //  boost::asio::write(*socket, boost::asio::buffer(file_name));
            // resultfile 是绝对路径,要提取文件名
            std::string file_name = result_file.substr(result_file.find_last_of("/") + 1);
            std::cout << "Sending file: " << result_file << std::endl;
            auto result_file_stream = std::make_shared<std::ifstream>(result_file, std::ios::binary);
            send_file_content(result_file_stream, socket.get(), file_name);
            // END_OF_FILE 用于标记文件结束
        }
            std::string endfile = "END_OF_FILE";
            // 创建这个文件
            std::ofstream endfilestream(endfile);
            endfilestream.close();
            auto endfilestream_ptr = std::make_shared<std::ifstream>(endfile, std::ios::binary);
            send_file_content(endfilestream_ptr, socket.get(), endfile);
            fs::remove(endfile);
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
        send_file_content(file, &socket, filename);
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

std::vector<std::string> RemoteServer::PerformVaspCompute(const std::string &poscar_name)
{
    std::string cwd = get_current_dir_name();
    // 为每次 VASP 计算准备一个新的目录、vasp对象
    Vasp *vasp = new Vasp(root_dir_.string(), data_dir_.string());

    if (HISTORY)
    {
        vasp->UseHistoryOptDir();
    }
    else
    {
        {
            vasp->PrepareDirectory("");

            std::cout << "Generating input files..." << std::endl;
            // this will move the POSCAR file to the compute directory and generate the input files
            vasp->GenerateInputFiles(poscar_name);

            std::cout << "Performing structure optimization..." << std::endl;
            vasp->PerformStructureOptimization();

            std::cout << "Performing static calculation..." << std::endl;
            vasp->PerformStaticCalculation();
        }
    }

    vasp->GetDensity();

    std::cout << "Performing dielectric calculation..." << std::endl;
    vasp->PerformDielectricCalculation();

    std::cout << "Performing band structure calculation..." << std::endl;
    vasp->PerformBandStructureCalculation();

    std::cout << "Performing conductivity calculation..." << std::endl;
    vasp->PerformConductivityCalculation();

    std::cout << "Performing thermal expansion calculation..." << std::endl;
    std::cout << "This could take a long time." << std::endl;
    vasp->PerformThermalExpansionCalculation();

    std::cout << "VASP calculation complete." << std::endl;
    return vasp->StoreResults();
}
