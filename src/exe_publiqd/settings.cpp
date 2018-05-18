#include "settings.hpp"

#include <publiq.pp/global.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <vector>
#include <exception>
#include <fstream>

#if defined(P_OS_LINUX)

#include <cstdlib>

#elif defined(P_OS_WINDOWS)

#include <shlobj.h>

#elif defined(P_OS_MACOS)

#include <CoreServices/CoreServices.h>

#else
#error config_dir function has not been implemented for your platform!
#endif

namespace filesystem = boost::filesystem;
using std::string;
using std::runtime_error;
using std::fstream;
using std::vector;

namespace settings
{
/*
 *   Windows: C:\Users\username\AppData\Roaming\appname\
 *   Linux: /home/username/.config/appname/
 *   Mac: /Users/username/Library/Application Support/appname/
 */
filesystem::path config_dir_path()
{
#ifdef P_OS_LINUX
    char* home = getenv("XDG_CONFIG_HOME");
    if (nullptr == home)
    {
        home = getenv("HOME");
        if (nullptr == home)
            throw runtime_error("neither XDG_CONFIG_HOME nor HOME environment"
                                " variable is specified");
    }

    filesystem::path fs_home(home);

    if (fs_home.empty() ||
        false == filesystem::is_directory(fs_home))
        throw runtime_error("detected invalid directory");

    filesystem::path fs_config = fs_home / ".config";

#elif defined(P_OS_WINDOWS)
    wchar_t appdata[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata)))
        throw runtime_error("SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata)");

    filesystem::path fs_config(appdata);

#elif defined(P_OS_MACOS)
    FSRef ref;
    FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &ref);
    char config[PATH_MAX];
    FSRefMakePath(&ref, (UInt8 *)&config, PATH_MAX);

    filesystem::path fs_config(config);
#endif

    fs_config /= settings::application_name();

    return fs_config;
}

filesystem::path data_dir_path()
{
    filesystem::path fs_data(settings::data_dir());
    return filesystem::canonical(fs_data);
}

void create_config_dir()
{
    filesystem::create_directory(config_dir_path());
}
void create_data_dir()
{
    filesystem::create_directory(data_dir_path());
}

enum class config_data {config, data};
filesystem::path file_path(vector<string> const& dirs, string const& file, config_data cd)
{
    filesystem::path fs_config = config_dir_path();
    if (cd == config_data::data)
        fs_config = data_dir_path();

    for (auto const& dir : dirs)
    {
        fs_config /= dir;
        if (false == filesystem::exists(fs_config))
            filesystem::create_directory(fs_config);
    }

    fs_config /= file;

    return fs_config;
}

filesystem::path config_file_path(string const& file)
{
    return file_path(vector<string>(), file, config_data::config);
}

filesystem::path data_file_path(string const& file)
{
    return file_path(vector<string>(), file, config_data::data);
}

string settings::s_data_dir = config_dir_path().string();
string settings::s_application_name = "exe_publiqd";

void settings::set_data_dir(string const& data_dir)
{
    s_data_dir = data_dir;
}

string settings::data_dir()
{
    return s_data_dir;
}

void settings::set_application_name(string const& application_name)
{
    s_application_name = application_name;
}

string settings::application_name()
{
    return s_application_name;
}



/*bool save_interface_port(unsigned short port)
{
    Config::Port2PID a;
    Config::detail::loader();
}*/
}   //  end settings namespace
