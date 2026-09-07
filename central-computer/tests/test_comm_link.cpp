// Exercises CommLink against LoopbackTransport with a small fake-LNC
// thread that answers commands the way the real firmware's app_command.c
// does, including a multi-frame GET_EVENTS-style stream terminated by an
// empty DATA_REPORT - this is the same request/response shape verified
// live against real hardware via tools/lnc_test_tool.py.

#include <atomic>
#include <thread>

#include "comm_link.h"
#include "transport/loopback_transport.h"
#include "test_harness.h"

using namespace submarine;
using submarine::proto::MessageBuilder;
using submarine::proto::MessageDecoder;

// Runs on its own thread: reads whatever CommLink writes to the loopback
// transport, decodes it, and pushes back a canned reply - standing in for
// the LNC's Task_CommRx + app_command.c.
static void fakeLncThread(LoopbackTransport& transport, std::atomic<bool>& stop) {
  MessageDecoder decoder;
  while (!stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    // NOTE: takeWritten(), not readSome() - readSome() would read from the
    // same incoming_ queue CommLink's own reader thread is draining (the
    // "toward CommLink" direction), stealing bytes from it in a race.
    // takeWritten() is the "what did CommLink just send" direction, which
    // is what a fake LNC needs to see.
    auto bytes = transport.takeWritten();
    if (bytes.empty()) continue;
    for (const auto& msg : decoder.feed(bytes)) {
      if (msg.type == MSG_TYPE_CMD_SET_TIME) {
        MessageBuilder ack;
        ack.u8(TAG_STATUS, STATUS_OK);
        transport.feedIncoming(ack.encode(MSG_TYPE_ACK));
      } else if (msg.type == MSG_TYPE_CMD_GET_EVENTS) {
        MessageBuilder line1;
        line1.raw(TAG_LOG_LINE, {'A', ' ', '1'});
        transport.feedIncoming(line1.encode(MSG_TYPE_DATA_REPORT));

        MessageBuilder line2;
        line2.raw(TAG_LOG_LINE, {'A', ' ', '2'});
        transport.feedIncoming(line2.encode(MSG_TYPE_DATA_REPORT));

        MessageBuilder terminator;  // empty payload = end of stream
        transport.feedIncoming(terminator.encode(MSG_TYPE_DATA_REPORT));
      }
    }
  }
}

static void test_send_and_wait_for_ack() {
  LoopbackTransport transport;
  transport.open();
  std::atomic<bool> stop{false};
  std::thread fake(fakeLncThread, std::ref(transport), std::ref(stop));

  CommLink link(transport, [](const proto::Message&) {
    CHECK(false && "no unsolicited message expected in this test");
  });
  link.start();

  MessageBuilder cmd;
  cmd.u16(TAG_TIMESTAMP_YEAR, 2026).u8(TAG_TIMESTAMP_MONTH, 9);
  auto reply = link.sendAndWait(cmd.encode(MSG_TYPE_CMD_SET_TIME), 1000);
  CHECK(reply.has_value());
  if (reply) {
    CHECK(reply->type == MSG_TYPE_ACK);
    auto status = reply->find(TAG_STATUS);
    CHECK(status && status->asU8() == STATUS_OK);
  }

  link.stop();
  stop = true;
  fake.join();
}

static void test_send_and_stream_get_events() {
  LoopbackTransport transport;
  transport.open();
  std::atomic<bool> stop{false};
  std::thread fake(fakeLncThread, std::ref(transport), std::ref(stop));

  CommLink link(transport, [](const proto::Message&) {});
  link.start();

  MessageBuilder cmd;
  cmd.u32(TAG_RANGE_START_YMD, 20260901).u32(TAG_RANGE_END_YMD, 20260905);

  std::vector<std::string> lines;
  bool complete = link.sendAndStream(
      cmd.encode(MSG_TYPE_CMD_GET_EVENTS),
      [&](const proto::Message& msg) {
        auto line = msg.find(TAG_LOG_LINE);
        if (line) lines.push_back(line->asString());
      },
      500, 2000);

  CHECK(complete);
  CHECK(lines.size() == 2);
  if (lines.size() == 2) {
    CHECK(lines[0] == "A 1");
    CHECK(lines[1] == "A 2");
  }

  link.stop();
  stop = true;
  fake.join();
}

static void test_unsolicited_keepalive_reaches_callback() {
  LoopbackTransport transport;
  transport.open();

  std::atomic<int> unsolicitedCount{0};
  CommLink link(transport, [&](const proto::Message& msg) {
    CHECK(msg.type == MSG_TYPE_KEEPALIVE);
    unsolicitedCount++;
  });
  link.start();

  MessageBuilder keepalive;
  keepalive.u8(TAG_MODE, 0);
  transport.feedIncoming(keepalive.encode(MSG_TYPE_KEEPALIVE));

  // Give the reader thread a moment to pick it up.
  for (int i = 0; i < 50 && unsolicitedCount == 0; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  CHECK(unsolicitedCount == 1);

  link.stop();
}

int main() {
  test_send_and_wait_for_ack();
  test_send_and_stream_get_events();
  test_unsolicited_keepalive_reaches_callback();
  TEST_MAIN_EXIT();
}
