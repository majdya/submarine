#ifndef CENTRAL_COMPUTER_DATA_COLLECTION_H
#define CENTRAL_COMPUTER_DATA_COLLECTION_H

#include <map>
#include <string>

#include "data_store.h"
#include "protocol_bridge.h"

namespace submarine {

// Spec section 3.4, Data Collection & Analysis Module: persists LNC data
// and events, and "prepares reports from the stored data, broken down by
// various criteria." This class is the analysis half; DataStore
// (data_store.h) is the persistence half it's built on.
class DataCollection {
 public:
  explicit DataCollection(DataStore& store) : store_(store) {}

  // Extracts and persists the fields of an incoming KEEPALIVE/EVENT
  // message. now* let the caller supply the reception timestamp.
  void recordKeepAlive(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms);
  void recordEvent(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms);

  // "Broken down by various criteria" - two concrete breakdowns: how many
  // measurements fell in each operating mode, and how many events of each
  // type occurred, both restricted to a date range.
  std::map<uint8_t, int> measurementCountsByMode(uint32_t startYmd, uint32_t endYmd) const;
  std::map<uint8_t, int> eventCountsByType(uint32_t startYmd, uint32_t endYmd) const;

 private:
  DataStore& store_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_DATA_COLLECTION_H
