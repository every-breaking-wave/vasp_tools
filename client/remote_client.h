#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <array>
#include <memory>
#include "tool.h"

using boost::asio::ip::tcp;

std::string GetFileName(const std::string& filePath) {
    std::string fileName;
    size_t pos = filePath.find_last_of('/');
    if (pos != std::string::npos) {
        fileName = filePath.substr(pos + 1);
    }
    else {
        fileName = filePath;
    }
    return fileName;
}


class RemoteClient
{
public:
    RemoteClient(boost::asio::io_context &io_context, const std::string &server, int port)
        : socket_(io_context)
    {
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(server, std::to_string(port));
        boost::asio::connect(socket_, endpoints);
    }
    void send_file(const std::string &file_path)
    {
        auto file = std::make_shared<std::ifstream>(file_path, std::ios::binary);
        std::string file_name = GetFileName(file_path);
        std::string file_header = "FILE" + file_name + "\n";
        if (!file->is_open())
        {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return;
        }
        boost::asio::async_write(socket_, boost::asio::buffer(file_header),
                                 [this, file, file_name](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         send_file_content(file, &socket_, file_name);
                                     }
                                     else
                                     {
                                         std::cerr << "Failed to send file header: " << ec.message() << std::endl;
                                     }
                                 });
    }

    
    void send_command(const std::string &command)
    {
        std::string message = "CMD " + command;
        boost::asio::async_write(socket_, boost::asio::buffer(message),
                                 [this](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         receive_response();
                                     }
                                     else
                                     {
                                         std::cerr << "Failed to send command: " << ec.message() << std::endl;
                                     }
                                 });
    }

private:
    void receive_response()
    {
        auto buffer = std::make_shared<std::array<char, 1024>>();
        socket_.async_read_some(boost::asio::buffer(*buffer),
                                [this, buffer](boost::system::error_code ec, std::size_t length)
                                {
                                    if (!ec)
                                    {
                                        std::string response(buffer->data(), length);
                                        std::cout << "Server response: " << response << std::endl;
                                    }
                                    else
                                    {
                                        std::cerr << "Failed to receive response: " << ec.message() << std::endl;
                                    }
                                });
    }

    tcp::socket socket_;
};