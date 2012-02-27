// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_JOBLISTRETRIEVERPLUGINWSRFGLUE2_H__
#define __ARC_JOBLISTRETRIEVERPLUGINWSRFGLUE2_H__

#include <arc/client/Job.h>
#include <arc/client/EndpointRetriever.h>
#include <arc/client/TargetInformationRetriever.h>

namespace Arc {

  class Logger;

  class JobListRetrieverPluginWSRFGLUE2 : public EndpointRetrieverPlugin<ComputingInfoEndpoint, Job> {
  public:
    JobListRetrieverPluginWSRFGLUE2() {}
    ~JobListRetrieverPluginWSRFGLUE2() {}

    static Plugin* Instance(PluginArgument *arg) { return new JobListRetrieverPluginWSRFGLUE2(); }
    EndpointQueryingStatus Query(const UserConfig&, const ComputingInfoEndpoint&, std::list<Job>&, const EndpointFilter<Job>&) const;

  private:
    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_JOBLISTRETRIEVERPLUGINWSRFGLUE2_H__