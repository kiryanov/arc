// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/data/DMCLoader.h>

#include "DataPointSRM.h"
#include "DMCSRM.h"

namespace Arc {

  Logger DMCSRM::logger(DMC::logger, "SRM");

  DMCSRM::DMCSRM(Config *cfg)
    : DMC(cfg) {
    Register(this);
  }

  DMCSRM::~DMCSRM() {
    Unregister(this);
  }

  Plugin* DMCSRM::Instance(PluginArgument *arg) {
    DMCPluginArgument *dmcarg =
      arg ? dynamic_cast<DMCPluginArgument*>(arg) : NULL;
    if (!dmcarg)
      return NULL;
    // Make this code non-unloadable because Globus
    // may have problems with unloading
    Glib::Module* module = dmcarg->get_module();
    PluginsFactory* factory = dmcarg->get_factory();
    if(factory && module) factory->makePersistent(module);
    return new DMCSRM((Config*)(*dmcarg));
  }

  DataPoint* DMCSRM::iGetDataPoint(const URL& url) {
    if (url.Protocol() != "srm")
      return NULL;
    return new DataPointSRM(url);
  }

} // namespace Arc

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
  { "srm", "HED:DMC", 0, &Arc::DMCSRM::Instance },
  { NULL, NULL, 0, NULL }
};
