// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/ArcConfig.h>
#include <arc/Logger.h>
#include <arc/client/TargetRetriever.h>
#include <arc/loader/FinderLoader.h>

namespace Arc {

  Logger TargetRetriever::logger(Logger::getRootLogger(), "TargetRetriever");

  TargetRetriever::TargetRetriever(const UserConfig& usercfg,
                                   const URL& url, ServiceType st,
                                   const std::string& flavour)
    : flavour(flavour),
      usercfg(usercfg),
      url(url),
      serviceType(st) {}

  TargetRetriever::~TargetRetriever() {}

  TargetRetrieverLoader::TargetRetrieverLoader()
    : Loader(BaseConfig().MakeConfig(Config()).Parent()) {}

  TargetRetrieverLoader::~TargetRetrieverLoader() {
    for (std::list<TargetRetriever*>::iterator it = targetretrievers.begin();
         it != targetretrievers.end(); it++)
      delete *it;
  }

  TargetRetriever* TargetRetrieverLoader::load(const std::string& name,
                                               const UserConfig& usercfg,
                                               const std::string& service,
                                               const ServiceType& st) {
    if (name.empty())
      return NULL;

    if(!factory_->load(FinderLoader::GetLibrariesList(),
                       "HED:TargetRetriever", name)) {
      logger.msg(ERROR, "TargetRetriever plugin \"%s\" not found. Please refer to installation instructions and check if package providing support for %s plugin is installed", name, name);
      return NULL;
    }

    TargetRetrieverPluginArgument arg(usercfg, service, st);
    TargetRetriever *targetretriever =
      factory_->GetInstance<TargetRetriever>("HED:TargetRetriever", name, &arg, false);

    if (!targetretriever) {
      logger.msg(ERROR, "TargetRetriever %s could not be created. Please refer to installation instructions and check if package providing support for %s plugin is installed", name, name);
      return NULL;
    }

    targetretrievers.push_back(targetretriever);
    logger.msg(INFO, "Loaded TargetRetriever %s", name);
    return targetretriever;
  }

} // namespace Arc
