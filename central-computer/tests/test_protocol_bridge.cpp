// Verifies the C++ MessageBuilder/MessageDecoder wrapper against the real
// firmware protocol source (tlv.c/comm_frame.c, compiled unmodified into
// this project - see protocol_bridge.h) and against LoopbackTransport,
// mirroring exactly the kind of message a real LNC keep-alive/event frame
// and a real Central Computer command look like on the wire.

#include "protocol_bridge.h"
#include "transport/loopback_transport.h"
#include "test_harness.h"

using submarine::proto::MessageBuilder;
using submarine::proto::MessageDecoder;
using submarine::LoopbackTransport;

static void test_roundtrip_keepalive_like_message() {
  MessageBuilder b;
  b.u16(TAG_TIMESTAMP_YEAR, 2026)
      .u8(TAG_TIMESTAMP_MONTH, 9)
      .u8(TAG_TIMESTAMP_DAY, 5)
      .u8(TAG_TIMESTAMP_HOUR, 21)
      .u8(TAG_TIMESTAMP_MIN, 42)
      .u8(TAG_TIMESTAMP_SEC, 10)
      .u8(TAG_MODE, 0)
      .u32(TAG_LIGHT_RAW, 384)
      .u32(TAG_TEMP_ADC_RAW, 0)
      .u32(TAG_BATTERY_RAW, 4095)
      .u8(TAG_DHT_TEMP, 25)
      .u8(TAG_DHT_HUMIDITY, 36)
      .u8(TAG_DHT_VALID, 1);
  auto wire = b.encode(MSG_TYPE_KEEPALIVE);
  CHECK(!wire.empty());
  CHECK(wire.front() == COMM_FRAME_STX);
  CHECK(wire.back() == COMM_FRAME_ETX);

  MessageDecoder dec;
  auto messages = dec.feed(wire);
  CHECK(messages.size() == 1);
  if (messages.size() == 1) {
    const auto& msg = messages[0];
    CHECK(msg.type == MSG_TYPE_KEEPALIVE);
    CHECK(msg.fields.size() == 13);
    CHECK(msg.find(TAG_TIMESTAMP_YEAR) && msg.find(TAG_TIMESTAMP_YEAR)->asU16() == 2026);
    CHECK(msg.find(TAG_MODE) && msg.find(TAG_MODE)->asU8() == 0);
    CHECK(msg.find(TAG_BATTERY_RAW) && msg.find(TAG_BATTERY_RAW)->asU32() == 4095u);
    CHECK(msg.find(TAG_DHT_VALID) && msg.find(TAG_DHT_VALID)->asU8() == 1);
  }
}

static void test_roundtrip_negative_i32_field() {
  // SET_LIMITS-style command carrying a negative lower bound (a real case:
  // temp warning_min can be below zero).
  MessageBuilder b;
  b.u8(TAG_PARAM, PARAM_TEMP).i32(TAG_WARNING_MIN, -5).i32(TAG_NORMAL_MIN, 15);
  auto wire = b.encode(MSG_TYPE_CMD_SET_LIMITS);

  MessageDecoder dec;
  auto messages = dec.feed(wire);
  CHECK(messages.size() == 1);
  if (messages.size() == 1) {
    const auto& msg = messages[0];
    CHECK(msg.type == MSG_TYPE_CMD_SET_LIMITS);
    auto warnMin = msg.find(TAG_WARNING_MIN);
    CHECK(warnMin && warnMin->asI32() == -5);
  }
}

static void test_multi_frame_in_one_feed() {
  // Two back-to-back frames delivered in a single read (realistic: a burst
  // of buffered serial bytes) must both decode.
  MessageBuilder a;
  a.u8(TAG_EVENT_TYPE, 1).u32(TAG_EVENT_VALUE, 0);
  auto wireA = a.encode(MSG_TYPE_EVENT);

  MessageBuilder c;
  c.u8(TAG_STATUS, STATUS_OK);
  auto wireC = c.encode(MSG_TYPE_ACK);

  std::vector<uint8_t> both = wireA;
  both.insert(both.end(), wireC.begin(), wireC.end());

  MessageDecoder dec;
  auto messages = dec.feed(both);
  CHECK(messages.size() == 2);
  if (messages.size() == 2) {
    CHECK(messages[0].type == MSG_TYPE_EVENT);
    CHECK(messages[1].type == MSG_TYPE_ACK);
  }
}

static void test_loopback_transport_carries_a_frame() {
  LoopbackTransport transport;
  CHECK(transport.open());

  MessageBuilder b;
  b.u8(TAG_STATUS, STATUS_OK);
  auto wire = b.encode(MSG_TYPE_ACK);

  CHECK(transport.write(wire));
  auto written = transport.takeWritten();
  CHECK(written == wire);

  // Simulate the "LNC" echoing a frame back.
  transport.feedIncoming(wire);
  auto received = transport.readSome(64, 0);
  CHECK(received == wire);
}

int main() {
  test_roundtrip_keepalive_like_message();
  test_roundtrip_negative_i32_field();
  test_multi_frame_in_one_feed();
  test_loopback_transport_carries_a_frame();
  TEST_MAIN_EXIT();
}
