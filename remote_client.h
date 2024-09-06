#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <array>
#include <memory>
#include "tool.h"

using boost::asio::ip::tcp;

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
    void send_file(const std::string &filename)
    {
        auto file = std::make_shared<std::ifstream>(filename, std::ios::binary);
        if (!file->is_open())
        {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        std::string file_header = "FILE" + filename + "\n";
        boost::asio::async_write(socket_, boost::asio::buffer(file_header),
                                 [this, file](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         send_file_content(file, &socket_);
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