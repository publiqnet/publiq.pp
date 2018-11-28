#pragma once

#include <belt.pp/idl_parser.hpp>

#include <string>
#include <unordered_map>

using expression_tree = beltpp::expression_tree<lexers, std::string>;

class state_holder
{
public:
    state_holder();
    std::unordered_map<std::string, std::string> map_types;
};

void analyze(               state_holder& state,
                            expression_tree const* pexpression,
                            std::string const& outputFilePath,
                            std::string const& prefix);

void analyze_struct(        state_holder& state,
                            expression_tree const* pexpression,
                            std::string const& type_name,
                            std::vector<std::string> enum_names,
                            std::string const& outputFilePath,
                            std::string const& prefix,
                            size_t rtt);

void analyze_enum(          expression_tree const* pexpression,
                            std::string const& enum_name,
                            std::string const& outputFilePath,
                            std::string const& prefix);
