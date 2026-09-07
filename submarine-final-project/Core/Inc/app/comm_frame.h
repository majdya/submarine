#ifndef APP_COMM_FRAME_H
#define APP_COMM_FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Outer byte-stream framing for the LNC<->Central Computer UART link.
   TLV (tlv.h) encodes individual fields, but says nothing about where
   one whole MESSAGE (a concatenation of TLV fields) starts and ends on a
   continuous UART byte stream - that is what this frame adds:

     STX(1) TYPE(1) LEN_HI(1) LEN_LO(1) PAYLOAD(LEN bytes) CHECKSUM(1) ETX(1)

   PAYLOAD is length-delimited (LEN is trusted, not scanned for), so
   PAYLOAD bytes never need escaping even though they may legitimately
   contain 0x02/0x03. CHECKSUM is the XOR of TYPE, LEN_HI, LEN_LO and
   every PAYLOAD byte. ETX is a resync aid for a confused receiver, not
   load-bearing for parsing. */

#define COMM_FRAME_STX 0x02u
#define COMM_FRAME_ETX 0x03u
#define COMM_FRAME_MAX_PAYLOAD 128u
/* STX+TYPE+LEN_HI+LEN_LO+CHECKSUM+ETX = 6 bytes of overhead around PAYLOAD. */
#define COMM_FRAME_OVERHEAD 6u
#define COMM_FRAME_MAX_ENCODED (COMM_FRAME_OVERHEAD + COMM_FRAME_MAX_PAYLOAD)

/* Outgoing (LNC -> Central Computer) message types. */
#define MSG_TYPE_KEEPALIVE 0x01u
#define MSG_TYPE_EVENT 0x02u
/* Reply to a query (GET_TIME/GET_DATA/GET_EVENTS): one result per frame,
   terminated by an empty (payload_len == 0) DATA_REPORT frame so the
   receiver knows the result set is complete without a separate count
   field. */
#define MSG_TYPE_DATA_REPORT 0x03u
/* Accept/reject confirmation for a state-changing command (SET_LIMITS,
   SET_TIME) - carries TAG_STATUS, see comm_tags.h. */
#define MSG_TYPE_ACK 0x04u

/* Incoming (Central Computer -> LNC) command types. */
#define MSG_TYPE_CMD_SET_LIMITS 0x10u
#define MSG_TYPE_CMD_SET_TIME 0x11u
#define MSG_TYPE_CMD_GET_TIME 0x12u
#define MSG_TYPE_CMD_GET_DATA 0x13u
#define MSG_TYPE_CMD_GET_EVENTS 0x14u

/* Encodes one frame into out (out_size bytes available). Returns the
   number of bytes written (always COMM_FRAME_OVERHEAD + len on success),
   or 0 if len exceeds COMM_FRAME_MAX_PAYLOAD or out_size is too small -
   out is left untouched on failure. */
size_t CommFrame_Encode(uint8_t *out, size_t out_size, uint8_t type,
                         const uint8_t *payload, uint16_t len);

typedef enum {
  COMM_FRAME_WAIT_STX = 0,
  COMM_FRAME_WAIT_TYPE,
  COMM_FRAME_WAIT_LEN_HI,
  COMM_FRAME_WAIT_LEN_LO,
  COMM_FRAME_WAIT_PAYLOAD,
  COMM_FRAME_WAIT_CHECKSUM,
  COMM_FRAME_WAIT_ETX,
} CommFrameState_t;

/* Streaming decoder - feed it one received byte at a time (from a task,
   not an ISR: it's plain C state with no locking). type/len/payload only
   mean anything right after CommFrame_DecoderFeed returns 1. */
typedef struct {
  CommFrameState_t state;
  uint8_t type;
  uint16_t len;
  uint16_t received;
  uint8_t payload[COMM_FRAME_MAX_PAYLOAD];
  uint8_t checksum_accum;
  uint8_t expected_checksum;
} CommFrameDecoder_t;

void CommFrame_DecoderInit(CommFrameDecoder_t *dec);

/* Feed one byte. Returns 1 exactly when this byte completed a full,
   checksum- and ETX-valid frame (dec->type/len/payload now hold it) -
   the decoder is already reset and ready for the next frame by the time
   this returns, so read the result before feeding the next byte.
   Returns 0 while still assembling a frame, and also (silently) whenever
   noise, a bad checksum, or a missing ETX forces a resync back to
   COMM_FRAME_WAIT_STX. */
uint8_t CommFrame_DecoderFeed(CommFrameDecoder_t *dec, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_FRAME_H */
