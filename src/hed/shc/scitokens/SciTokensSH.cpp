#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <fstream>

#include <arc/StringConv.h>
#include <arc/scitokens/jwse.h>
#include <arc/message/SecAttr.h>
#include <arc/external/cJSON/cJSON.h>

#include "SciTokensSH.h"

static Arc::Logger logger(Arc::Logger::rootLogger, "SciTokensSH");

Arc::Plugin* ArcSec::SciTokensSH::get_sechandler(Arc::PluginArgument* arg) {
  ArcSec::SecHandlerPluginArgument* shcarg =
          arg?dynamic_cast<ArcSec::SecHandlerPluginArgument*>(arg):NULL;
  if(!shcarg) return NULL;
  ArcSec::SciTokensSH* plugin = new ArcSec::SciTokensSH((Arc::Config*)(*shcarg),(Arc::ChainContext*)(*shcarg),arg);
  if(!plugin) return NULL;
  if(!(*plugin)) { delete plugin; plugin = NULL; };
  return plugin;
}

extern Arc::PluginDescriptor const ARC_PLUGINS_TABLE_NAME[] = {
    { "scitokens.handler", "HED:SHC", NULL, 0, &ArcSec::SciTokensSH::get_sechandler},
    { NULL, NULL, NULL, 0, NULL }
};

namespace ArcSec {
using namespace Arc;


class SciTokensSecAttr: public SecAttr {
 public:
  SciTokensSecAttr(Arc::Message* msg);
  virtual ~SciTokensSecAttr(void);
  virtual operator bool(void) const;
  virtual std::string get(const std::string& id) const;
 protected:
  std::string subject_;  // sub
  std::string issuer_;   // iss 
  std::string audience_; // aud
  bool valid_;
};

int strnicmp(char const* left, char const* right, size_t len) {
  while(len > 0) {
    if(std::tolower(*left) != std::tolower(*right)) return *left - *right;
    if(*left == '\0') return 0;
    --len;
    ++left;
    ++right;
  };
  return 0;
}

SciTokensSecAttr::SciTokensSecAttr(Arc::Message* msg):valid_(false) {
  static const char tokenid[] = "bearer ";
  if(msg) {
    MessageAttributes* attrs = msg->Attributes();
    if(attrs) {
      std::string token = attrs->get("HTTP:authorization");
      if(strnicmp(token.c_str(), tokenid, sizeof(tokenid)-1) == 0) {
        token.erase(0, sizeof(tokenid)-1);
        Arc::JWSE jwse(token);
        if(jwse) {
          cJSON const* obj(NULL);
          obj = jwse.Claim(Arc::JWSE::ClaimNameSubject);
          if(obj && (obj->type == cJSON_String) && (obj->valuestring)) subject_ = obj->valuestring;
          obj = jwse.Claim(Arc::JWSE::ClaimNameIssuer);
          if(obj && (obj->type == cJSON_String) && (obj->valuestring)) issuer_ = obj->valuestring;
          obj = jwse.Claim(Arc::JWSE::ClaimNameAudience);
          if(obj && (obj->type == cJSON_String) && (obj->valuestring)) audience_ = obj->valuestring;
          valid_ = true;
        };
      };
    };
  };
}

SciTokensSecAttr::~SciTokensSecAttr() {
}

std::string SciTokensSecAttr::get(const std::string& id) const {
  if(id == "SUBJECT") return subject_;
  if(id == "ISSUER") return issuer_;
  if(id == "AUDIENCE") return audience_;
  return "";
}

SciTokensSecAttr::operator bool() const {
  return valid_;
}

SciTokensSH::SciTokensSH(Config *cfg,ChainContext*,Arc::PluginArgument* parg):SecHandler(cfg,parg),valid_(false){
  valid_ = true;
}

SciTokensSH::~SciTokensSH(){
}

SecHandlerStatus SciTokensSH::Handle(Arc::Message* msg) const {
  if(msg) {
    SciTokensSecAttr* sattr = new SciTokensSecAttr(msg);
    if(!*sattr) {
      //logger.msg(ERROR, "Failed to SciTokens security attributes");
      delete sattr;
      sattr = NULL;
    } else {
      msg->Auth()->set("SCITOKENS",sattr);
    };
  };
  return true;
}


} // namespace ArcSec

