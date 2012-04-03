// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ComputingServiceRetriever.h"

namespace Arc {

  ComputingServiceRetriever::ComputingServiceRetriever(
    const UserConfig& uc,
    const std::list<Endpoint>& services,
    const std::list<std::string>& rejectedServices,
    const std::list<std::string>& preferredInterfaceNames,
    const std::list<std::string>& capabilityFilter
  ) : ser(uc, EndpointQueryOptions<Endpoint>(true, capabilityFilter, rejectedServices)),
      tir(uc, EndpointQueryOptions<ComputingServiceType>(preferredInterfaceNames))
  {
    ser.addConsumer(*this);
    tir.addConsumer(*this);
    for (std::list<Endpoint>::const_iterator it = services.begin(); it != services.end(); it++) {
      addEndpoint(*it);
    }
  }

  void ComputingServiceRetriever::addEndpoint(const Endpoint& service) {
    // If we got a computing element info endpoint, then we pass it to the TIR
    if (service.HasCapability(Endpoint::COMPUTINGINFO)) {
      tir.addEndpoint(service);
    } else if (service.HasCapability(Endpoint::REGISTRY)) {
      ser.addEndpoint(service);
    }
  }

} // namespace Arc