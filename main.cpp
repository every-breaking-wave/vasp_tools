#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include "remote_server.h"


int main()
{
    try
    {
        boost::asio::io_context io_context;
        RemoteServer server(io_context, 12345);

        std::thread server_thread([&io_context]()
                                  { io_context.run(); });
        // server.SendFileToServer("/home/fyf/Materials/GaAs-0729/origin/POSCAR");
        // server.SendFileToServer("/home/fyf/Materials/SiO2/POSCAR");
        
        server_thread.join();

    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}