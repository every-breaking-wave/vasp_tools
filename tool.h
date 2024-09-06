#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include <memory>
#include <iostream>

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
