#ifndef CENTRAL_COMPUTER_PROTOCOL_BRIDGE_H
#define CENTRAL_COMPUTER_PROTOCOL_BRIDGE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// This header wraps the LNC firmware's own protocol source, compiled
// unmodified into this project (see CMakeLists.txt's `protocol` target) -
// tlv.h/tlv.c, comm_frame.h/comm_frame.c and comm_tags.h are the exact same
// files that live in submarine-final-project/Core/{Inc,Src}/app and were
// already host-compiled and unit-tested there (including the real
// decoder-resync bug that testing caught and fixed). Reusing them directly,
// rather than re-implementing the wire format a second time in C++, is a
// deliberate choice: there is exactly one implementation of the protocol in
// the whole system, so the firmware and this PC app can never drift apart
// on frame/TLV encoding.
extern "C" {
#include "comm_frame.h"
#include "comm_tags.h"
#include "tlv.h"
}

namespace submarine::proto {

// One decoded TLV field, with the raw tag/value plus convenience accessors.
// Mirrors TLV_Field_t but owns its bytes (TLV_Field_t::value only points
// into whatever buffer was decoded, which is not safe to hold onto).
struct Field {
  uint8_t tag = 0;
  std::vector<uint8_t> value;

  std::optional<uint8_t> asU8() const;
  std::optional<uint16_t> asU16() const;
  std::optional<uint32_t> asU32() const;
  std::optional<int32_t> asI32() const;
  std::string asString() const;  // for TAG_LOG_LINE - raw bytes as text
};

// A fully decoded message: the outer frame's type plus every TLV field
// found in its payload, in wire order.
struct Message {
  uint8_t type = 0;
  std::vector<Field> fields;

  // Returns the first field with this tag, if present.
  const Field* find(uint8_t tag) const;
};

// --- Encoding -------------------------------------------------------------

// A small builder mirroring TLV_Encode/TLV_EncodeU8/etc. but appending into
// a std::vector<uint8_t> instead of a fixed C buffer, then a matching
// CommFrame_Encode wrapper to produce the final on-wire bytes.
class MessageBuilder {
 public:
  MessageBuilder& u8(uint8_t tag, uint8_t val);
  MessageBuilder& u16(uint8_t tag, uint16_t val);
  MessageBuilder& u32(uint8_t tag, uint32_t val);
  MessageBuilder& i32(uint8_t tag, int32_t val);
  MessageBuilder& raw(uint8_t tag, const std::vector<uint8_t>& bytes);

  // Produces the final STX..ETX byte sequence for the given outer message
  // type. Returns an empty vector if the payload built so far exceeds
  // COMM_FRAME_MAX_PAYLOAD (128 bytes) - callers should treat that as a
  // logic error (it never happens for this project's fixed-shape
  // messages) rather than a runtime condition to recover from.
  std::vector<uint8_t> encode(uint8_t msgType) const;

 private:
  std::vector<uint8_t> payload_;
};

// --- Decoding ---------------------------------------------------------------

// Streaming decoder wrapping CommFrameDecoder_t. Feed it raw bytes as they
// arrive from an ITransport; whenever a complete, checksum-valid frame is
// assembled, its TLV payload is parsed into a Message and appended to the
// vector returned by feed().
class MessageDecoder {
 public:
  MessageDecoder();

  // Feeds one chunk of raw bytes and returns every complete Message found
  // within it (usually 0 or 1, but a burst of buffered bytes can complete
  // more than one frame in a single call).
  std::vector<Message> feed(const std::vector<uint8_t>& bytes);

 private:
  CommFrameDecoder_t decoder_{};
};

}  // namespace submarine::proto

#endif  // CENTRAL_COMPUTER_PROTOCOL_BRIDGE_H
