#include "generator.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <cassert>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <exception>
#include <utility>
#include <iostream>
#include <string>
#include <algorithm>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::pair;
using std::runtime_error;

state_holder::state_holder()
    : map_types{{"String", "string"},
                {"Bool", "boolean"},
                {"Int8", "number"},
                {"UInt8", "number"},
                {"Int16", "number"},
                {"UInt16", "number"},
                {"Int", "number"},
                {"Int32", "number"},
                {"UInt32", "number"},
                {"Int64", "number"},
                {"UInt64", "number"},
                {"Float32", "number"},
                {"Float64", "number"},
                {"TimePoint", "Date"},
                {"Object", "::beltpp::packet"},
                {"Extension", "::beltpp::packet"}}
{
}

namespace
{
enum g_type_info {type_empty = 0x0,
                   type_simple = 0x1,
                   type_object = 0x2,
                   type_extension = 0x4,
                   type_simple_object = type_simple | type_object,
                   type_simple_extension = type_simple | type_extension,
                   type_object_extension = type_object | type_extension,
                   type_simple_object_extension = type_simple | type_object_extension};

string convert_type(string const& type_name, state_holder& state, g_type_info& type_detail)
{
    type_detail = type_empty;
    if ("Object" == type_name)
        type_detail = type_object;
    else if ("Extension" == type_name)
        type_detail = type_extension;
    else
        type_detail = type_simple;

    auto it_type_name = state.map_types.find(type_name);
    if (it_type_name != state.map_types.end())
        return it_type_name->second;
    return type_name;
}

void construct_type_name(   expression_tree const* member_type,
                         state_holder& state,
                         g_type_info& type_detail,
                         string* result)
{
    expression_tree const* member;
    if (member_type->lexem.rtt == keyword_optional::rtt &&
        member_type->children.size() == 1)
    {
        result[3] = "Optional";
        member =  member_type->children.front();
    }
    else
        member = member_type;

    if (member->lexem.rtt == identifier::rtt)
    {
        result[0] = convert_type(member->lexem.value, state, type_detail);
    }
    else if (member->lexem.rtt == keyword_array::rtt &&
             member->children.size() == 1 )
    {
        result[0] = "array";
        int count = 1;
        auto it = member->children.front();
        for(; it->lexem.rtt != identifier::rtt; it = it->children.front()){
            count++;
        }
        if(it->lexem.rtt == identifier::rtt)
        {
            result[1]=it->lexem.value;
        }
        result[2] = std::to_string(count);
    }
    else
        throw runtime_error("can't get type definition, wtf!");
}
}

string transformString( string const& scoreString )
{
    string camelString = scoreString;

    for ( size_t x = 0; x < camelString.length() - 1; x++ )
    {
        if ( camelString[x] == '_' )
        {
            char ch = camelString[x + 1];
            ch = char( toupper( int( ch ) ) );
            camelString.erase( x, 2 );
            camelString.insert( x, 1, ch );
        }
    }
    return camelString;
}

void analyze(   state_holder& state,
             expression_tree const* pexpression,
             std::string const& outputFilePath,
             std::string const& prefix)
{
    boost::filesystem::path root = outputFilePath;
    boost::filesystem::create_directory( root );

    size_t rtt = 0;
    assert(pexpression);
    unordered_map<size_t, string> class_names;
    vector <string> enum_names;

    if (pexpression->lexem.rtt != keyword_module::rtt ||
        pexpression->children.size() != 2 ||
        pexpression->children.front()->lexem.rtt != identifier::rtt ||
        pexpression->children.back()->lexem.rtt != scope_brace::rtt ||
        pexpression->children.back()->children.empty())
        throw runtime_error("wtf");
    else
    {

        for ( auto item : pexpression->children.back()->children )
        {
            if (item->lexem.rtt == keyword_enum::rtt)
            {
                if (item->children.size() != 2 ||
                    item->children.front()->lexem.rtt != identifier::rtt ||
                    item->children.back()->lexem.rtt != scope_brace::rtt)
                    throw runtime_error("enum syntax is wrong");

                enum_names.push_back(item->children.front()->lexem.value);
                string enum_name = item->children.front()->lexem.value;
                analyze_enum(   item->children.back(),
                             enum_name,
                             outputFilePath,
                             prefix );
            }
        }

        for (auto item : pexpression->children.back()->children)
        {
            if (item->lexem.rtt == keyword_class::rtt)
            {
                if (item->children.size() != 2 ||
                    item->children.front()->lexem.rtt != identifier::rtt ||
                    item->children.back()->lexem.rtt != scope_brace::rtt)
                    throw runtime_error("type syntax is wrong");

                string type_name = item->children.front()->lexem.value;
                analyze_struct( state,
                               item->children.back(),
                               type_name, enum_names,
                               outputFilePath,
                               prefix,
                               rtt);
                class_names.insert(std::make_pair(rtt, type_name));
                ++rtt;

            }
        }
    }

    if (class_names.empty())
        throw runtime_error("wtf, nothing to do");

    size_t max_rtt = 0;
    for (auto const& class_item : class_names)
    {
        if (max_rtt < class_item.first)
            max_rtt = class_item.first;
    }
    /////////////////////////// create ModelTypes file //////////////////////////////////
    boost::filesystem::path modelTypesPath = root.string() + "/" + "ModelTypes.ts" ;
    boost::filesystem::ofstream ModelTypes( modelTypesPath );

    for (size_t index = 0; index < max_rtt + 1; ++index)
    {
        if(!(class_names[index].empty()))
        {
            ModelTypes << "import "+ prefix + class_names[index] + " from './models/" + prefix + class_names[index] + "';\n";
        }
    }
    ModelTypes << "\n\nconst MODELS_TYPES = [ \n";
    for (size_t index = 0; index < max_rtt + 1; ++index)
    {
        if(!(class_names[index].empty()))
        {
            ModelTypes << "    " << prefix << class_names[index] << ",\n";
        }
    }
    ModelTypes << "];";


    ModelTypes << R"file_template(

export const createInstanceFromJson = data => {

  if(data.constructor.Rtt !== undefined){
      return  data;
  }

  if(data.rtt !== undefined){
      const ModelClass = MODELS_TYPES[data.rtt];

      if(!ModelClass){
          throw new Error("invalid model class");
      }

      return new ModelClass(data);
  }

  return  data;
};

export default MODELS_TYPES;)file_template";

}

void analyze_struct(    state_holder& state,
                    expression_tree const* pexpression,
                    string const& type_name,
                    std::vector<std::string> enum_names,
                    std::string const& outputFilePath,
                    std::string const& prefix,
                    size_t rtt )
{
    assert(pexpression);

    vector<pair<expression_tree const*, expression_tree const*>> members;

    if (pexpression->children.size() % 2 != 0)
        throw runtime_error("inside class syntax error, wtf - " + type_name);

    auto it = pexpression->children.begin();
    for (size_t index = 0; it != pexpression->children.end(); ++index, ++it)
    {
        auto const* member_type = *it;
        ++it;
        auto const* member_name = *it;

        if (member_name->lexem.rtt != identifier::rtt)
            throw runtime_error("inside class syntax error, wtf, still " + type_name);

        members.push_back(std::make_pair(member_name, member_type));
    }

    string import;
    import += "import BaseModel from '../BaseModel';\n\n";
    import += "import {createInstanceFromJson} from '../ModelTypes'\n\n";
    string params;
    string constructor;
    string memberNamesMap = "";
    vector <string> imported;

    for (auto member_pair : members)
    {
        auto const& member_name = member_pair.first->lexem;
        auto const& member_type = member_pair.second;

        if (member_name.rtt != identifier::rtt)
            throw runtime_error("use \"variable type\" syntax please");

        g_type_info type_detail;

        string info[4];
        construct_type_name(member_type, state, type_detail, info);
        string camelCaseMemberName = transformString( member_name.value );
        memberNamesMap += "            " + camelCaseMemberName  + " : '" + member_name.value + "',\n";
        string addToConstructor;
        if (camelCaseMemberName != member_name.value)
            addToConstructor = " === undefined ?  data." + camelCaseMemberName + ": data." + member_name.value;

        bool isEnum = false;

        if ( std::find( enum_names.begin(), enum_names.end(), info[0]) != enum_names.end() )
        {
            isEnum = true;

            if ( std::find( imported.begin(), imported.end(), info[0]) == imported.end() )
            {
                import += "import { " + prefix + info[0] + " } from './" + prefix + info[0] + "';\n";
                imported.push_back(info[0]);
            }
            if ( info[3] == "Optional")
            {
                params +=
                    "    " + camelCaseMemberName + "?: " + prefix + info[0] + ";\n";
                constructor +=
                    "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";}\n";
            }
            else
            {
                params +=
                    "    " + camelCaseMemberName + ": " + prefix + info[0] + ";\n";
                constructor +=
                    "            this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";\n";
            }
        }

        /////////////////////////// array of non primitive types ///////////////////
        if ( info[0] == "array" && info[1] != "number" && info[1] != "String" && info[1] != "boolean" && info[1] != "::beltpp::packet" )
        {
            if ( info[1] != "Date")
            {
                if ( std::find( imported.begin(), imported.end(), info[1]) == imported.end() )
                {
                    import += "import " + prefix + info[1] + " from './" + prefix + info[1] + "';\n";
                    imported.push_back(info[1]);
                }
                if ( info[3] == "Optional")
                    params +=
                        "    " + camelCaseMemberName + "?: Array<" + prefix + info[1] + ">;\n";
                else
                    params +=
                        "    " + camelCaseMemberName + ": Array<" + prefix + info[1] + ">;\n";

                if (camelCaseMemberName == member_name.value)
                {
                    if ( info[3] == "Optional")
                    {
                        constructor +=
                            "             if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + ".map(d => new " + prefix + info[1] + "(d));}\n";
                    }
                    else
                    {
                        constructor +=
                            "            this." + camelCaseMemberName + " = data." + member_name.value + ".map(d => new " + prefix + info[1] + "(d));\n";
                    }
                }
                else
                {
                    if ( info[3] == "Optional")
                    {
                        constructor +=
                            "             if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + " === undefined ? data." + camelCaseMemberName + ".map(d => new " + prefix + info[1] + "(d)) : data." + member_name.value + ".map(d => new " + prefix + info[1] + "(d));}\n";
                    }
                    else
                    {
                        constructor +=
                            "            this." + camelCaseMemberName + " = data." + member_name.value + " === undefined ? data." + camelCaseMemberName + ".map(d => new " + prefix + info[1] + "(d)) : data." + member_name.value + ".map(d => new " + prefix + info[1] + "(d));\n";
                    }
                }
            }
            else
            {
                if ( info[3] == "Optional")
                {
                    params +=
                        "    " + camelCaseMemberName + "?: Array<" + info[1] + ">;\n";

                    if (camelCaseMemberName == member_name.value)
                        constructor +=
                            "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + ".map(d => new " + info[1] + "(d));}\n";
                    else
                        constructor +=
                            "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + " === undefined ? data." + camelCaseMemberName + ".map(d => new " + info[1] + "(d)) : data." + member_name.value + ".map(d => new " + info[1] + "(d));}\n";
                }
                else
                {
                    params +=
                        "    " + camelCaseMemberName + ": Array<" + info[1] + ">;\n";

                    if (camelCaseMemberName == member_name.value)
                        constructor +=
                            "            this." + camelCaseMemberName + " = data." + member_name.value + ".map(d => new " + info[1] + "(d));\n";
                    else
                        constructor +=
                            "            this." + camelCaseMemberName + " = data." + member_name.value + " === undefined ? data." + camelCaseMemberName + ".map(d => new " + info[1] + "(d)) : data." + member_name.value + ".map(d => new " + info[1] + "(d));\n";
                }
            }
        }
        ////////////////////// array of objects //////////////////////////////
        else if ( info[0] == "array" && info[1] == "::beltpp::packet" )
        {
            if ( info[3] == "Optional")
            {
                params +=
                    "    " + camelCaseMemberName + "?: Array<Object>;\n";

                constructor +=
                    "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";}\n";
            }
            else
            {
                params +=
                    "    " + camelCaseMemberName + ": Array<Object>;\n";

                constructor +=
                    "            this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";\n";
            }
        }
        ////////////////////// array of primitive types /////////////////////
        else if ( info[0] == "array" && ( info[1] == "number" || info[1] == "String" || info[1] == "boolean" ) )
        {
            if ( info[3] == "Optional")
            {
                params +=
                    "    " + camelCaseMemberName + "?: Array<" + info[1] + ">;\n";

                constructor +=
                    "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";}\n";
            }
            else
            {
                params +=
                    "    " + camelCaseMemberName + ": Array<" + info[1] + ">;\n";

                constructor +=
                    "            this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";\n";
            }
        }
        ///////////////////////// non primitive type /////////////////////////
        else if ( info[0] != "number" && info[0] != "string" && info[0] != "boolean" && info[0] != "::beltpp::packet"  && !isEnum)
        {
            if ( info[0] != "Date")
            {
                if ( std::find( imported.begin(), imported.end(), info[0]) == imported.end() )
                {
                    import += "import " + prefix + info[0] + " from './" + prefix + info[0] + "';\n";
                    imported.push_back(info[0]);
                }
                if ( info[3] == "Optional")
                {
                    params +=
                        "    " + camelCaseMemberName + "?: " + prefix + info[0] + ";\n";

                    constructor +=
                        "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = new " + prefix + info[0] + "(data." + member_name.value + addToConstructor + ");}\n";
                }
                else
                {
                    params +=
                        "    " + camelCaseMemberName + ": " + prefix + info[0] + ";\n";

                    constructor +=
                        "            this." + camelCaseMemberName + " = new " + prefix + info[0] + "(data." + member_name.value + addToConstructor + ");\n";
                }
            }
            else
            {
                if ( info[3] == "Optional")
                {
                    params +=
                        "    " + camelCaseMemberName + "?: " + info[0] + ";\n";

                    constructor +=
                        "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = new " + info[0] + "(data." + member_name.value + addToConstructor + ");}\n";
                }
                else
                {
                    params +=
                        "    " + camelCaseMemberName + ": " + info[0] + ";\n";

                    constructor +=
                        "            this." + camelCaseMemberName + " = new " + info[0] + "(data." + member_name.value + addToConstructor + ");\n";
                }
            }

        }
        /////////////////////////// object type ///////////////////////////////
        else if ( info[0] == "::beltpp::packet")
        {
            if ( info[3] == "Optional")
            {
                params +=
                    "    " + camelCaseMemberName + "?: Object;\n";
                constructor +=
                    "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = createInstanceFromJson(data." + member_name.value + addToConstructor + ");}\n";
            }
            else
            {
                params +=
                    "    " + camelCaseMemberName + ": Object;\n";
                constructor +=
                    "            this." + camelCaseMemberName + " = createInstanceFromJson(data." + member_name.value + addToConstructor + ");\n";

            }
        }
        /////////////////////////// primitive type ///////////////////////////
        else if ( info[0] == "number" || info[0] == "string" || info[0] == "boolean" )
        {
            if ( info[3] == "Optional")
            {
                params +=
                    "    " + camelCaseMemberName + "?: " + info[0] + ";\n";
                constructor +=
                    "            if (data." + camelCaseMemberName + " !== undefined) { this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";}\n";
            }
            else
            {
                params +=
                    "    " + camelCaseMemberName + ": " + info[0] + ";\n";
                constructor +=
                    "            this." + camelCaseMemberName + " = data." + member_name.value + addToConstructor + ";\n";
            }
        }
    }

    /////////////////////////// create model files //////////////////////////////////
    boost::filesystem::path root = outputFilePath;
    string models = "models";
    root.append( models );
    boost::filesystem::create_directory( root );
    boost::filesystem::path FilePath = root.string() + "/" + prefix + type_name + ".ts";
    boost::filesystem::ofstream model( FilePath );
    model << import;
    model << "\nexport default class " + prefix +  type_name + " extends BaseModel {\n\n";
    model << params;
    model << "\n";
    model << "    constructor(data?: any) { \n";
    model << "        super();\n";
    model << "        if (data !== undefined) {\n";
    model << constructor;
    model << "        }\n";
    model << "    }\n\n";
    model << "    static get PropertyMap () {\n";
    model << "        return {\n";
    model << memberNamesMap;
    model << "        }\n";
    model << "    }\n\n";
    model << "    static get Rtt () {\n";
    model << "        return " << rtt <<";\n";
    model << "    }\n\n";
    model << "} \n";
}

void analyze_enum(  expression_tree const* pexpression,
                  string const& enum_name,
                  std::string const& outputFilePath,
                  std::string const& prefix)
{

    if (pexpression->children.empty())
        throw runtime_error("inside enum syntax error, wtf - " + enum_name);

    boost::filesystem::path root = outputFilePath;
    string models = "models";
    root.append( models );
    boost::filesystem::create_directory( root );
    boost::filesystem::path FilePath = root.string() + "/" + prefix + enum_name + ".ts";
    boost::filesystem::ofstream model( FilePath );

    model <<
        "export enum " + prefix + enum_name + " {\n";

    auto const& children = pexpression->children;
    auto iter = children.begin();

    while (iter != children.end())
    {
        if (iter == children.end() - 1)
            model<< "    " << transformString( (*iter)->lexem.value ) << "\n";
        else
            model<< "    " << transformString( (*iter)->lexem.value ) << ",\n";

        ++iter;
    }

    model << "} \n";
}
