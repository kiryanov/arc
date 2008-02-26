#ifndef __ARC_AREX_H__
#define __ARC_AREX_H__

#include <arc/message/Service.h>
#include <arc/message/PayloadRaw.h>
#include <arc/delegation/DelegationInterface.h>
#include <arc/infosys/InformationInterface.h>

#include "grid-manager/grid_manager.h"

namespace ARex {

class ARexGMConfig;
class ARexConfigContext;

class ARexService: public Arc::Service {
 protected:
  Arc::NS ns_;
  Arc::Logger logger_;
  Arc::DelegationContainerSOAP delegations_;
  Arc::InformationContainer infodoc_;
  std::string endpoint_;
  std::string uname_;
  std::string gmconfig_;
  GridManager* gm_;
  ARexConfigContext* get_configuration(Arc::Message& inmsg);
  Arc::MCC_Status CreateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid);
  Arc::MCC_Status GetActivityStatuses(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status TerminateActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status GetActivityDocuments(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status GetFactoryAttributesDocument(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status StopAcceptingNewActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status StartAcceptingNewActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status ChangeActivityStatus(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status make_response(Arc::Message& outmsg);
  Arc::MCC_Status make_fault(Arc::Message& outmsg);
  Arc::MCC_Status make_soap_fault(Arc::Message& outmsg);
  Arc::MCC_Status Get(Arc::Message& outmsg,ARexGMConfig& config,const std::string& id,const std::string& subpath);
  Arc::MCC_Status Put(ARexGMConfig& config,const std::string& id,const std::string& subpath,Arc::PayloadRawInterface& buf);
 public:
  ARexService(Arc::Config *cfg);
  virtual ~ARexService(void);
  virtual Arc::MCC_Status process(Arc::Message& inmsg,Arc::Message& outmsg);
  void InformationCollector(void);
};

} // namespace ARex

#endif

