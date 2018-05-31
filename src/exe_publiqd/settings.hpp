#pragma once

#include <boost/filesystem.hpp>

#include <string>
#include <memory>

namespace settings
{

boost::filesystem::path config_directory_path();
boost::filesystem::path data_directory_path();

void create_config_directory();
void create_data_directory();

boost::filesystem::path config_file_path(std::string const& file);
boost::filesystem::path data_file_path(std::string const& file);
boost::filesystem::path data_directory_path(std::string const& dir);


class settings
{
public:
    static void set_data_directory(std::string const& data_dir);
    static std::string data_directory();

    static void set_application_name(std::string const& application_name);
    static std::string application_name();
private:
    static std::string s_data_dir;
    static std::string s_application_name;
};
}
