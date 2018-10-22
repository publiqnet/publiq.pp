#include "generator.hpp"

#include <cassert>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <exception>
#include <utility>
#include<iostream>
#include<string>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;
using std::pair;
using std::runtime_error;


state_holder::state_holder()
    : namespace_name()
    , map_types{{"String", "string"},
                {"Bool", "bool"},
                {"Int8", "int"},
                {"UInt8", "int"},
                {"Int16", "int"},
                {"UInt16", "int"},
                {"Int", "int"},
                {"Int32", "int"},
                {"UInt32", "int"},
                {"Int64", "int"},
                {"UInt64", "int"},
                {"Float32", "float"},
                {"Float64", "double"},
                {"TimePoint", "integer"},
                {"Object", "::beltpp::packet"},
                {"Extension", "::beltpp::packet"}}
{
}

namespace
{

enum g_type_info {
    type_empty = 0x0,
    type_simple = 0x1,
    type_object = 0x2,
    type_extension = 0x4,
    type_simple_object = type_simple | type_object,
    type_simple_extension = type_simple | type_extension,
    type_object_extension = type_object | type_extension,
    type_simple_object_extension = type_simple | type_object_extension
};

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

string handleArrayForPrimitives( int count, string member_name )
{

    string arrayCase;
    string item = transformString( member_name ) + "Item";
    arrayCase +=
            "          foreach ($data->" + transformString( member_name ) + " as $" + item + ") { \n";

    for( int i = 1; i != count; i++)
    {
        arrayCase +=
                "          foreach ($" + item + " as $" + item + std::to_string( i ) + ") { \n";
        item = item + std::to_string( i );
    }
    arrayCase +=
            "            $this->add" + (static_cast<char>( member_name.at( 0 ) - 32 ) + transformString( member_name.substr( 1, member_name.length() - 1 ) ) ) + "($" + item + ");\n";

    for( int i = 1; i != count; i++)
    {
        arrayCase +=
                "           } \n";
    }
    arrayCase +=
            "           } \n";
    return arrayCase;
}

string handleArrayForObjects( int count, string member_name, string object_name )
{
    string camelCaseMemberName = transformString( member_name );

    string item = camelCaseMemberName +"Item";

    string arrayCase;
    arrayCase+=
            "          foreach ($data->" + camelCaseMemberName + " as $" + item + ") { \n";

    for ( int i = 1; i != count; i++ )
    {
        arrayCase +=
                "          foreach ($" + item + " as $" + item + std::to_string(i) + ") { \n";
        item = item + std::to_string( i );
    }

    if ( object_name == "::beltpp::packet")
    {
        arrayCase +=
                "    $this->" + camelCaseMemberName + " =    Rtt::validate($data->" +  member_name + ");\n";
    }
    else
    {
        arrayCase +=
                "              $" + item + "Obj = new " + object_name + "(); \n"
                                                                        "              $" + item + "Obj->validate($" + item + "); \n"+
                "              $this->" + camelCaseMemberName + "[] = $" + item + "Obj;\n";
    }


    for( int  i = 1; i != count; i++ )
    {
        arrayCase +=
                "           } \n";
    }
    arrayCase +=
            "           } \n";
    return arrayCase;

}

void handleHashForPrimitives( string info[], string& setFunction, string& hashCase, string member_name )
{
    string camelCaseMemberName = transformString( member_name );
    string camleCaseTypeName = transformString( info[3] );
    setFunction +=
            "    /**\n"
            "    * @var string\n"
            "    */\n"
            "    private $" + camelCaseMemberName + "Key;\n"
            "    /**\n"
            "    * @param string  $" + camelCaseMemberName + "Key\n"
            "    */\n"
            "    public function set" + camelCaseMemberName + "Key(string $" + camelCaseMemberName + "Key)\n"
            "    {\n"
            "        $this->" + camelCaseMemberName + "Key = $" + camelCaseMemberName + "Key;\n"
            "    }\n"

            "    /**\n"
            "    * @var " + camleCaseTypeName  + "\n"
            "    */\n"
            "    private $" + camelCaseMemberName + "Value;\n"
            "    /**\n"
            "    * @param " + camleCaseTypeName  + " $" + camelCaseMemberName + "Value\n"
            "    */\n"
            "    public function set" + camelCaseMemberName + "Value(" + camleCaseTypeName + " $value)\n"
            "    {\n"
            "        $this->" + camelCaseMemberName + "Value = $value;\n"
            "    }\n";

    hashCase +=
            "        foreach ($data->" + camelCaseMemberName + " as $key => $value) {\n"
            "            $this->set" + camelCaseMemberName + "Key($key);\n"
            "            $this->set" + camelCaseMemberName + "Value($value);\n"
            "        }\n";
}

void handleHashForObjects( string info[], string& setFunction, string& hashCase, string member_name )
{

    string camelCaseMemberName = transformString( member_name );
    setFunction +=
            "    /**\n"
            "    * @var string\n"
            "    */\n"
            "    private $" + camelCaseMemberName + "Key;\n"
            "    /**\n"
            "    * @param string  $" + camelCaseMemberName + "Key\n"
            "    */\n"
            "    public function set" + camelCaseMemberName + "Key(string  $" + camelCaseMemberName + "Key)\n"
            "    {\n"
            "        $this->" + camelCaseMemberName + "Key = $" + camelCaseMemberName + "Key;\n"
            "    }\n";

    string item = camelCaseMemberName + "Item";

    if ( info[3] == "::beltpp::packet" )
    {
        hashCase +=
                "        foreach ($data->" + camelCaseMemberName + " as $key => $value) {\n"
                "            $this->set" + camelCaseMemberName + "Key($key);\n"
                "            $this->" + camelCaseMemberName + " = Rtt::validate($data->" +  member_name + ");\n"
                "         }\n";

    }
    else
    {

        hashCase +=
                "        foreach ($data->" + camelCaseMemberName + " as $key => $value) {\n"
                "            $this->set" + camelCaseMemberName + "Key($key);\n"
                "            $hashItemObj = new " + info[3] + "();\n"
                "            $hashItemObj->validate($value);\n"
                "            $this->" + camelCaseMemberName + "[] = $" + item + "Obj"
                "         }\n";
    }

}

string convert_type( string const& type_name, state_holder& state, g_type_info& type_detail )
{
    type_detail = type_empty;
    if ( "Object" == type_name )
        type_detail = type_object;
    else if ( "Extension" == type_name )
        type_detail = type_extension;
    else
        type_detail = type_simple;

    auto it_type_name = state.map_types.find( type_name );
    if ( it_type_name != state.map_types.end() )
        return it_type_name->second;
    return type_name;
}

void construct_type_name( expression_tree const* member_type,
                          state_holder& state,
                          g_type_info& type_detail,
                          string* result )
{
    if ( member_type->lexem.rtt == identifier::rtt )
    {
        result[0] = convert_type( member_type->lexem.value, state, type_detail );
    }
    else if ( member_type->lexem.rtt == keyword_array::rtt &&
             member_type->children.size() == 1 )
    {
        result[0] = "array";
        int count = 1;
        auto it = member_type->children.front();
        for( ; it->lexem.rtt != identifier::rtt; it = it->children.front() )
        {
            count++;
        }
        if( it->lexem.rtt == identifier::rtt )
        {
            result[1] = convert_type( it->lexem.value, state, type_detail );
        }
        result[2] = std::to_string( count );
    }
    else
    if ( member_type->lexem.rtt == keyword_hash::rtt &&
         member_type->children.size() == 2 )
    {
        int count = 1;
        auto it = member_type->children.front();
        for( ; it->lexem.rtt != identifier::rtt; it = it->children.front() )
        {
            count++;
        }
        if( it->lexem.rtt == identifier::rtt )
        {
            result[1] = convert_type( it->lexem.value, state, type_detail );
        }

        result[0] = "hash";
        result[2] = std::to_string( count );
        result[3] = convert_type( member_type->children.back()->lexem.value, state, type_detail );

    }
    else
        throw runtime_error( "can't get type definition, wtf!" );
}
}

void analyze(   state_holder& state,
                expression_tree const* pexpression,
                std::string const& outputFilePath,
                std::string const& PackageName )
{

    size_t rtt = 0;
    assert( pexpression );

    //////////////// create Interface Folder ////////////////


    boost::filesystem::path root = outputFilePath;

    root.append( PackageName );
    boost::filesystem::create_directory( root );

    std::string src = "src";
    root.append( src );
    boost::filesystem::create_directory( root );

    //////////////// create Base Folder ////////////////////
    boost::filesystem::path BaseFolderPath = root.string() + "/" + "Base";
    boost::filesystem::create_directory( BaseFolderPath );

    boost::filesystem::path ValidatorInterfaceFilePath = BaseFolderPath.string() + "/" + "ValidatorInterface.php";
    boost::filesystem::ofstream validator( ValidatorInterfaceFilePath );

    validator << "<?php\n" <<
                "namespace " << PackageName << "\\Base;\n";
    validator << R"file_template(
interface ValidatorInterface
{
    public function validate(\stdClass $data);
}
)file_template";

    ///////////////////////////////////////////////////////

    unordered_map<size_t, string> class_names;
    if ( pexpression->lexem.rtt != keyword_module::rtt ||
         pexpression->children.size() != 2 ||
         pexpression->children.front()->lexem.rtt != identifier::rtt ||
         pexpression->children.back()->lexem.rtt != scope_brace::rtt ||
         pexpression->children.back()->children.empty() )
        throw runtime_error( "wtf" );
    else
    {
        //////////////// create Model Folder ////////////////////

        std::string Model = "Model";
        boost::filesystem::path ModelFolder = outputFilePath + "/" + PackageName + "/" + src + "/" + Model;
        boost::filesystem::create_directory( ModelFolder );

        ////////////////////////////////////////////////////////

        string module_name = pexpression->children.front()->lexem.value;
        state.namespace_name = module_name;

        for ( auto item : pexpression->children.back()->children )
        {
            if ( item->lexem.rtt == keyword_class::rtt )
            {
                if (    item->children.size() != 2 ||
                        item->children.front()->lexem.rtt != identifier::rtt ||
                        item->children.back()->lexem.rtt != scope_brace::rtt )
                    throw runtime_error( "type syntax is wrong" );

                string type_name = item->children.front()->lexem.value;
                analyze_struct(     state,
                                    item->children.back(),
                                    type_name,
                                    PackageName,
                                    ModelFolder );

                class_names.insert( std::make_pair( rtt, type_name ) );

            }
            ++rtt;
        }
    }

    if ( class_names.empty() )
        throw runtime_error( "wtf, nothing to do" );

    size_t max_rtt = 0;
    for ( auto const& class_item : class_names )
    {
        if (max_rtt < class_item.first)
            max_rtt = class_item.first;
    }

    std::string Base = "Base";
    boost::filesystem::path BaseFolder = outputFilePath + "/" + PackageName + "/" + src + "/" + Base;
    boost::filesystem::create_directory( BaseFolder );

    ///////////////// create Rtt.php file ////////////////////

    boost::filesystem::path RttFilePath = BaseFolderPath.string() + "/" + "Rtt.php";
    boost::filesystem::ofstream RTT( RttFilePath );
    RTT << "<?php\n" <<
           "namespace "  << PackageName << "\\Base;\n";
    RTT <<
          R"file_template(
class Rtt
{
    CONST types = [
)file_template";


    for ( size_t index = 0; index < max_rtt + 1; ++index )
    {
        if ( !(class_names[index].empty()) )
            RTT <<  "\t" + std::to_string(index) + " => '" + class_names[index] + "',\n";

    }
    RTT <<
R"file_template(  ];

/**
* @param string|object $jsonObj
* @return bool|string
*/
public static function validate($jsonObj)
{
    if (!is_object($jsonObj)) {
        $jsonObj = json_decode($jsonObj);
        if ($jsonObj === null) {
            return false;
        }
    }

    if (!isset($jsonObj->rtt)) {
        return false;
    }

    if (!isset(Rtt::types[$jsonObj->rtt])) {
        return false;
    }
    try {
        $className = ')file_template";
    RTT << PackageName<<"\\\\Model\\\\' . Rtt::types[$jsonObj->rtt];";
    RTT <<
R"file_template(
        /**
        * @var ValidatorInterface $class
        */
        $class = new $className;
        $class->validate($jsonObj);
        return $class;
        } catch (\Throwable $e) {
            return $e->getMessage();
        }
    }
})file_template";

    ////////////////// create RttSerializableTrait.php file /////////////////

    boost::filesystem::path RttSerializableTraitFilePath = BaseFolderPath.string() + "/" + "RttSerializableTrait.php";
    boost::filesystem::ofstream RttSerializableTrait( RttSerializableTraitFilePath );
    RttSerializableTrait << "<?php\n" <<
                           "namespace " << PackageName << "\\Base;\n";
    RttSerializableTrait <<
R"file_template(
trait RttSerializableTrait
{
    public function jsonSerialize()
    {
        $vars = get_object_vars($this);

        $path = explode('\\', static::class);
        $className = array_pop($path);
        if (!$className) {
            throw new \Exception("Cannot find class in rtt list");
        }
        $vars2['rtt'] = array_search($className, Rtt::types);

        foreach ($vars as  $name => $value)
        {
            $member = (static::class)::getMemberName($name);
            if ($member['convertToDate']) {
                $vars2[$member['key']] = date("Y-m-d H:i:s", $value);
            } else {
                $vars2[$member['key']] = $value;
            }
        }
        return $vars2;
    }
}
)file_template";

    ////////////////// create RttToJsonTrait.php file /////////////////

    boost::filesystem::path RttToJsonTraitFilePath = BaseFolderPath.string() + "/" + "RttToJsonTrait.php";
    boost::filesystem::ofstream RttToJsonTrait( RttToJsonTraitFilePath );
    RttToJsonTrait << "<?php\n" <<
                     "namespace " << PackageName << "\\Base;\n";
    RttToJsonTrait <<
R"file_template(
trait RttToJsonTrait
{
    public function convertToJson()
    {
        return json_encode($this);
    }
}
)file_template";

}


void analyze_struct(    state_holder& state,
                        expression_tree const* pexpression,
                        string const& type_name,
                        std::string const& PackageName,
                        boost::filesystem::path const& ModelFolder
                        )
{
    if ( state.namespace_name.empty() )
        throw runtime_error( "please specify package name" );

    assert( pexpression );

    vector<pair<expression_tree const*, expression_tree const*>> members;

    if ( pexpression->children.size() % 2 != 0 )
        throw runtime_error( "inside class syntax error, wtf - " + type_name );

    auto it = pexpression->children.begin();
    for ( size_t index = 0; it != pexpression->children.end(); ++index, ++it )
    {
        auto const* member_type = *it;
        ++it;
        auto const* member_name = *it;

        if ( member_name->lexem.rtt != identifier::rtt )
            throw runtime_error( "inside class syntax error, wtf, still " + type_name );

        members.push_back( std::make_pair( member_name, member_type) );
    }


    /////////////////////
    string params;
    string setFunction;
    string getFunction;
    string addFunction;
    string arrayCase;
    string trivialTypes;
    string objectTypes;
    string mixedTypes;
    string hashCase;

    /////////////////////////// create model files //////////////////////////////////

    boost::filesystem::path FilePath = ModelFolder.string() + "/" + type_name + ".php";
    boost::filesystem::ofstream model( FilePath );
    model <<
            "<?php\n"

            "namespace " + PackageName + "\\Model;\n" +
            "use " + PackageName +  "\\Base\\RttSerializableTrait;\n" +
            "use " + PackageName +  "\\Base\\RttToJsonTrait;\n" +
            "use " + PackageName +  "\\Base\\ValidatorInterface;\n" +
            "use " + PackageName +  "\\Base\\Rtt;\n" +



            "class " + type_name + " implements ValidatorInterface, \\JsonSerializable\n"+
            "{\n" +
            "    use RttSerializableTrait;\n" +
            "    use RttToJsonTrait;\n";

    string memberNamesMap = "";
    for ( auto member_pair : members )
    {

        auto const& member_name = member_pair.first->lexem;
        auto const& member_type = member_pair.second;
        if ( member_name.rtt != identifier::rtt )
            throw runtime_error( "use \"variable type\" syntax please" );

        g_type_info type_detail;

        string info[4];
        construct_type_name( member_type, state, type_detail, info );

        string camelCaseMemberName = transformString( member_name.value );

        memberNamesMap += "        '" + member_name.value + "'" + " => ['name' => '" + camelCaseMemberName + "', 'convertToDate' => ";
        if ( info[0] == "integer")
        {
            memberNamesMap += "true],\n";
        }
        else
        {
            memberNamesMap += "false],\n";
        }
        if ( info[0] == "::beltpp::packet" )
        {

            params +=
                    "    /**\n"
                    "    * @var mixed \n"
                    "    */ \n"
                    "    private $" + camelCaseMemberName + ";\n";

            mixedTypes +=
                    "          $this->" + camelCaseMemberName + " = Rtt::validate($data->" +  member_name.value + ");\n";
        }
        else
        if ( info[0] == "hash" || info[0] == "array" )
        {
            params +=
                    "    /**\n"
                    "    * @var array\n"
                    "    */ \n"
                    "    private $" + camelCaseMemberName + " = [];\n";
        }
        else
        {
            params +=
                    "    /**\n"
                    "    * @var "+ transformString( info[0] ) + "\n" +
                    "    */ \n" +
                    "    private $" + camelCaseMemberName + ";\n";
        }
        if (     info[0] == "array" &&
                ( info[1] != "int" &&
                  info[1] != "string" &&
                  info[1] != "bool" &&
                  info[1] != "float" &&
                  info[1] != "double" &&
                  info[1] != "integer"
                  )
                )
        {
            arrayCase += handleArrayForObjects( std::stoi( info[2] ), member_name.value, info[1] );

        }
        else if (
                 (              info[0] == "array" &&
                                (  info[1] == "int" ||
                                   info[1] == "string" ||
                                   info[1] == "bool" ||
                                   info[1] == "float" ||
                                   info[1] == "double" ||
                                   info[1] == "integer"
                                   )
                                )
                 )
        {

            string item = camelCaseMemberName + "Item";
            addFunction +=
                    "    /**\n"
                    "    * @param " + transformString( info[1] ) + " $" + item +"\n"
                    "    */\n"
                    "    public function add" + (static_cast<char>( member_name.value.at( 0 ) - 32 ) + transformString( member_name.value.substr( 1, member_name.value.length() - 1 ) ) )  +  "(" + info[1] + " $" + item + ")\n"
                    "    {\n"
                    "        $this->" + camelCaseMemberName + "[] = $" + item + ";\n"
                                                                                                                                                                                                                                                                                                                                 "    }\n";


            arrayCase += handleArrayForPrimitives( std::stoi( info[2] ), member_name.value );

        }
        if (     info[0] == "hash" &&
                ( info[3] != "int" &&
                  info[3] != "string" &&
                  info[3] != "bool" &&
                  info[3] != "float" &&
                  info[3] != "double" &&
                  info[3] != "integer"
                  )
                )
        {

            handleHashForObjects( info, setFunction, hashCase, member_name.value );

        }
        else if (
                 (              info[0] == "hash" &&
                                (  info[3] == "int" ||
                                   info[3] == "string" ||
                                   info[3] == "bool" ||
                                   info[3] == "float" ||
                                   info[3] == "double" ||
                                   info[3] == "integer"
                                   )
                                )
                 )
        {
            handleHashForPrimitives( info, setFunction, hashCase, member_name.value  );

        }

        else if (
                 info[0] == "int" ||
                 info[0] == "string" ||
                 info[0] == "bool" ||
                 info[0] == "float" ||
                 info[0] == "double" ||
                 info[0] == "integer"
                 )
        {

            string type;
            if (info[0] == "integer")
            {
                trivialTypes +=
                        "        $this->set" +   ( static_cast<char>( member_name.value.at( 0 ) - 32 ) +  transformString( member_name.value.substr( 1, member_name.value.length() - 1 ) ) ) + "(strtotime($data->" + member_name.value  + ")); \n";

                type = "int";
            }
            else
                type = info[0];

            setFunction +=
                    "    /** \n"
                    "    * @param " + type + " $" + camelCaseMemberName + "\n"
                    "    */ \n"
                    "    public function set" + static_cast<char>( member_name.value.at( 0 ) - 32 ) + transformString( member_name.value.substr( 1, member_name.value.length() - 1 ) ) + "(" + type +" $" + transformString( member_name.value ) + ") \n"
                    "    { \n"
                    "       $this->" + camelCaseMemberName + " = $" + camelCaseMemberName + ";\n"
                    "    }\n";

            if (info[0] != "integer")
            {
                trivialTypes +=
                        "        $this->set" + ( static_cast<char>( member_name.value.at( 0 ) - 32 ) + transformString( member_name.value.substr( 1, member_name.value.length() - 1 ) ) ) + "($data->" + member_name.value  + "); \n";
            }

        }
        else
        if ( !( info[0] == "::beltpp::packet" )  && !( info[0] == "hash" ) && !(info[0] == "array") )
        {
            objectTypes +=
                    "        $this->" + camelCaseMemberName + " = new "+ info[0] + "();\n"
                    "        $this->" + camelCaseMemberName + " -> validate($data-> "+ member_name.value  + ");\n";
        }
        getFunction +=
                "    public function get" + ( static_cast<char>( member_name.value.at( 0 ) - 32 ) + transformString( member_name.value.substr( 1, member_name.value.length() - 1 ) ) ) + "() \n"
                "    {\n"
                "        return $this->" + camelCaseMemberName + ";\n"
                "    }\n";
    }

    string  validation =
            "    public function validate(\\stdClass $data) \n"
            "    { \n"
                    + objectTypes + trivialTypes + arrayCase + mixedTypes + hashCase +
            "    } \n";
    model << " \n    CONST  memberNames = [\n"
            + memberNamesMap +
            "    ];\n\n";
    model << params + setFunction + getFunction + addFunction + validation;
    model <<
R"file_template(    public static function getMemberName(string $camelCaseName)
    {
        foreach (self::memberNames as $key => $value) {
               if ($value['name'] == $camelCaseName) {
                   $value['key'] = $key;
                   return $value;
               }
       }
    }
)file_template";
    model << "\n} \n";

    /////////////////

}
