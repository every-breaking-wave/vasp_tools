#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include <memory>
#include <iostream>
#include <vector>
#include <boost/filesystem.hpp>

using boost::asio::ip::tcp;

namespace fs = boost::filesystem;

void send_file_content(std::shared_ptr<std::ifstream> file, tcp::socket* socket);

void CopyFiles(const std::vector<fs::path> &source_files, const fs::path &destination_dir);

void CopyAllFiles(const fs::path &source_dir, const fs::path &destination_dir);

void RunCommand(const std::string &command);