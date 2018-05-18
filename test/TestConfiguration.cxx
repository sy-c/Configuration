/// \file TestConfiguration.cxx
/// \brief Unit tests for the Configuration.
///
/// \author Sylvain Chapeland, CERN
/// \author Pascal Boeschoten, CERN
///
/// \todo Clean up
/// \todo Test all backends in uniform way

#include <fstream>
#include <iostream>
#include <unordered_map>
#include "Configuration/ConfigurationFactory.h"
#include "Configuration/ConfigurationInterface.h"
#include "Configuration/Visitor.h"
#include "Configuration/Tree.h"

#define BOOST_TEST_MODULE hello test
#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <assert.h>

using namespace std::literals::string_literals;
using namespace o2::configuration;

namespace {

BOOST_AUTO_TEST_CASE(TestIllFormedUri)
{
  try {
    ConfigurationFactory::getConfiguration("file:/bad/uri.ini");
  }
  catch (const std::runtime_error &e) {
    BOOST_CHECK(std::string(e.what()).find("Ill-formed URI") != std::string::npos);
  }
}

BOOST_AUTO_TEST_CASE(IniFileTest)
{
  // Put stuff in temp file
  const std::string TEMP_FILE = "/tmp/alice_o2_configuration_test_file.ini";
  {
    std::ofstream stream(TEMP_FILE);
    stream <<
      "key=value\n"
        "[section]\n"
        "key_int=123\n"
        "key_float=4.56\n"
        "key_string=hello\n";
  }

  // Get file configuration interface from factory
  auto conf = ConfigurationFactory::getConfiguration("file:/" + TEMP_FILE);

  std::string key{"/test/key"};
  std::string value{"test_value"};

  // Check with nonexistant keys
  BOOST_CHECK_NO_THROW(BOOST_CHECK(conf->get<int>("this_is/a_bad/key").get_value_or(-1) == -1));

  // File backend does not support putting values
  BOOST_CHECK_THROW(conf->put(key, value), std::runtime_error);

  // Check with default separator
  BOOST_CHECK(conf->get<std::string>("key").get_value_or("") == "value");
  BOOST_CHECK(conf->get<int>("section/key_int").get_value_or(-1) == 123);
  BOOST_CHECK(conf->get<double>("section/key_float").get_value_or(-1.0) == 4.56);
  BOOST_CHECK(conf->get<std::string>("section/key_string").get_value_or("") == "hello");


  // Check with custom separator
  conf->setPathSeparator('.');
  BOOST_CHECK(conf->get<std::string>("key").get_value_or("") == "value");
  BOOST_CHECK(conf->get<int>("section.key_int").get_value_or(-1) == 123);
  BOOST_CHECK(conf->get<double>("section.key_float").get_value_or(-1.0) == 4.56);
  BOOST_CHECK(conf->get<std::string>("section.key_string").get_value_or("") == "hello");
}

#ifdef FLP_CONFIGURATION_BACKEND_FILE_JSON_ENABLED
BOOST_AUTO_TEST_CASE(JsonFileTest)
{
  const std::string TEMP_FILE = "/tmp/alice_o2_configuration_test_file.json";
  {
    std::ofstream stream(TEMP_FILE);
    stream << R"({"menu": {
      "id": "file",
      "popup": {
        "menuitem": {
          "one": {"value": "123", "onclick": "CreateNewDoc"}
        }
      }
    }})";
  }

  auto conf = ConfigurationFactory::getConfiguration("json:/" + TEMP_FILE);

  BOOST_CHECK(conf->get<std::string>("menu/id").get_value_or("") == "file");
  BOOST_CHECK(conf->get<std::string>("menu/popup/menuitem/one/onclick").get_value_or("") == "CreateNewDoc");
  BOOST_CHECK(conf->get<int>("menu/popup/menuitem/one/value").get_value_or(0) == 123);

  {
    std::ofstream stream(TEMP_FILE);
    stream << R"({"menu": {
      "id": "file",
      "popup": {
        "menuitem": [
          {"value": "123", "onclick": "CreateNewDoc"},
          {"value": "123", "onclick": "DeleteNewDoc"}
        ]
      }
    }})";
  }
  BOOST_CHECK_THROW(ConfigurationFactory::getConfiguration("json:/" + TEMP_FILE), std::runtime_error);
}
#endif

inline std::string getReferenceFileName()
{
  return "/tmp/aliceo2_configuration_recursive_test.json";
}

inline std::string getConfigurationUri()
{
  return "json:/" + getReferenceFileName();
}

inline std::string getReferenceJson()
{
  return R"({
  "equipment_1":
  {
    "enabled": true,
    "type"   : "rorc",
    "serial" : 33333,
    "channel": 0
  },
  "equipment_2":
  {
    "enabled": true,
    "type"   : "dummy",
    "serial" : -1,
    "channel": 0
  }
})";
}

inline std::unordered_map<std::string, std::string> getReferenceMap()
{
  return {
    {"/equipment_1/enabled", "1"},
    {"/equipment_1/type", "rorc"},
    {"/equipment_1/serial", "33333"},
    {"/equipment_1/channel", "0"},
    {"/equipment_2/enabled", "1"},
    {"/equipment_2/type", "dummy"},
    {"/equipment_2/serial", "-1"},
    {"/equipment_2/channel", "0"},
  };
}

inline tree::Node getEquipment1()
{
  return tree::Branch {
    {"enabled", true},
    {"type", "rorc"s},
    {"serial", 33333},
    {"channel", 0}};
}

inline tree::Node getEquipment2()
{
  return tree::Branch {
    {"enabled", true},
    {"type", "dummy"s},
    {"serial", -1},
    {"channel", 0}};
}

inline tree::Node getBadEquipment2()
{
  return tree::Branch {
    {"enabled", false},
    {"type", "dummy"s},
    {"serial", -1},
    {"channel", 0}};
}

// Data structure equivalent to the reference JSON above
// This is what should be generated by the backend
inline tree::Node getReferenceTree()
{
  return tree::Branch {
    {"equipment_1", getEquipment1()},
    {"equipment_2", getEquipment2()}};
}

// Data structure NOT equivalent to the reference JSON, it contains an error
inline tree::Node getBadReferenceTree()
{
  return tree::Branch {
    {"equipment_1", getEquipment1()},
    {"equipment_2", getBadEquipment2()}};
}

void writeReferenceFile()
{
  std::ofstream stream(getReferenceFileName());
  stream << getReferenceJson();
}

BOOST_AUTO_TEST_CASE(RecursiveTest)
{
  writeReferenceFile();

  std::unique_ptr<ConfigurationInterface> conf;
  try {
    conf = ConfigurationFactory::getConfiguration(getConfigurationUri());
  }
  catch (const std::exception& e) {
    BOOST_WARN_MESSAGE(false,
        std::string("Exception thrown, you may be missing the required infrastructure: ") + e.what());
    return;
  }

  tree::Node tree = conf->getRecursive("/");
  BOOST_CHECK(tree == getReferenceTree());
  BOOST_CHECK(tree != getBadReferenceTree());
}

BOOST_AUTO_TEST_CASE(RecursiveTest2)
{
  writeReferenceFile();

  std::unique_ptr<ConfigurationInterface> conf;
  try {
    conf = ConfigurationFactory::getConfiguration(getConfigurationUri());
  }
  catch (const std::exception& e) {
    BOOST_WARN_MESSAGE(false,
        std::string("Exception thrown, you may be missing the required infrastructure: ") + e.what());
    return;
  }

  BOOST_CHECK(getReferenceTree() == conf->getRecursive("/"));
  BOOST_CHECK(getEquipment1() == conf->getRecursive("/equipment_1"));
  BOOST_CHECK(getEquipment2() == conf->getRecursive("/equipment_2"));
  BOOST_CHECK(getBadEquipment2() != conf->getRecursive("/equipment_2"));
  BOOST_CHECK(tree::get<int>(getEquipment1(), "channel") == tree::get<int>(conf->getRecursive("/equipment_1/channel")));

  conf->setPrefix("/equipment_1");
  BOOST_CHECK(getEquipment1() == conf->getRecursive("/"));

  conf->setPrefix("/equipment_2");
  BOOST_CHECK(getEquipment2() == conf->getRecursive("/"));

  conf->setPrefix("/");
  BOOST_CHECK(getReferenceTree() == conf->getRecursive("/"));
}

BOOST_AUTO_TEST_CASE(RecursiveTest3)
{
  writeReferenceFile();

  std::unique_ptr<ConfigurationInterface> conf;
  try {
    conf = ConfigurationFactory::getConfiguration(getConfigurationUri());
  }
  catch (const std::exception& e) {
    BOOST_WARN_MESSAGE(false,
        std::string("Exception thrown, you may be missing the required infrastructure: ") + e.what());
    return;
  }

  tree::Node tree = conf->getRecursive("/");
  std::ofstream stream;

  for (const auto& keyValue : tree::getBranch(tree)) {
    const auto& equipment = tree::getBranch(keyValue.second);
    stream << "Equipment '" << keyValue.first << "' \n"
      << "serial  " << tree::getRequired<int>(equipment, "serial") << '\n'
      << "channel " << tree::getRequired<int>(equipment, "channel") << '\n'
      << "enabled " << tree::getRequired<bool>(equipment, "enabled") << '\n'
      << "type    " << tree::getRequired<std::string>(equipment, "type") << '\n';

    BOOST_CHECK(!tree::get<int>(equipment, "nope").is_initialized());
  }
}

BOOST_AUTO_TEST_CASE(RecursiveMapTest)
{
  writeReferenceFile();

  std::unique_ptr<ConfigurationInterface> conf;
  try {
    conf = ConfigurationFactory::getConfiguration(getConfigurationUri());
  }
  catch (const std::exception& e) {
    BOOST_WARN_MESSAGE(false,
      std::string("Exception thrown, you may be missing the required infrastructure: ") + e.what());
    return;
  }

  ConfigurationInterface::KeyValueMap map = conf->getRecursiveMap("/");
  BOOST_CHECK(map == getReferenceMap());
}

BOOST_AUTO_TEST_CASE(EtcdTest)
{
  // Get file configuration interface from factory
  std::unique_ptr<ConfigurationInterface> conf;
  try {
    conf = ConfigurationFactory::getConfiguration("etcd-v3://localhost:2379");
  }
  catch (const std::exception& e) {
    BOOST_WARN_MESSAGE(false,
        std::string("Exception thrown, you may be missing the required infrastructure: ") + e.what());
    return;
  }

  std::string key {"/test/key"};
  std::string value {"test_value"};

  conf->putString("/test/key1", "1");
  conf->putString("/test/key2", "2");
  conf->putString("/test/key3", "3");
  conf->putString("/test/dir/key4", "4");
  conf->putString("/test/dir/key5", "5");

  conf->put(key, value);
  auto returnedValue = conf->get<std::string>(key);

  {
    auto tree = conf->getRecursive("/");
    BOOST_CHECK(tree::getRequired<int>(tree::getSubtree(tree, "/test/key1")) == 1);
    BOOST_CHECK(tree::getRequired<int>(tree::getSubtree(tree, "/test/key2")) == 2);
    BOOST_CHECK(tree::getRequired<int>(tree::getSubtree(tree, "/test/dir/key4")) == 4);
  }

  {
    auto tree = conf->getRecursive("/test");
    BOOST_CHECK(tree::getRequired<int>(tree::getSubtree(tree, "/key1")) == 1);
    BOOST_CHECK(tree::getRequired<int>(tree::getSubtree(tree, "/key2")) == 2);
    BOOST_CHECK(tree::getRequired<int>(tree::getSubtree(tree, "/dir/key4")) == 4);
  }
}

BOOST_AUTO_TEST_CASE(TreeConversionTest)
{
  using namespace tree;

  //! [Key-value pair conversion]
  std::vector<std::pair<std::string, Leaf>> pairs {
      {"/dir/bool", false},
         {"/dir/double", 45.6},
         {"/dir/subdir/int", 123},
         {"/dir/subdir/subsubdir/string", "string"s}};

  Node convertedTree = keyValuesToTree(pairs);
  BOOST_CHECK(treeToKeyValues(convertedTree) == pairs);
}
} // Anonymous namespace
