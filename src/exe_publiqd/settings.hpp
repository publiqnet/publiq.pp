#pragma once

#include <boost/filesystem.hpp>

#include <string>
#include <memory>

namespace settings
{

boost::filesystem::path config_dir_path();
boost::filesystem::path data_dir_path();

void create_config_dir();
void create_data_dir();

boost::filesystem::path config_file_path(std::string const& file);
boost::filesystem::path data_file_path(std::string const& file);


class settings
{
public:
    static void set_data_dir(std::string const& data_dir);
    static std::string data_dir();

    static void set_application_name(std::string const& application_name);
    static std::string application_name();
private:
    static std::string s_data_dir;
    static std::string s_application_name;
};
}
