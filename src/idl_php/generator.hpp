#pragma once

#include <belt.pp/idl_parser.hpp>

#include <string>
#include <unordered_map>

using expression_tree = beltpp::expression_tree<lexers, std::string>;

class state_holder
{
public:
    state_holder();
    std::string namespace_name;
    std::unordered_map<std::string, std::string> map_types;
};

std::string analyze(state_holder& state,
                    expression_tree const* pexpression);

std::string analyze_struct(state_holder& state,
                           expression_tree const* pexpression,
                           std::string const& type_name);
