#ifndef APP_TLV_H
#define APP_TLV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generic Tag-Length-Value primitives, per the spec's requirement that all
   system messages are TLV-based. Pure data manipulation only - no framing
   (start/end markers, checksums), no I/O. The outer message frame lives
   in comm_frame.h, and this project's actual tag numbers live in
   comm_tags.h, both built on top of this.

   Wire layout of one field: [tag: 1 byte][length: 2 bytes, big-endian]
   [value: length bytes]. A 2-byte length supports value payloads up to
   65535 bytes, far more than anything this project sends. */

typedef struct {
  uint8_t tag;
  uint16_t length;
  const uint8_t *value; /* points into the buffer passed to TLV_Decode -
                            NOT owned, NOT valid after that buffer changes */
} TLV_Field_t;

/* Appends one TLV field to buf, starting at *offset, and advances *offset
   past it. Returns 1 on success, 0 if it wouldn't fit in buf_size (buf is
   left unchanged on failure). value may be NULL only if length is 0. */
uint8_t TLV_Encode(uint8_t *buf, size_t buf_size, size_t *offset, uint8_t tag,
                    uint16_t length, const uint8_t *value);

/* Reads one TLV field from buf starting at *offset, advances *offset past
   it, and fills out_field (out_field->value points into buf itself).
   Returns 1 on success, 0 if the field's header or value would run past
   buf_size (a truncated/corrupt buffer) - *offset is left unchanged on
   failure so the caller can distinguish "no more fields" from "garbage". */
uint8_t TLV_Decode(const uint8_t *buf, size_t buf_size, size_t *offset,
                    TLV_Field_t *out_field);

/* Convenience encoders for the scalar sizes this project's fields actually
   use, packed big-endian (network byte order) so a value's bytes read the
   same regardless of which end is little- or big-endian internally). */
uint8_t TLV_EncodeU8(uint8_t *buf, size_t buf_size, size_t *offset,
                      uint8_t tag, uint8_t val);
uint8_t TLV_EncodeU16(uint8_t *buf, size_t buf_size, size_t *offset,
                       uint8_t tag, uint16_t val);
uint8_t TLV_EncodeU32(uint8_t *buf, size_t buf_size, size_t *offset,
                       uint8_t tag, uint32_t val);
uint8_t TLV_EncodeI32(uint8_t *buf, size_t buf_size, size_t *offset,
                       uint8_t tag, int32_t val);

/* Convenience decoders - read a big-endian scalar out of an already-decoded
   field's value. Return 0 (leaving *out unchanged) if field->length doesn't
   match the expected size, so callers can't silently misread a field. */
uint8_t TLV_FieldAsU8(const TLV_Field_t *field, uint8_t *out);
uint8_t TLV_FieldAsU16(const TLV_Field_t *field, uint16_t *out);
uint8_t TLV_FieldAsU32(const TLV_Field_t *field, uint32_t *out);
uint8_t TLV_FieldAsI32(const TLV_Field_t *field, int32_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_TLV_H */
