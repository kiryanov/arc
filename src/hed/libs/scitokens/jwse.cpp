#include <cctype>
#include <cstdlib>
#include <cstring>
#include <openssl/evp.h>

#include <arc/Base64.h>
#include <arc/external/cJSON/cJSON.h>

#include "jwse.h"


namespace Arc {
  JWSE::JWSE(): valid_(false), header_(NULL), publicKey_(NULL)  {
    header_ = cJSON_CreateObject();
    if(header_ == NULL)
      return;
    cJSON* algStr = cJSON_CreateString("none");
    if(algStr == NULL) {
      Cleanup();
      return;
    }
    cJSON_AddItemToObject(header_, "alg", algStr);
    valid_ = true;
  }

  JWSE::JWSE(std::string const& jwseCompact): valid_(false), header_(NULL), publicKey_(NULL)  {
    (void)Input(jwseCompact);
  }

  bool JWSE::Input(std::string const& jwseCompact) {
    Cleanup();

    char const* pos = jwseCompact.c_str();
    while(std::isspace(*pos) != 0) {
      if(*pos == '\0') return false;
      ++pos;
    }
    // Identify header
    char const* joseStart = pos;
    while(*pos != '.') {
      if(*pos == '\0') return false;
      ++pos;
    }
    char const* joseEnd = pos;
    ++pos;
    // Decode header so we know if we have JWS or JWE
    std::string joseStr = Base64::decodeURLSafe(joseStart, joseEnd-joseStart);
    header_ = cJSON_Parse(joseStr.c_str());
    if(header_ == NULL) return false;
    cJSON* algObject = cJSON_GetObjectItem(header_, "alg");
    if(algObject == NULL) return false; // Neither JWS nor JWE
    if(algObject->type != cJSON_String) return false;
    if(algObject->string == NULL) return false;
    cJSON* encObject = cJSON_GetObjectItem(header_, "enc");
    if (encObject == NULL) {
      // JWS
      char const* payloadStart = pos;
      while(*pos != '.') {
        if(*pos == '\0') return false;
        ++pos;
      }
      char const* payloadEnd = pos;
      ++pos;
      content_ = Base64::decodeURLSafe(payloadStart, payloadEnd-payloadStart);
      char const* signatureStart = pos;
      char const* signatureEnd = jwseCompact.c_str() + jwseCompact.length();
      std::string signature = Base64::decodeURLSafe(payloadStart, payloadEnd-payloadStart);
      bool verifyResult = false;
      if(strcmp(algObject->string, "none") == 0) {
        verifyResult = signature.empty(); // expecting empty signature if no protection is requested
      } else if(strcmp(algObject->string, "HS256") == 0) {
        verifyResult = VerifyHMAC("SHA256", joseStart, payloadEnd-joseStart,
                         reinterpret_cast<unsigned char const*>(signature.c_str()), signature.length());
      } else if(strcmp(algObject->string, "HS384") == 0) {
        verifyResult = VerifyHMAC("SHA384", joseStart, payloadEnd-joseStart,
                         reinterpret_cast<unsigned char const*>(signature.c_str()), signature.length());
      } else if(strcmp(algObject->string, "HS512") == 0) {
        verifyResult = VerifyHMAC("SHA512", joseStart, payloadEnd-joseStart,
                         reinterpret_cast<unsigned char const*>(signature.c_str()), signature.length());
      } else if(strcmp(algObject->string, "ES256") == 0) {
        verifyResult = VerifyECDSA("SHA256", joseStart, payloadEnd-joseStart,
                         reinterpret_cast<unsigned char const*>(signature.c_str()), signature.length());
      } else if(strcmp(algObject->string, "ES384") == 0) {
        verifyResult = VerifyECDSA("SHA384", joseStart, payloadEnd-joseStart,
                         reinterpret_cast<unsigned char const*>(signature.c_str()), signature.length());
      } else if(strcmp(algObject->string, "ES512") == 0) {
        verifyResult = VerifyECDSA("SHA512", joseStart, payloadEnd-joseStart,
                         reinterpret_cast<unsigned char const*>(signature.c_str()), signature.length());
      }
/*
   | RS256        | RSASSA-PKCS1-v1_5 using       | Recommended        |
   |              | SHA-256                       |                    |
   | RS384        | RSASSA-PKCS1-v1_5 using       | Optional           |
   |              | SHA-384                       |                    |
   | RS512        | RSASSA-PKCS1-v1_5 using       | Optional           |
   |              | SHA-512                       |                    |
   | PS256        | RSASSA-PSS using SHA-256 and  | Optional           |
   |              | MGF1 with SHA-256             |                    |
   | PS384        | RSASSA-PSS using SHA-384 and  | Optional           |
   |              | MGF1 with SHA-384             |                    |
   | PS512        | RSASSA-PSS using SHA-512 and  | Optional           |
   |              | MGF1 with SHA-512             |                    |
*/

      if(!verifyResult) return false;
    } else {
      // JWE - not yet
      cJSON_Delete(header_);
      header_ = NULL;
      return false;
    }      
    valid_ = true;
    return true;
  }

  bool JWSE::Output(std::string& jwseCompact) const {
    jwseCompact.clear();
    if(!valid_)
      return false;

    char* joseStr = cJSON_PrintUnformatted(header_);
    if(joseStr == NULL)
      return false;
    jwseCompact += Base64::encodeURLSafe(joseStr);
    std::free(joseStr); 
    jwseCompact += '.';
    jwseCompact += Base64::encodeURLSafe(content_.c_str());
    jwseCompact += '.';
    // No signature
    return false;
  }

  void JWSE::Cleanup() {
    if(header_ != NULL)
      cJSON_Delete(header_);
    header_ = NULL;
    if(publicKey_ != NULL)
      EVP_PKEY_free(publicKey_);
    publicKey_ = NULL;
  }

  JWSE::~JWSE() {
    Cleanup();
  }

  char const* JWSE::Content() const {
    return content_.c_str();
  }

  cJSON const* JWSE::HeaderParameter(char const* name) const {
    if(header_ == NULL)
      return NULL;
    cJSON const* param = cJSON_GetObjectItem(const_cast<cJSON*>(header_), name);
    if(param == NULL)
      return NULL;
    return param;
  }


/*



   private:
    cJSON* header_;

     std::string content_;
*/


} // namespace Arc
