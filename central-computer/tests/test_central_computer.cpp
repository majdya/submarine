// End-to-end test of CentralComputer: a LoopbackTransport stands in for
// the real COM port, a fake-LNC thread plays the firmware's role (answers
// SET_TIME and pushes an unsolicited KEEPALIVE + EVENT), and we verify the
// Log and DataCollection modules actually recorded what came in.

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "central_computer.h"
#include "transport/loopback_transport.h"
#include "test_harness.h"

namespace fs = std::filesystem;
using namespace submarine;
using submarine::proto::MessageBuilder;
using submarine::proto::MessageDecoder;

static void fakeLncThread(LoopbackTransport& transport, std::atomic<bool>& stop) {
  MessageDecoder decoder;
  bool pushedUnsolicited = false;
  while (!stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    if (!pushedUnsolicited) {
      MessageBuilder keepalive;
      keepalive.u8(TAG_MODE, 1).u32(TAG_LIGHT_RAW, 400).u32(TAG_TEMP_ADC_RAW, 0)
          .u32(TAG_BATTERY_RAW, 4095).u8(TAG_DHT_TEMP, 26).u8(TAG_DHT_HUMIDITY, 40);
      transport.feedIncoming(keepalive.encode(MSG_TYPE_KEEPALIVE));

      MessageBuilder event;
      event.u8(TAG_EVENT_TYPE, 1).u32(TAG_EVENT_VALUE, 0).u8(TAG_MODE, 1);
      transport.feedIncoming(event.encode(MSG_TYPE_EVENT));
      pushedUnsolicited = true;
    }

    auto bytes = transport.takeWritten();
    if (bytes.empty()) continue;
    for (const auto& msg : decoder.feed(bytes)) {
      if (msg.type == MSG_TYPE_CMD_SET_TIME) {
        MessageBuilder ack;
        ack.u8(TAG_STATUS, STATUS_OK);
        transport.feedIncoming(ack.encode(MSG_TYPE_ACK));
      }
    }
  }
}

static void test_central_computer_end_to_end() {
  fs::path tmpDir = fs::temp_directory_path() / "cc_test_XXXXXX";
  fs::path logDir = tmpDir / "logs";
  fs::path dataDir = tmpDir / "data";
  fs::remove_all(tmpDir);

  auto transport = std::make_unique<LoopbackTransport>();
  LoopbackTransport* transportPtr = transport.get();

  CentralComputer cc(logDir.string(), dataDir.string(), std::move(transport));
  CHECK(cc.connect());
  CHECK(cc.isConnected());

  std::atomic<bool> stop{false};
  std::thread fake(fakeLncThread, std::ref(*transportPtr), std::ref(stop));

  // Give the unsolicited KEEPALIVE/EVENT time to arrive and be processed.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  TimeStamp ts{2026, 9, 5, 22, 0, 0};
  CHECK(cc.commands().setTime(ts, 1000));

  stop = true;
  fake.join();
  cc.disconnect();

  // Log module: at least one log file should exist with our data in it.
  bool foundKeepaliveLine = false;
  bool foundEventLine = false;
  for (const auto& entry : fs::directory_iterator(logDir)) {
    std::ifstream in(entry.path());
    std::string line;
    while (std::getline(in, line)) {
      if (line.find("KEEPALIVE") != std::string::npos && line.find("batt=4095") != std::string::npos) {
        foundKeepaliveLine = true;
      }
      if (line.find("EVENT OBJECT_DETECTED") != std::string::npos) {
        foundEventLine = true;
      }
    }
  }
  CHECK(foundKeepaliveLine);
  CHECK(foundEventLine);

  // DataCollection: the keepalive and event should be queryable/countable.
  uint32_t today = 20000101;
  {
    std::time_t t = std::time(nullptr);
    std::tm tmVal{};
#if defined(_WIN32)
    localtime_s(&tmVal, &t);
#else
    localtime_r(&t, &tmVal);
#endif
    today = static_cast<uint32_t>((tmVal.tm_year + 1900) * 10000 + (tmVal.tm_mon + 1) * 100 + tmVal.tm_mday);
  }
  auto modeCounts = cc.dataCollection().measurementCountsByMode(today, today);
  CHECK(modeCounts[1] >= 1);  // mode=WARNING(1) from our fake keepalive

  auto eventCounts = cc.dataCollection().eventCountsByType(today, today);
  CHECK(eventCounts[1] >= 1);  // eventType=1 (OBJECT_DETECTED)

  fs::remove_all(tmpDir);
}

int main() {
  test_central_computer_end_to_end();
  TEST_MAIN_EXIT();
}
