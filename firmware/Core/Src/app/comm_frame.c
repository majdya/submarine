#include "comm_frame.h"

size_t CommFrame_Encode(uint8_t *out, size_t out_size, uint8_t type,
                         const uint8_t *payload, uint16_t len) {
  if (len > COMM_FRAME_MAX_PAYLOAD) {
    return 0;
  }
  size_t total = COMM_FRAME_OVERHEAD + (size_t)len;
  if (!out || out_size < total) {
    return 0;
  }

  size_t i = 0;
  out[i++] = (uint8_t)COMM_FRAME_STX;
  out[i++] = type;
  out[i++] = (uint8_t)((len >> 8) & 0xFFu);
  out[i++] = (uint8_t)(len & 0xFFu);

  uint8_t checksum = out[1] ^ out[2] ^ out[3];
  for (uint16_t j = 0; j < len; j++) {
    out[i++] = payload[j];
    checksum ^= payload[j];
  }

  out[i++] = checksum;
  out[i++] = (uint8_t)COMM_FRAME_ETX;
  return i;
}

/* Resets the byte-level parsing state only - type/len/payload are left
   alone so a caller that just received a completed frame can still read
   them after this runs (called from the WAIT_ETX success path too). */
static void ResetForNextFrame(CommFrameDecoder_t *dec) {
  dec->state = COMM_FRAME_WAIT_STX;
  dec->received = 0;
  dec->checksum_accum = 0;
}

void CommFrame_DecoderInit(CommFrameDecoder_t *dec) {
  ResetForNextFrame(dec);
  dec->type = 0;
  dec->len = 0;
  dec->expected_checksum = 0;
}

uint8_t CommFrame_DecoderFeed(CommFrameDecoder_t *dec, uint8_t byte) {
  /* A loop rather than a plain switch: whenever a framing/checksum error
     forces a reset back to COMM_FRAME_WAIT_STX, the byte that triggered
     the reset must be re-examined in that fresh state rather than
     discarded - it may itself be the STX of the very next real frame
     (this matters most right after a false start: a stray byte equal to
     COMM_FRAME_STX inside noise makes the decoder legitimately try to
     parse a frame from there, and whichever byte fails that attempt -
     often the real next frame's own STX - must not be silently eaten).
     Each failure path transitions to WAIT_STX before looping, and
     WAIT_STX itself always returns without looping, so this always
     terminates in at most two passes. */
  for (;;) {
    switch (dec->state) {
      case COMM_FRAME_WAIT_STX:
        if (byte == COMM_FRAME_STX) {
          dec->checksum_accum = 0;
          dec->state = COMM_FRAME_WAIT_TYPE;
        }
        /* else: noise before a real frame - stay put and keep discarding */
        return 0;

      case COMM_FRAME_WAIT_TYPE:
        dec->type = byte;
        dec->checksum_accum ^= byte;
        dec->state = COMM_FRAME_WAIT_LEN_HI;
        return 0;

      case COMM_FRAME_WAIT_LEN_HI:
        dec->len = (uint16_t)((uint16_t)byte << 8);
        dec->checksum_accum ^= byte;
        dec->state = COMM_FRAME_WAIT_LEN_LO;
        return 0;

      case COMM_FRAME_WAIT_LEN_LO:
        dec->len = (uint16_t)(dec->len | byte);
        dec->checksum_accum ^= byte;
        dec->received = 0;
        if (dec->len > COMM_FRAME_MAX_PAYLOAD) {
          ResetForNextFrame(dec); /* corrupt/oversized length - resync */
          continue;               /* re-examine this byte as a possible STX */
        }
        dec->state = (dec->len == 0) ? COMM_FRAME_WAIT_CHECKSUM
                                      : COMM_FRAME_WAIT_PAYLOAD;
        return 0;

      case COMM_FRAME_WAIT_PAYLOAD:
        dec->payload[dec->received++] = byte;
        dec->checksum_accum ^= byte;
        if (dec->received >= dec->len) {
          dec->state = COMM_FRAME_WAIT_CHECKSUM;
        }
        return 0;

      case COMM_FRAME_WAIT_CHECKSUM:
        dec->expected_checksum = byte;
        dec->state = COMM_FRAME_WAIT_ETX;
        return 0;

      case COMM_FRAME_WAIT_ETX: {
        uint8_t ok = (byte == COMM_FRAME_ETX) &&
                     (dec->expected_checksum == dec->checksum_accum);
        ResetForNextFrame(dec);
        if (ok) {
          return 1;
        }
        continue; /* re-examine this byte - it may itself be a fresh STX */
      }

      default:
        ResetForNextFrame(dec);
        continue;
    }
  }
}
