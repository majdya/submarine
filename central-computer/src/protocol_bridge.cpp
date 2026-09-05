#include "protocol_bridge.h"

#include <cstring>

namespace submarine::proto {

// ---- Field -----------------------------------------------------------------

std::optional<uint8_t> Field::asU8() const {
  TLV_Field_t f{tag, static_cast<uint16_t>(value.size()), value.empty() ? nullptr : value.data()};
  uint8_t out = 0;
  if (!TLV_FieldAsU8(&f, &out)) return std::nullopt;
  return out;
}

std::optional<uint16_t> Field::asU16() const {
  TLV_Field_t f{tag, static_cast<uint16_t>(value.size()), value.empty() ? nullptr : value.data()};
  uint16_t out = 0;
  if (!TLV_FieldAsU16(&f, &out)) return std::nullopt;
  return out;
}

std::optional<uint32_t> Field::asU32() const {
  TLV_Field_t f{tag, static_cast<uint16_t>(value.size()), value.empty() ? nullptr : value.data()};
  uint32_t out = 0;
  if (!TLV_FieldAsU32(&f, &out)) return std::nullopt;
  return out;
}

std::optional<int32_t> Field::asI32() const {
  TLV_Field_t f{tag, static_cast<uint16_t>(value.size()), value.empty() ? nullptr : value.data()};
  int32_t out = 0;
  if (!TLV_FieldAsI32(&f, &out)) return std::nullopt;
  return out;
}

std::string Field::asString() const {
  return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

// ---- Message -----------------------------------------------------------------

const Field* Message::find(uint8_t tag) const {
  for (const auto& f : fields) {
    if (f.tag == tag) return &f;
  }
  return nullptr;
}

// ---- MessageBuilder ------------------------------------------------------

MessageBuilder& MessageBuilder::u8(uint8_t tag, uint8_t val) {
  size_t offset = payload_.size();
  payload_.resize(offset + 3 + 1);
  TLV_EncodeU8(payload_.data(), payload_.size(), &offset, tag, val);
  payload_.resize(offset);
  return *this;
}

MessageBuilder& MessageBuilder::u16(uint8_t tag, uint16_t val) {
  size_t offset = payload_.size();
  payload_.resize(offset + 3 + 2);
  TLV_EncodeU16(payload_.data(), payload_.size(), &offset, tag, val);
  payload_.resize(offset);
  return *this;
}

MessageBuilder& MessageBuilder::u32(uint8_t tag, uint32_t val) {
  size_t offset = payload_.size();
  payload_.resize(offset + 3 + 4);
  TLV_EncodeU32(payload_.data(), payload_.size(), &offset, tag, val);
  payload_.resize(offset);
  return *this;
}

MessageBuilder& MessageBuilder::i32(uint8_t tag, int32_t val) {
  size_t offset = payload_.size();
  payload_.resize(offset + 3 + 4);
  TLV_EncodeI32(payload_.data(), payload_.size(), &offset, tag, val);
  payload_.resize(offset);
  return *this;
}

MessageBuilder& MessageBuilder::raw(uint8_t tag, const std::vector<uint8_t>& bytes) {
  size_t offset = payload_.size();
  payload_.resize(offset + 3 + bytes.size());
  TLV_Encode(payload_.data(), payload_.size(), &offset, tag,
             static_cast<uint16_t>(bytes.size()), bytes.empty() ? nullptr : bytes.data());
  payload_.resize(offset);
  return *this;
}

std::vector<uint8_t> MessageBuilder::encode(uint8_t msgType) const {
  if (payload_.size() > COMM_FRAME_MAX_PAYLOAD) return {};
  std::vector<uint8_t> out(COMM_FRAME_OVERHEAD + payload_.size());
  size_t written = CommFrame_Encode(out.data(), out.size(), msgType,
                                     payload_.empty() ? nullptr : payload_.data(),
                                     static_cast<uint16_t>(payload_.size()));
  if (written == 0) return {};
  out.resize(written);
  return out;
}

// ---- MessageDecoder ------------------------------------------------------

MessageDecoder::MessageDecoder() { CommFrame_DecoderInit(&decoder_); }

std::vector<Message> MessageDecoder::feed(const std::vector<uint8_t>& bytes) {
  std::vector<Message> messages;
  for (uint8_t b : bytes) {
    if (!CommFrame_DecoderFeed(&decoder_, b)) continue;

    Message msg;
    msg.type = decoder_.type;

    size_t offset = 0;
    TLV_Field_t field;
    while (TLV_Decode(decoder_.payload, decoder_.len, &offset, &field)) {
      Field f;
      f.tag = field.tag;
      if (field.length > 0 && field.value) {
        f.value.assign(field.value, field.value + field.length);
      }
      msg.fields.push_back(std::move(f));
    }
    messages.push_back(std::move(msg));
  }
  return messages;
}

}  // namespace submarine::proto
