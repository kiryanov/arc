// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>

#include <arc/client/JobDescription.h>
#include <arc/message/MCC.h>
#include <arc/ws-addressing/WSA.h>

#include "SubmitterUNICORE.h"
#include "UNICOREClient.h"

namespace Arc {

  Logger SubmitterUNICORE::logger(Submitter::logger, "UNICORE");

  SubmitterUNICORE::SubmitterUNICORE(Config *cfg)
    : Submitter(cfg, "UNICORE") {}

  SubmitterUNICORE::~SubmitterUNICORE() {}

  Plugin* SubmitterUNICORE::Instance(PluginArgument *arg) {
    ACCPluginArgument *accarg =
      arg ? dynamic_cast<ACCPluginArgument*>(arg) : NULL;
    if (!accarg)
      return NULL;
    return new SubmitterUNICORE((Config*)(*accarg));
  }

  bool SubmitterUNICORE::Submit(const JobDescription& jobdesc, XMLNode& info) const {
    MCCConfig cfg;
    ApplySecurity(cfg);

    logger.msg(INFO, "Creating client chain for UNICORE BES service");
    ClientSOAP client(cfg, submissionEndpoint);
    //if((!client) || (!(*client))) {
    //   logger.msg(ERROR,"Failed to create client for SOAP exchange");
    //   return false;
    //}

    // Prepare BES request
    logger.msg(INFO, "Creating and sending request");

    // Create job request
    /*
       bes-factory:CreateActivity
         bes-factory:ActivityDocument
           jsdl:JobDefinition
     */
    NS ns;
    ns["bes-factory"] = "http://schemas.ggf.org/bes/2006/08/bes-factory";
    ns["wsa"] = "http://www.w3.org/2005/08/addressing";
    ns["jsdl"] = "http://schemas.ggf.org/jsdl/2005/11/jsdl";
    PayloadSOAP req(ns);
    XMLNode op = req.NewChild("bes-factory:CreateActivity");
    XMLNode act_doc = op.NewChild("bes-factory:ActivityDocument");
    WSAHeader(req).Action("http://schemas.ggf.org/bes/2006/08/bes-factory/BESFactoryPortType/CreateActivity");
    WSAHeader(req).To(submissionEndpoint.str());
    std::string jsdl_str = jobdesc.UnParse("POSIXJSDL");
    act_doc.NewChild(XMLNode(jsdl_str));
    //act_doc.Child(0).Namespaces(arex_ns); // Unify namespaces
    PayloadSOAP *resp = NULL;
    act_doc.GetXML(jsdl_str);
    logger.msg(VERBOSE, "Job description to be sent: %s", jsdl_str);
    MCC_Status status =
      client.process("http://schemas.ggf.org/bes/2006/08/bes-factory/"
                     "BESFactoryPortType/CreateActivity", &req, &resp);
    if (!status) {
      logger.msg(ERROR, "Submission request failed");
      return false;
    }
    if (resp == NULL) {
      logger.msg(ERROR, "There was no SOAP response");
      return false;
    }

    SOAPFault fs(*resp);
    if (fs) {
      std::string faultstring = fs.Reason();
      std::string s;
      resp->GetXML(s);
      // delete resp;
      logger.msg(VERBOSE, "Submission returned failure: %s", s);
      logger.msg(ERROR, "Submission failed, service returned: %s", faultstring);
      return false;
    }
    XMLNode id;
    (*resp)["CreateActivityResponse"]["ActivityIdentifier"].New(id);
    std::string jobid;
    id.GetDoc(jobid);
    // delete resp;
    if (jobid.empty()) {
      logger.msg(ERROR, "Service returned no job identifier");
      return false;
    }
    info.NewChild("AuxInfo") = jobid;
    info.NewChild("JobID") = (std::string)id["Address"];
    info.NewChild("InfoEndpoint") = submissionEndpoint.str();

    return true;
  }

  bool SubmitterUNICORE::Migrate(const URL& jobid, const JobDescription& jobdesc, bool forcemigration, XMLNode& info) const {
    logger.msg(ERROR, "Migration to a UNICORE cluster is not supported.");
    return false;
  }
} // namespace Arc
