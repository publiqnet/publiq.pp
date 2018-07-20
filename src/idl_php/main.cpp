#include "generator.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <exception>
#include <streambuf>
#include <cassert>

using std::cout;
using std::endl;
using std::string;
using std::runtime_error;
using std::ofstream;
using std::ifstream;
using std::vector;

using ptr_expression_tree = std::unique_ptr<expression_tree>;

int main(int argc, char* argv[])
{
    string definition;
    ptr_expression_tree ptr_expression;
    try
    {
        if (argc >= 2)
        {
            ifstream file_definition(argv[1]);
            if (file_definition)
            {
                file_definition.seekg(0, std::ios::end);
                definition.reserve(file_definition.tellg());
                file_definition.seekg(0, std::ios::beg);

                definition.assign((std::istreambuf_iterator<char>(file_definition)),
                                  std::istreambuf_iterator<char>());
                file_definition.close();
            }
        }

        if (definition.empty())
        {
            definition = "module beltpp {"
                                "class Name"
                                "{"
                                    "String value "
                                 "}"
                                "type bbbjbj"
                                "{"
                                     "Array Person person"

                                 "}"
                                 "class Age"
                                 "{"
                                     "UInt32 value "
                                  "}"
                                 "class Married"
                                 "{"
                                     "Bool value"
                                  "}"
                                  "type jbbk"
                                  "{"
                                     "Array Array Person person"

                                  "}"
                                  "class Person"
                                  "{"
                                     "Name name "
                                     "Age age "
                                     "Married married "
                                     "String gender "
                                  "}"
                                  "class Group"
                                  "{"
                                     "TimePoint time "
                                     "Array Array Array Person person "
                                     "Array Array Array int per "
                                     "Array int pers "
                                  "}"
                                  "type jbbk"
                                  "{"
                                      "Array Array Array Array Person person "
                                   "}"
                            "}";
        }

        auto it_begin = definition.begin();
        auto it_end = definition.end();
        auto it_begin_keep = it_begin;
        while (beltpp::parse(ptr_expression, it_begin, it_end))
        {
            if (it_begin == it_begin_keep)
                break;
            else
            {
                it_begin_keep = it_begin;
            }
        }

        bool is_value = false;
        auto proot = beltpp::root(ptr_expression.get(), is_value);
        ptr_expression.release();
        ptr_expression.reset(proot);

        if (false == is_value)
            throw runtime_error("missing expression, apparently");

        if (it_begin != it_end)
            throw runtime_error("syntax error, maybe: " + string(it_begin, it_end));

        if (ptr_expression->depth() > 30)
            throw runtime_error("expected tree max depth 30 is exceeded");

        state_holder state;
        string generated = analyze(state, ptr_expression.get());

        bool generation_success = false;
        if (argc >= 3)
        {
            ofstream file_generate(argv[2]);
            if (file_generate)
            {
                file_generate << generated;
                file_generate.close();
                generation_success = true;
            }
        }
        if (false == generation_success){
            cout << generated;
        }
    }
    catch(std::exception const& ex)
    {
        cout << "exception: " << ex.what() << endl;

        if (ptr_expression)
        {
            cout << "=====\n";
            cout << beltpp::dump(ptr_expression.get()) << endl;
        }

        return 2;
    }
    catch(...)
    {
        cout << "that was an exception" << endl;
        return 3;
    }
    return 0;
}
