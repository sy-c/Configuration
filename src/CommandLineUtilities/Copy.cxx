/// \file Copy.cxx
/// \brief Command-line utility for copying values from a configuration backend
///
/// \author Pascal Boeschoten (pascal.boeschoten@cern.ch)

#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include "Common/Program.h"
#include "Configuration/ConfigurationFactory.h"
#include "Configuration/Tree.h"

namespace po = boost::program_options;
namespace
{
class Copy : public AliceO2::Common::Program
{
    virtual Description getDescription() override
    {
      return {"configuration-copy", "Copies values from source to destination",
        "configuration-copy --source=etcd-v3://host1:2379/stuff/ --dest=consul://host2:8500/some/dirs/new/stuff/"};
    }

    virtual void addOptions(boost::program_options::options_description& optionsDescription) override
    {
      optionsDescription.add_options()
          ("source,s", po::value<std::string>(&mSourceUri)->required(), "Source server URI")
          ("dest,d", po::value<std::string>(&mDestinationUri)->required(), "Destination server URI");
    }

    virtual void run(const boost::program_options::variables_map& variablesMap) override
    {
      using namespace AliceO2::Configuration;
      auto source = ConfigurationFactory::getConfiguration(mSourceUri);
      auto destination = ConfigurationFactory::getConfiguration(mDestinationUri);
      auto keyValues = Tree::treeToKeyValues(source->getRecursive("/"));

      for (const auto& kv : keyValues) {
        Visitor::apply(kv.second,
            [&](const std::string& value) { destination->put<std::string>(kv.first, value); },
            [&](int value) { destination->put<int>(kv.first, value); },
            [&](bool value) { destination->put<int>(kv.first, int(value)); },
            [&](double value) { destination->put<double>(kv.first, value); });
      }
    }

    std::string mSourceUri;
    std::string mDestinationUri;
};
} // Anonymous namespace

int main(int argc, char** argv)
{
  return Copy().execute(argc, argv);
}

