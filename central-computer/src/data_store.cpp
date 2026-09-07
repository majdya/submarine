#include "data_store.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace submarine {

namespace {
constexpr char kFieldSep = ',';

std::vector<std::string> splitFields(const std::string& line) {
  std::vector<std::string> fields;
  std::stringstream ss(line);
  std::string field;
  while (std::getline(ss, field, kFieldSep)) fields.push_back(field);
  return fields;
}
}  // namespace

DataStore::DataStore(std::string dataDir) : dataDir_(std::move(dataDir)) {
  fs::create_directories(dataDir_);
}

void DataStore::appendMeasurement(const MeasurementRecord& r) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ofstream out(fs::path(dataDir_) / "measurements.csv", std::ios::app);
  out << r.ymd << kFieldSep << r.hms << kFieldSep << static_cast<int>(r.mode) << kFieldSep
      << r.lightRaw << kFieldSep << r.tempAdcRaw << kFieldSep << r.batteryRaw << kFieldSep
      << static_cast<int>(r.dhtTemp) << kFieldSep << static_cast<int>(r.dhtHumidity) << kFieldSep
      << static_cast<int>(r.dhtValid) << "\n";
}

void DataStore::appendEvent(const EventRecord& r) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ofstream out(fs::path(dataDir_) / "events.csv", std::ios::app);
  out << r.ymd << kFieldSep << r.hms << kFieldSep << static_cast<int>(r.eventType) << kFieldSep
      << r.eventValue << kFieldSep << static_cast<int>(r.mode) << "\n";
}

std::vector<MeasurementRecord> DataStore::queryMeasurements(uint32_t startYmd, uint32_t endYmd) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<MeasurementRecord> results;
  std::ifstream in(fs::path(dataDir_) / "measurements.csv");
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto f = splitFields(line);
    if (f.size() != 9) continue;  // skip a malformed/partial line rather than crash
    uint32_t ymd = static_cast<uint32_t>(std::stoul(f[0]));
    if (ymd < startYmd || ymd > endYmd) continue;
    MeasurementRecord r;
    r.ymd = ymd;
    r.hms = f[1];
    r.mode = static_cast<uint8_t>(std::stoi(f[2]));
    r.lightRaw = static_cast<uint32_t>(std::stoul(f[3]));
    r.tempAdcRaw = static_cast<uint32_t>(std::stoul(f[4]));
    r.batteryRaw = static_cast<uint32_t>(std::stoul(f[5]));
    r.dhtTemp = static_cast<uint8_t>(std::stoi(f[6]));
    r.dhtHumidity = static_cast<uint8_t>(std::stoi(f[7]));
    r.dhtValid = static_cast<uint8_t>(std::stoi(f[8]));
    results.push_back(r);
  }
  return results;
}

std::vector<EventRecord> DataStore::queryEvents(uint32_t startYmd, uint32_t endYmd) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<EventRecord> results;
  std::ifstream in(fs::path(dataDir_) / "events.csv");
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto f = splitFields(line);
    if (f.size() != 5) continue;
    uint32_t ymd = static_cast<uint32_t>(std::stoul(f[0]));
    if (ymd < startYmd || ymd > endYmd) continue;
    EventRecord r;
    r.ymd = ymd;
    r.hms = f[1];
    r.eventType = static_cast<uint8_t>(std::stoi(f[2]));
    r.eventValue = static_cast<uint32_t>(std::stoul(f[3]));
    r.mode = static_cast<uint8_t>(std::stoi(f[4]));
    results.push_back(r);
  }
  return results;
}

}  // namespace submarine
