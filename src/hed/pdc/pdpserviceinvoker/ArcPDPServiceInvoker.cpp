#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdexcept>

#include <arc/XMLNode.h>
#include <arc/Thread.h>
#include <arc/ArcConfig.h>
#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/message/MCC.h>
//#include <arc/security/ArcPDP/Response.h>
//#include <arc/security/ArcPDP/attr/AttributeValue.h>

#include "ArcPDPServiceInvoker.h"

Arc::Logger ArcSec::ArcPDPServiceInvoker::logger(ArcSec::PDP::logger,"ArcPDPServiceInvoker");

using namespace Arc;

namespace ArcSec {
Plugin* ArcPDPServiceInvoker::get_pdpservice_invoker(PluginArgument* arg) {
    PDPPluginArgument* pdparg =
            arg?dynamic_cast<PDPPluginArgument*>(arg):NULL;
    if(!pdparg) return NULL;
    return new ArcPDPServiceInvoker((Config*)(*pdparg));
}

ArcPDPServiceInvoker::ArcPDPServiceInvoker(Config* cfg):PDP(cfg), client(NULL) {
  XMLNode filter = (*cfg)["Filter"];
  if((bool)filter) {
    XMLNode select_attr = filter["Select"];
    XMLNode reject_attr = filter["Reject"];
    for(;(bool)select_attr;++select_attr) select_attrs.push_back((std::string)select_attr);
    for(;(bool)reject_attr;++reject_attr) reject_attrs.push_back((std::string)reject_attr);
  };

  //Create a SOAP client
  logger.msg(Arc::INFO, "Creating a pdpservice client");

  std::string url_str;
  url_str = (std::string)((*cfg)["ServiceEndpoint"]);
  Arc::URL url(url_str);
  
  std::cout<<"URL: "<<url_str<<std::endl;

  Arc::MCCConfig mcc_cfg;
  std::cout<<"Keypath: "<<(std::string)((*cfg)["KeyPath"])<<"CertificatePath: "<<(std::string)((*cfg)["CertificatePath"])<<"CAPath: "<<(std::string)((*cfg)["CACertificatePath"])<<std::endl;
  mcc_cfg.AddPrivateKey((std::string)((*cfg)["KeyPath"]));
  mcc_cfg.AddCertificate((std::string)((*cfg)["CertificatePath"]));
  mcc_cfg.AddProxy((std::string)((*cfg)["ProxyPath"]));
  mcc_cfg.AddCAFile((std::string)((*cfg)["CACertificatePath"]));
  mcc_cfg.AddCADir((std::string)((*cfg)["CACertificatesDir"]));

  client = new Arc::ClientSOAP(mcc_cfg,url);
}

bool ArcPDPServiceInvoker::isPermitted(Message *msg){
  //Compose the request
  MessageAuth* mauth = msg->Auth()->Filter(select_attrs,reject_attrs);
  MessageAuth* cauth = msg->AuthContext()->Filter(select_attrs,reject_attrs);
  if((!mauth) && (!cauth)) {
    logger.msg(ERROR,"Missing security object in message");
    return false;
  };
  NS ns;
  XMLNode requestxml(ns,"");
  if(mauth) {
    if(!mauth->Export(SecAttr::ARCAuth,requestxml)) {
      delete mauth;
      logger.msg(ERROR,"Failed to convert security information to ARC request");
      return false;
    };
    delete mauth;
  };
  if(cauth) {
    if(!cauth->Export(SecAttr::ARCAuth,requestxml)) {
      delete mauth;
      logger.msg(ERROR,"Failed to convert security information to ARC request");
      return false;
    };
    delete cauth;
  };
  {
    std::string s;
    requestxml.GetXML(s);
    logger.msg(VERBOSE,"ARC Auth. request: %s",s);
  };
  if(requestxml.Size() <= 0) {
    logger.msg(ERROR,"No requested security information was collected");
    return false;
  };

  //Invoke the remote pdp service

  Arc::NS req_ns;
  req_ns["ra"] = "http://www.nordugrid.org/schemas/request-arc";
  req_ns["pdp"] = "http://www.nordugrid.org/schemas/pdp";
  Arc::PayloadSOAP req(req_ns);
  Arc::XMLNode reqnode = req.NewChild("pdp:GetPolicyDecisionRequest");
  reqnode.NewChild(requestxml);

  Arc::PayloadSOAP* resp = NULL;
  if(client) {
    Arc::MCC_Status status = client->process(&req,&resp);
    if(!status) {
      logger.msg(Arc::ERROR, "Policy Decision Service invocation failed");
    }
    if(resp == NULL) {
      logger.msg(Arc::ERROR,"There was no SOAP response");
    }
  }

  std::string authz_res;
  if(resp) {
    std::string str;
    resp->GetXML(str);
    logger.msg(Arc::INFO, "Response: %s", str);

    // TODO: Fix namespaces
    authz_res=(std::string)((*resp)["pdp:GetPolicyDecisionResponse"]["response:Response"]["response:AuthZResult"]);

    delete resp;
  } 

  if(authz_res == "PERMIT") { logger.msg(Arc::INFO,"Authorized from remote pdp service"); return true; }
  else { logger.msg(Arc::INFO,"Unauthorized from remote pdp service"); return false; }

}

ArcPDPServiceInvoker::~ArcPDPServiceInvoker(){
}

} // namespace ArcSec

