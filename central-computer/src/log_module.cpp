#include "log_module.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace submarine {

namespace {

// Same algorithm as the LNC firmware's task_log.c YmdToDayNumber - ported
// rather than re-derived, and already verified there against 4018 real
// calendar dates (2020-2030) compared to libc's timegm. Returns a
// proleptic-Gregorian day count (Howard Hinnant's civil_from_days) so two
// YYYYMMDD values can be subtracted to get an exact day difference,
// including across month/year/leap-year boundaries.
int32_t YmdToDayNumber(uint32_t ymd) {
  int32_t year = static_cast<int32_t>(ymd / 10000u);
  int32_t month = static_cast<int32_t>((ymd / 100u) % 100u);
  int32_t day = static_cast<int32_t>(ymd % 100u);
  int32_t y = year - (month <= 2 ? 1 : 0);
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = static_cast<uint32_t>(y - era * 400);
  uint32_t mp = static_cast<uint32_t>((month + 9) % 12);
  uint32_t doy = (153u * mp + 2u) / 5u + static_cast<uint32_t>(day) - 1u;
  uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return era * 146097 + static_cast<int32_t>(doe);
}

std::string ymdFileName(uint32_t ymd) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%08u.TXT", ymd);
  return buf;
}

bool parseYmdFileName(const std::string& name, uint32_t* outYmd) {
  if (name.size() != 12) return false;
  for (int i = 0; i < 8; i++) {
    if (name[i] < '0' || name[i] > '9') return false;
  }
  if (name.substr(8) != ".TXT") return false;
  uint32_t ymd = 0;
  for (int i = 0; i < 8; i++) ymd = ymd * 10u + static_cast<uint32_t>(name[i] - '0');
  *outYmd = ymd;
  return true;
}

constexpr int kRetentionDays = 7;

}  // namespace

LogModule::LogModule(std::string logDir) : logDir_(std::move(logDir)) {
  fs::create_directories(logDir_);
}

void LogModule::writeLine(uint32_t ymd, const std::string& line) {
  std::lock_guard<std::mutex> lock(mutex_);
  fs::path path = fs::path(logDir_) / ymdFileName(ymd);
  std::ofstream out(path, std::ios::app);
  out << line << "\n";
  std::cout << "[CentralComputer LOG] " << line << "\n";
}

std::string LogModule::formatMode(const proto::Message& msg) const {
  auto mode = msg.find(TAG_MODE);
  if (!mode) return "?";
  switch (mode->asU8().value_or(0xFF)) {
    case 0: return "NORMAL";
    case 1: return "WARNING";
    case 2: return "ERROR";
    default: return "?";
  }
}

void LogModule::recordKeepAlive(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms) {
  std::ostringstream line;
  line << nowHms << " KEEPALIVE mode=" << formatMode(msg);
  if (auto light = msg.find(TAG_LIGHT_RAW)) line << " light=" << light->asU32().value_or(0);
  if (auto temp = msg.find(TAG_TEMP_ADC_RAW)) line << " tempADC=" << temp->asU32().value_or(0);
  if (auto batt = msg.find(TAG_BATTERY_RAW)) line << " batt=" << batt->asU32().value_or(0);
  if (auto dt = msg.find(TAG_DHT_TEMP)) line << " dhtTemp=" << static_cast<int>(dt->asU8().value_or(0));
  if (auto dh = msg.find(TAG_DHT_HUMIDITY)) line << " dhtHum=" << static_cast<int>(dh->asU8().value_or(0));
  writeLine(nowYmd, line.str());
}

void LogModule::recordEvent(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms) {
  static const char* kEventNames[] = {"?", "OBJECT_DETECTED", "OBJECT_CLEARED", "SILENCE_PRESSED", "MODE_CHANGED"};
  std::ostringstream line;
  uint8_t eventType = msg.find(TAG_EVENT_TYPE) ? msg.find(TAG_EVENT_TYPE)->asU8().value_or(0) : 0;
  const char* name = (eventType < 5) ? kEventNames[eventType] : "?";
  line << nowHms << " EVENT " << name << " mode=" << formatMode(msg);
  if (auto value = msg.find(TAG_EVENT_VALUE)) line << " value=" << value->asU32().value_or(0);
  writeLine(nowYmd, line.str());
}

void LogModule::enforceRetention(uint32_t todayYmd) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!fs::exists(logDir_)) return;
  int32_t todayDays = YmdToDayNumber(todayYmd);
  for (const auto& entry : fs::directory_iterator(logDir_)) {
    if (!entry.is_regular_file()) continue;
    uint32_t fileYmd = 0;
    if (!parseYmdFileName(entry.path().filename().string(), &fileYmd)) continue;
    if (todayDays - YmdToDayNumber(fileYmd) >= kRetentionDays) {
      std::error_code ec;
      fs::remove(entry.path(), ec);
    }
  }
}

}  // namespace submarine
