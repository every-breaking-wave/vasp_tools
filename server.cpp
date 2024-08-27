// server.cpp (Linux)
#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <array>
#include <cstdlib>
#include <memory>
#include <thread>

using boost::asio::ip::tcp;

class RemoteServer
{
public:
    RemoteServer(boost::asio::io_context &io_context, int port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        auto socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
        acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec)
                               {
            if (!ec) {
                handle_connection(socket);
            }
            start_accept(); });
    }

    void handle_connection(std::shared_ptr<tcp::socket> socket)
    {
        auto buffer = std::make_shared<std::array<char, 1024>>();
        socket->async_read_some(boost::asio::buffer(*buffer), [this, socket, buffer](boost::system::error_code ec, std::size_t length)
                                {
        if (!ec) {
            std::string data(buffer->data(), length);
            
            if (data.substr(0, 4) == "FILE") {
                std::string::size_type pos = data.find('\n');
                if (pos != std::string::npos) {
                    std::string filename = data.substr(4, pos - 4);
                    auto file = std::make_shared<std::ofstream>(filename, std::ios::binary);
                    if (!file->is_open()) {
                        std::cerr << "Failed to open file: " << filename << std::endl;
                        return;
                    }
                    // 保存首个数据块（去掉文件名部分）
                    file->write(data.data() + pos + 1, length - pos - 1);

                    // 继续接收并保存文件内容
                    receive_file_content(socket, file);
                }
            } else if (data.substr(0, 4) == "CMD ") {
                    std::string command = data.substr(4);
                    std::string output = execute_command(command);
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

    void receive_file_content(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file)
    {
        auto buffer = std::make_shared<std::array<char, 1024>>();
        socket->async_read_some(boost::asio::buffer(*buffer), [this, socket, buffer, file](boost::system::error_code ec, std::size_t length)
                                {
        if (!ec) {
            file->write(buffer->data(), length);
            receive_file_content(socket, file); // 继续接收剩余内容
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

    std::string execute_command(const std::string &command)
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

    tcp::acceptor acceptor_;
};

int main()
{
    try
    {
        boost::asio::io_context io_context;
        RemoteServer server(io_context, 12345);

        std::thread server_thread([&io_context]()
                                  { io_context.run(); });
        server_thread.join();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
