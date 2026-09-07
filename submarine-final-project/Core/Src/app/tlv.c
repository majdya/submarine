#include "tlv.h"

uint8_t TLV_Encode(uint8_t *buf, size_t buf_size, size_t *offset, uint8_t tag,
                    uint16_t length, const uint8_t *value) {
  if (!buf || !offset) {
    return 0;
  }
  size_t needed = 1u + 2u + (size_t)length;
  if (*offset > buf_size || needed > buf_size - *offset) {
    return 0; /* wouldn't fit - buf/offset left untouched */
  }
  size_t pos = *offset;
  buf[pos++] = tag;
  buf[pos++] = (uint8_t)((length >> 8) & 0xFFu);
  buf[pos++] = (uint8_t)(length & 0xFFu);
  if (length > 0) {
    if (!value) {
      return 0;
    }
    for (uint16_t i = 0; i < length; i++) {
      buf[pos + i] = value[i];
    }
    pos += length;
  }
  *offset = pos;
  return 1;
}

uint8_t TLV_Decode(const uint8_t *buf, size_t buf_size, size_t *offset,
                    TLV_Field_t *out_field) {
  if (!buf || !offset || !out_field) {
    return 0;
  }
  size_t pos = *offset;
  if (pos > buf_size || buf_size - pos < 3u) {
    return 0; /* not even a full header left */
  }
  uint8_t tag = buf[pos];
  uint16_t length = ((uint16_t)buf[pos + 1] << 8) | (uint16_t)buf[pos + 2];
  pos += 3u;
  if (buf_size - pos < (size_t)length) {
    return 0; /* header claims more value bytes than remain - truncated/corrupt */
  }
  out_field->tag = tag;
  out_field->length = length;
  out_field->value = (length > 0) ? &buf[pos] : NULL;
  *offset = pos + length;
  return 1;
}

uint8_t TLV_EncodeU8(uint8_t *buf, size_t buf_size, size_t *offset,
                      uint8_t tag, uint8_t val) {
  return TLV_Encode(buf, buf_size, offset, tag, 1, &val);
}

uint8_t TLV_EncodeU16(uint8_t *buf, size_t buf_size, size_t *offset,
                       uint8_t tag, uint16_t val) {
  uint8_t bytes[2] = {(uint8_t)((val >> 8) & 0xFFu), (uint8_t)(val & 0xFFu)};
  return TLV_Encode(buf, buf_size, offset, tag, 2, bytes);
}

uint8_t TLV_EncodeU32(uint8_t *buf, size_t buf_size, size_t *offset,
                       uint8_t tag, uint32_t val) {
  uint8_t bytes[4] = {
      (uint8_t)((val >> 24) & 0xFFu), (uint8_t)((val >> 16) & 0xFFu),
      (uint8_t)((val >> 8) & 0xFFu), (uint8_t)(val & 0xFFu)};
  return TLV_Encode(buf, buf_size, offset, tag, 4, bytes);
}

uint8_t TLV_EncodeI32(uint8_t *buf, size_t buf_size, size_t *offset,
                       uint8_t tag, int32_t val) {
  return TLV_EncodeU32(buf, buf_size, offset, tag, (uint32_t)val);
}

uint8_t TLV_FieldAsU8(const TLV_Field_t *field, uint8_t *out) {
  if (!field || !out || field->length != 1 || !field->value) {
    return 0;
  }
  *out = field->value[0];
  return 1;
}

uint8_t TLV_FieldAsU16(const TLV_Field_t *field, uint16_t *out) {
  if (!field || !out || field->length != 2 || !field->value) {
    return 0;
  }
  *out = ((uint16_t)field->value[0] << 8) | (uint16_t)field->value[1];
  return 1;
}

uint8_t TLV_FieldAsU32(const TLV_Field_t *field, uint32_t *out) {
  if (!field || !out || field->length != 4 || !field->value) {
    return 0;
  }
  *out = ((uint32_t)field->value[0] << 24) | ((uint32_t)field->value[1] << 16) |
         ((uint32_t)field->value[2] << 8) | (uint32_t)field->value[3];
  return 1;
}

uint8_t TLV_FieldAsI32(const TLV_Field_t *field, int32_t *out) {
  uint32_t u;
  if (!TLV_FieldAsU32(field, &u)) {
    return 0;
  }
  *out = (int32_t)u;
  return 1;
}
