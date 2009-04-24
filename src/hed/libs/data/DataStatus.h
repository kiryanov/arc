// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_DATASTATUS_H__
#define __ARC_DATASTATUS_H__

#include <iostream>
#include <string>

namespace Arc {

  class DataStatus {

  public:

    enum DataStatusType {

      /// Operation completed successfully
      Success = 0,

      /// Source is bad URL or can't be used due to some reason
      ReadAcquireError = 1,

      /// Destination is bad URL or can't be used due to some reason
      WriteAcquireError = 2,

      /// Resolving of index service URL for source failed
      ReadResolveError = 3,

      /// Resolving of index service URL for destination failed
      WriteResolveError = 4,

      /// Can't read from source
      ReadStartError = 5,

      /// Can't write to destination
      WriteStartError = 6,

      /// Failed while reading from source
      ReadError = 7,

      /// Failed while writing to destination
      WriteError = 8,

      /// Failed while transfering data (mostly timeout)
      TransferError = 9,

      /// Failed while finishing reading from source
      ReadStopError = 10,

      /// Failed while finishing writing to destination
      WriteStopError = 11,

      /// First stage of registration of index service URL failed
      PreRegisterError = 12,

      /// Last stage of registration of index service URL failed
      PostRegisterError = 13,

      /// Unregistration of index service URL failed
      UnregisterError = 14,

      /// Error in caching procedure
      CacheError = 15,

      /// Error in caching procedure (retryable)
      CacheErrorRetryable = 16,

      /// Error due to provided credentials are expired
      CredentialsExpiredError = 17,

      /// Error deleting location or URL
      DeleteError = 18,

      /// No valid location available
      NoLocationError = 19,

      /// No valid location available
      LocationAlreadyExistsError = 20,

      /// Operation has no sense for this kind of URL
      NotSupportedForDirectDataPointsError = 21,

      /// Feature is unimplemented
      UnimplementedError = 22,

      /// DataPoint is already reading
      IsReadingError = 23,

      /// DataPoint is already writing
      IsWritingError = 24,

      /// Access check failed
      CheckError = 25,

      /// File listing failed
      ListError = 26,

      /// Object initialization failed
      NotInitializedError = 27,

      /// Undefined
      UnknownError = 27
    };

    DataStatus(const DataStatusType& status)
      : status(status) {}
    ~DataStatus() {}

    bool operator==(const DataStatusType& s) {
      return status == s;
    }
    bool operator==(const DataStatus& s) {
      return status == s.status;
    }

    bool operator!() {
      return status != Success;
    }
    operator bool() {
      return status == Success;
    }

    bool Passed(void) {
      return (status == Success) || (status == NotSupportedForDirectDataPointsError);
    }

    operator std::string(void) const;

  private:

    DataStatusType status;

  };

  inline std::ostream& operator<<(std::ostream& o, const DataStatus& d) {
    return (o << ((std::string)d));
  }
} // namespace Arc


#endif // __ARC_DATASTATUS_H__
