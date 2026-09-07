#include "central_computer.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace submarine {

namespace {
// "now" as YYYYMMDD + HH:MM:SS, used to timestamp incoming keep-alive/event
// frames on arrival (the PC's own clock, independent of the LNC's RTC).
void nowYmdHms(uint32_t* ymd, std::string* hms) {
  std::time_t t = std::time(nullptr);
  std::tm tmVal{};
#if defined(_WIN32)
  localtime_s(&tmVal, &t);
#else
  localtime_r(&t, &tmVal);
#endif
  *ymd = static_cast<uint32_t>((tmVal.tm_year + 1900) * 10000 + (tmVal.tm_mon + 1) * 100 + tmVal.tm_mday);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);
  *hms = buf;
}
}  // namespace

CentralComputer::CentralComputer(std::string logDir, std::string dataDir,
                                  std::unique_ptr<ITransport> transport)
    : transport_(std::move(transport)),
      commLink_(*transport_, [this](const proto::Message& msg) { onUnsolicited(msg); }),
      dataStore_(std::move(dataDir)),
      logModule_(std::move(logDir)),
      dataCollection_(dataStore_),
      managementCommand_(commLink_) {}

CentralComputer::~CentralComputer() { disconnect(); }

bool CentralComputer::connect() {
  if (!transport_->open()) return false;
  commLink_.start();
  return true;
}

void CentralComputer::disconnect() {
  commLink_.stop();
  transport_->close();
}

bool CentralComputer::isConnected() const { return transport_->isOpen(); }

namespace {
const char* eventName(uint8_t type) {
  static const char* kNames[] = {"?", "OBJECT_DETECTED", "OBJECT_CLEARED", "SILENCE_PRESSED", "MODE_CHANGED"};
  return (type < 5) ? kNames[type] : "?";
}
}  // namespace

void CentralComputer::onUnsolicited(const proto::Message& msg) {
  uint32_t ymd;
  std::string hms;
  nowYmdHms(&ymd, &hms);

  if (msg.type == MSG_TYPE_KEEPALIVE) {
    logModule_.recordKeepAlive(msg, ymd, hms);
    dataCollection_.recordKeepAlive(msg, ymd, hms);

    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_.hasData = true;
    snapshot_.receivedAtHms = hms;
    if (auto f = msg.find(TAG_MODE)) snapshot_.mode = f->asU8().value_or(0);
    if (auto f = msg.find(TAG_LIGHT_RAW)) snapshot_.lightRaw = f->asU32().value_or(0);
    if (auto f = msg.find(TAG_TEMP_ADC_RAW)) snapshot_.tempAdcRaw = f->asU32().value_or(0);
    if (auto f = msg.find(TAG_BATTERY_RAW)) snapshot_.batteryRaw = f->asU32().value_or(0);
    if (auto f = msg.find(TAG_DHT_TEMP)) snapshot_.dhtTemp = f->asU8().value_or(0);
    if (auto f = msg.find(TAG_DHT_HUMIDITY)) snapshot_.dhtHumidity = f->asU8().value_or(0);
    if (auto f = msg.find(TAG_DHT_VALID)) snapshot_.dhtValid = f->asU8().value_or(0);
  } else if (msg.type == MSG_TYPE_EVENT) {
    logModule_.recordEvent(msg, ymd, hms);
    dataCollection_.recordEvent(msg, ymd, hms);

    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_.hasData = true;
    uint8_t eventType = msg.find(TAG_EVENT_TYPE) ? msg.find(TAG_EVENT_TYPE)->asU8().value_or(0) : 0;
    snapshot_.lastEventDescription = eventName(eventType);
    snapshot_.lastEventAtHms = hms;
    if (auto f = msg.find(TAG_MODE)) snapshot_.mode = f->asU8().value_or(0);
  }
}

LiveSnapshot CentralComputer::latestSnapshot() const {
  std::lock_guard<std::mutex> lock(snapshotMutex_);
  return snapshot_;
}

}  // namespace submarine
