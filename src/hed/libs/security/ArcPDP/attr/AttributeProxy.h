#ifndef __ARC_ATTRIBUTEPROXY_H__
#define __ARC_ATTRIBUTEPROXY_H__

#include <list>
#include <fstream>
#include <arc/XMLNode.h>
#include <arc/Logger.h>
#include "AttributeValue.h"
//#include "StringAttribute.h"

namespace Arc {

class AttributeProxy {
public:
  AttributeProxy() {};
  virtual ~AttributeProxy(){};
public:
  virtual AttributeValue* getAttribute(const XMLNode& node) = 0;
};

template <class TheAttribute>
class ArcAttributeProxy : public AttributeProxy {
public:
  ArcAttributeProxy(){};
  virtual ~ArcAttributeProxy(){};
public: 
  virtual AttributeValue* getAttribute(const XMLNode& node);
};

template <class TheAttribute>
AttributeValue* ArcAttributeProxy<TheAttribute>::getAttribute(const XMLNode& node){
  std::string value = (std::string)node;
  
 // std::cout<<value<<std::endl;  //for testing
  
  return new TheAttribute(value);
  //return new TheAttribute();
}

} // namespace Arc

#endif /* __ARC_ATTRIBUTEPROXY_H__ */

