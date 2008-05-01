#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MessageAuth.h"

namespace Arc {

MessageAuth::MessageAuth(void):attrs_created_(true) { }

MessageAuth::~MessageAuth(void) {
  if(!attrs_created_) return;
  std::map<std::string,SecAttr*>::iterator attr = attrs_.begin();
  for(;attr != attrs_.end();++attr) {
    if(attr->second) delete attr->second;
  };
}

void MessageAuth::set(const std::string& key, SecAttr* value) {
  if(!attrs_created_) return;
  std::map<std::string,SecAttr*>::iterator attr = attrs_.find(key);
  if(attr == attrs_.end()) {
    attrs_[key]=value;
  } else {
    if(attr->second) delete attr->second;
    attr->second=value;
  };
}

void MessageAuth::remove(const std::string& key) {
  std::map<std::string,SecAttr*>::iterator attr = attrs_.find(key);
  if(attr != attrs_.end()) {
    if(attrs_created_) if(attr->second) delete attr->second;
    attrs_.erase(attr);
  };
}

SecAttr* MessageAuth::get(const std::string& key) {
  std::map<std::string,SecAttr*>::iterator attr = attrs_.find(key);
  if(attr == attrs_.end()) return NULL;
  return attr->second;
}

static void add_subject(XMLNode request,XMLNode subject) {
  XMLNode item = request["RequestItem"];
  for(;(bool)item;++item) item.NewChild(subject);
}

static void add_new_request(XMLNode request,XMLNode resource,XMLNode action) {
  if((!resource) && (!action)) return;
  XMLNode newitem = request.NewChild("ra:RequestItem");
  for(;(bool)resource;++resource) newitem.NewChild(resource);
  for(;(bool)action;++action) newitem.NewChild(action);
}

// All Subject elements go to all new request items.
// Every Resource, Action or Resource+Action set makes own request item.
bool MessageAuth::Export(SecAttr::Format format,XMLNode &val) const {
  // Currently only ARCAuth is supported
  if(format != SecAttr::ARCAuth) return false;
  NS ns;
  ns["ra"]="http://www.nordugrid.org/schemas/request-arc";
  XMLNode newreq = val;
  newreq.Namespaces(ns);
  newreq.Name("ra:Request");
  std::map<std::string,SecAttr*>::const_iterator attr = attrs_.begin();
  for(;attr != attrs_.end();++attr) {
    XMLNode r(ns,"");
    if(!(attr->second)) return false;
    if(!(attr->second->Export(format,r))) return false;
    XMLNode item;
    for(item=r["RequestItem"];(bool)item;++item) {
      XMLNode resource = item["Resource"];
      XMLNode action = item["Action"];
      add_new_request(newreq,resource,action);
    };
    if(!newreq["RequestItem"]) newreq.NewChild("ra:RequestItem");
    for(item=r["RequestItem"];(bool)item;++item) {
      XMLNode subject = item["Subject"];
      for(;(bool)subject;++subject) add_subject(newreq,subject);
    };
  };
}

MessageAuth* MessageAuth::Filter(const std::list<std::string> selected_keys,const std::list<std::string> rejected_keys) const {
  MessageAuth* newauth = new MessageAuth;
  newauth->attrs_created_=false;
  if(selected_keys.empty()) {
    newauth->attrs_=attrs_;
  } else {
    for(std::list<std::string>::const_iterator key = selected_keys.begin();
                       key!=selected_keys.end();++key) {
      std::map<std::string,SecAttr*>::const_iterator attr = attrs_.find(*key);
      if((attr != attrs_.end()) && (attr->second != NULL)) newauth->attrs_[*key]=attr->second;
    };
  };
  if(!rejected_keys.empty()) {
    for(std::list<std::string>::const_iterator key = rejected_keys.begin();
                       key!=rejected_keys.end();++key) {
      newauth->remove(*key);
    };
  };
  return newauth;
}

}

