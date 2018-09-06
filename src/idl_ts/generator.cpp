#include "generator.hpp"
#include "boost/filesystem.hpp"
#include <cassert>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <exception>
#include <utility>
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::pair;
using std::runtime_error;

state_holder::state_holder()
    : namespace_name()
    , map_types{{"String", "string"},
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
                type_object=0x2,
                type_extension=0x4,
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

void construct_type_name(expression_tree const* member_type,
                            state_holder& state,
                            g_type_info& type_detail,
                            string* result)
{
    if (member_type->lexem.rtt == identifier::rtt)
    {
        result[0] = convert_type(member_type->lexem.value, state, type_detail);
    }
    else if (member_type->lexem.rtt == keyword_array::rtt &&
             member_type->children.size() == 1 )
    {
        result[0] = "array";
        int count = 1;
        auto it = member_type->children.front();
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
    string camelString =  "";
    camelString = scoreString;

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
                std::string const& outputFilePath)
{
boost::filesystem::path root = outputFilePath;
B_UNUSED(pexpression);
B_UNUSED(state);
/*
/////////////////////////// create BaseModel file //////////////////////////////////
boost::filesystem::ofstream mapper( root.string() + "/" + "mapper.ts" );
mapper << R"file_template(import MODELS_TYPES from './ModelTypes';

export const parceToModel = jsonData => {
     const data = JSON.parse(jsonData);
     const ModelClass = MODELS_TYPES[data.rtt];
     if(!ModelClass){
         throw new Error("invalid model class");
     }
     return new ModelClass(data);
};

export const parceToJson = typedData => {
     return JSON.stringify(typedData.toJson())
};
)file_template";

/////////////////////////// create BaseModel file //////////////////////////////////
boost::filesystem::ofstream BaseModel( root.string() + "/" + "BaseModel.ts" );

BaseModel << R"file_template(import MODELS_TYPES from './ModelTypes';

 export default class BaseModel {

     static createInstanceFromJson (data) {
         const ModelClass = MODELS_TYPES[data.rtt];
         if(!ModelClass){
             throw new Error("invalid model class");
         }
         return new ModelClass(data);
     }

     static get PropertyMap () {
         return {}
     }

     static setProperty (propertyName, propertyValue, toObject, constructor) {
         const PropertyMap = constructor.PropertyMap || BaseModel.PropertyMap;
         const pn = PropertyMap[propertyName] || propertyName;
         toObject[pn] = propertyValue;
     }

     static hasRtt (type) {
         const rtt = BaseModel.getRtt(type);
         if(rtt === -1){
             return false
         }
         return true
     }

     static getRtt (type) {
         return MODELS_TYPES.indexOf(type);
     }

     static getDataWithRtt(data) {

         let dataWithRtt = {};

         for (let i in data) {
             const pv = data[i];
             const constructor = pv.constructor;
             let propertySetValue;

             if(constructor === Function){
                 continue;

             } else if (constructor === Array){
                 propertySetValue = data[i].map(d => BaseModel.getDataWithRtt(d));

             } else if(BaseModel.hasRtt(constructor)){
                 propertySetValue = BaseModel.getDataWithRtt(data[i]);

             } else {
                 propertySetValue = data[i];

             }

             BaseModel.setProperty(i, propertySetValue, dataWithRtt, data.constructor);
         }

         if(BaseModel.hasRtt(data.constructor)){
             dataWithRtt['rtt'] =  BaseModel.getRtt(data.constructor);
         }

         return dataWithRtt;
     }


     toJson() {
         return BaseModel.getDataWithRtt(this)
     }

 }
)file_template";

    size_t rtt = 0;
    assert(pexpression);
    unordered_map<size_t, string> class_names;
    if (pexpression->lexem.rtt != keyword_module::rtt ||
        pexpression->children.size() != 2 ||
        pexpression->children.front()->lexem.rtt != identifier::rtt ||
        pexpression->children.back()->lexem.rtt != scope_brace::rtt ||
        pexpression->children.back()->children.empty())
        throw runtime_error("wtf");
    else
    {
        string module_name = pexpression->children.front()->lexem.value;
        state.namespace_name = module_name;

        for (auto item : pexpression->children.back()->children)
        {
            if (item->lexem.rtt == keyword_class::rtt)
            {
                if (item->children.size() != 2 ||
                    item->children.front()->lexem.rtt != identifier::rtt ||
                    item->children.back()->lexem.rtt != scope_brace::rtt)
                    throw runtime_error("type syntax is wrong");

                string type_name = item->children.front()->lexem.value;
                analyze_struct(state, item->children.back(), type_name,  outputFilePath);
                class_names.insert(std::make_pair(rtt, type_name));

            }
            ++rtt;
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
    boost::filesystem::ofstream ModelTypes( root.string() + "/" + "ModelTypes.ts" );

    for (size_t index = 0; index < max_rtt + 1; ++index)
    {
        if(!(class_names[index].empty()))
        {
            ModelTypes << "import "+ class_names[index] + " from './models/" + class_names[index] + "';\n";
        }
    }
    ModelTypes << "\n\nconst MODELS_TYPES = [ \n";
    for (size_t index = 0; index < max_rtt + 1; ++index)
    {
        if(!(class_names[index].empty()))
        {
            ModelTypes << "    " << class_names[index] << ",\n";
        }
    }
    ModelTypes << "];";
    ModelTypes << "\n\n\nexport default MODELS_TYPES;";
*/
}

void analyze_struct(state_holder& state,
                      expression_tree const* pexpression,
                      string const& type_name,
                      std::string const& outputFilePath)
{
    if (state.namespace_name.empty())
        throw runtime_error("please specify package name");

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
    import += "import BaseModel from '../BaseModel';\n";
    string params;
    string constructor;
    string memberNamesMap = "";
    for (auto member_pair : members)
    {
        auto const& member_name = member_pair.first->lexem;
        auto const& member_type = member_pair.second;
        if (member_name.rtt != identifier::rtt)
            throw runtime_error("use \"variable type\" syntax please");

        g_type_info type_detail;

        string info[3];
        construct_type_name(member_type, state, type_detail, info);
        string camelCaseMemberName = transformString( member_name.value );
        memberNamesMap += "            " + camelCaseMemberName  + " : '" + member_name.value + "',\n";
        /////////////////////////// array of non primitive types ///////////////////
        if ( info[0] == "array" && info[1] != "number" && info[1] != "String" && info[1] != "boolean" && info[1] != "::beltpp::packet" )
        {
            if ( info[1] != "Date")
            {
                import += "import " + info[1] + " from './" + info[1] + "';\n";
            }
            params +=
                    "    " + camelCaseMemberName + ": Array<" + info[1] + ">;\n";
            constructor +=
                        "        this." + camelCaseMemberName + " = data." + member_name.value + ".map(d => new " + info[1] + "(d));\n";
        }

        ////////////////////// array of Objects ///////////////////////
        else if ( info[0] == "array" && info[1] == "::beltpp::packet" )
        {
            params +=
                    "    " + camelCaseMemberName + ": Array<Object>;\n";
            constructor +=
                        "        this." + camelCaseMemberName + " = data." + member_name.value + ";\n";
        }
        ////////////////////// array of primitive types /////////////////////
        else if ( info[0] == "array" && ( info[1] == "number" || info[1] == "String" || info[1] == "boolean" ) )
        {
            params +=
                    "    " + camelCaseMemberName + ": Array<" + info[1] + ">;\n";
            constructor +=
                        "        this." + camelCaseMemberName + " = data." + member_name.value + ";\n";
        }


        ///////////////////////// non primitive type /////////////////////////
        else if ( info[0] != "number" && info[0] != "string" && info[0] != "boolean" && info[0] != "::beltpp::packet" )
        {
            if ( info[0] != "Date")
            {
                import += "import " + info[0] + " from './" + info[0] + "';\n";
            }
            params +=
                    "    " + camelCaseMemberName + ": " + info[0] + ";\n";

            constructor +=
                        "        this." + camelCaseMemberName + " = new " + info[0] + "(data." + member_name.value + ");\n";

        }

        /////////////////////////// object type ///////////////////////////////
        else if ( info[0] == "::beltpp::packet")
        {
            params +=
                    "    " + camelCaseMemberName + ": Object;\n";
            constructor +=
                        "        this." + camelCaseMemberName + " = BaseModel.createInstanceFromJson(data." + member_name.value + ");\n";

        }

        /////////////////////////// primitive type ///////////////////////////
        else if ( info[0] == "number" || info[0] == "string" || info[0] == "boolean" )
        {
            params +=
                    "    " + camelCaseMemberName + ": " + info[0] + ";\n";
            constructor +=
                        "        this." + camelCaseMemberName + " = data." + member_name.value + ";\n";

        }
    }

    /////////////////////////// create model files //////////////////////////////////
    boost::filesystem::path root = outputFilePath;
//    string models = "models";
//    root.append( models );
//    boost::filesystem::create_directory( root );
//    boost::filesystem::path FilePath = root.string() + "/" + type_name + ".ts";
//    boost::filesystem::ofstream model( FilePath );
//    model << import;
//    model << "\nexport default class " + type_name + " extends BaseModel\n";
//    model << "{\n";
//    model << params;
//    model << "\n";
//    model << "    constructor(data) { \n";
//    model << "        super();\n";
//    model << constructor;
//    model << "    }\n\n";
//    model << "    static get PropertyMap () {\n";
//    model << "        return {\n";
//    model << memberNamesMap;
//    model << "        }\n";
//    model << "    }\n";
//    model << "} \n";
}
