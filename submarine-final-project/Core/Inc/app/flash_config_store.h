#ifndef APP_FLASH_CONFIG_STORE_H
#define APP_FLASH_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generic driver for persisting one small fixed-size record to the
   reserved flash page at the top of flash (see CONFIG_FLASH_ADDR in
   flash_config_store.c, and the matching LENGTH reduction in
   STM32L476xx_FLASH.ld). Knows nothing about AppConfig_t's layout -
   app_config.c is the only caller and owns the meaning of the bytes.
   Driver/diagnostic separation: this file does the flash register-level
   work only; app_config.c decides when to load/save and what the
   fallback is when there is nothing valid stored yet. */

/* Reads back a previously-saved record into *data (len bytes). Returns
   1 if a valid record (correct magic + checksum, saved by a prior
   FlashConfigStore_Save call with the same len) was found and copied,
   0 otherwise (blank/erased flash, corrupted record, or len mismatch -
   all treated identically as "nothing usable stored yet"). */
uint8_t FlashConfigStore_Load(void *data, size_t len);

/* Erases the reserved page and writes data (len bytes) wrapped in a
   magic+length+checksum header. Task context only - blocks for the
   duration of a page erase (a few ms) plus one doubleword program per 8
   bytes; do not call from an ISR or with interrupts disabled. Returns 1
   on success, 0 if any flash operation failed (left flash in an
   indeterminate state - caller should treat this as "not persisted",
   not silently succeed). len must not exceed the driver's internal
   buffer size (currently 256 bytes, comfortably larger than AppConfig_t). */
uint8_t FlashConfigStore_Save(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASH_CONFIG_STORE_H */
