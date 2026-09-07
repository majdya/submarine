#include "data_collection.h"

namespace submarine {

void DataCollection::recordKeepAlive(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms) {
  MeasurementRecord r;
  r.ymd = nowYmd;
  r.hms = nowHms;
  if (auto f = msg.find(TAG_MODE)) r.mode = f->asU8().value_or(0);
  if (auto f = msg.find(TAG_LIGHT_RAW)) r.lightRaw = f->asU32().value_or(0);
  if (auto f = msg.find(TAG_TEMP_ADC_RAW)) r.tempAdcRaw = f->asU32().value_or(0);
  if (auto f = msg.find(TAG_BATTERY_RAW)) r.batteryRaw = f->asU32().value_or(0);
  if (auto f = msg.find(TAG_DHT_TEMP)) r.dhtTemp = f->asU8().value_or(0);
  if (auto f = msg.find(TAG_DHT_HUMIDITY)) r.dhtHumidity = f->asU8().value_or(0);
  if (auto f = msg.find(TAG_DHT_VALID)) r.dhtValid = f->asU8().value_or(0);
  store_.appendMeasurement(r);
}

void DataCollection::recordEvent(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms) {
  EventRecord r;
  r.ymd = nowYmd;
  r.hms = nowHms;
  if (auto f = msg.find(TAG_EVENT_TYPE)) r.eventType = f->asU8().value_or(0);
  if (auto f = msg.find(TAG_EVENT_VALUE)) r.eventValue = f->asU32().value_or(0);
  if (auto f = msg.find(TAG_MODE)) r.mode = f->asU8().value_or(0);
  store_.appendEvent(r);
}

std::map<uint8_t, int> DataCollection::measurementCountsByMode(uint32_t startYmd, uint32_t endYmd) const {
  std::map<uint8_t, int> counts;
  for (const auto& r : store_.queryMeasurements(startYmd, endYmd)) counts[r.mode]++;
  return counts;
}

std::map<uint8_t, int> DataCollection::eventCountsByType(uint32_t startYmd, uint32_t endYmd) const {
  std::map<uint8_t, int> counts;
  for (const auto& r : store_.queryEvents(startYmd, endYmd)) counts[r.eventType]++;
  return counts;
}

}  // namespace submarine
