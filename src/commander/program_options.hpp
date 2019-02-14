#pragma once

#include <belt.pp/isocket.hpp>

#include <string>

bool process_command_line(int argc, char** argv,
                          std::string& prefix,
                          beltpp::ip_address& connect_to_address,
                          beltpp::ip_address& listen_on_address);
