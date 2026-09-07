#include "flash_config_store.h"
#include "main.h"
#include <string.h>

/* The last 2K page of this device's 1MB flash - STM32L476xx uses 2KB
   pages (FLASH_PAGE_SIZE == 0x800, per stm32l4xx_hal_flash.h) split into
   two 512KB banks. STM32L476xx_FLASH.ld's FLASH region LENGTH is
   shortened to 1022K specifically so nothing else can ever be linked
   into this page. */
#define CONFIG_FLASH_ADDR 0x080FF800UL

#define CONFIG_MAGIC 0x53554243UL /* 'S','U','B','C' - arbitrary but fixed */
#define CONFIG_HEADER_LEN 12u     /* magic(4) + len(4) + checksum(4) */
#define CONFIG_STORE_MAX_LEN 256u /* generous headroom over AppConfig_t */

/* FNV-1a, 32-bit - simple and effective for detecting a torn/blank/
   corrupted record; this is not a security checksum, just a "was this
   actually written by FlashConfigStore_Save" sanity check. */
static uint32_t Fnv1a(const uint8_t *data, size_t len) {
  uint32_t hash = 0x811C9DC5u;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

/* Page/bank lookup for HAL_FLASHEx_Erase. Unlike the larger L4+ Cat.5/6
   parts, STM32L476xx has no DBANK option byte - any L4 Cat.3/Cat.4
   device with >= 512KB of flash (this one has 1MB) is unconditionally
   organized as two fixed 512KB banks of 256 x 2KB pages each, so which
   bank an address falls in is a plain compile-time-known address
   comparison, not something to read out of FLASH->OPTR (confirmed
   against the vendored CMSIS header for this part: it defines no
   FLASH_OPTR_DBANK bit at all). Page numbers are bank-relative. */
static uint32_t GetBank(uint32_t addr) {
  return (addr < (FLASH_BASE + FLASH_BANK_SIZE)) ? FLASH_BANK_1 : FLASH_BANK_2;
}

static uint32_t GetPage(uint32_t addr) {
  if (addr < (FLASH_BASE + FLASH_BANK_SIZE)) {
    return (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
  }
  return (addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
}

uint8_t FlashConfigStore_Load(void *data, size_t len) {
  if (!data || len == 0 || (CONFIG_HEADER_LEN + len) > CONFIG_STORE_MAX_LEN) {
    return 0;
  }

  const uint8_t *flash = (const uint8_t *)CONFIG_FLASH_ADDR;
  uint32_t magic;
  uint32_t stored_len;
  uint32_t stored_checksum;
  memcpy(&magic, flash, 4);
  memcpy(&stored_len, flash + 4, 4);
  memcpy(&stored_checksum, flash + 8, 4);

  if (magic != CONFIG_MAGIC || stored_len != (uint32_t)len) {
    return 0; /* blank/erased flash reads as all-0xFF, or a record saved
                 for a different AppConfig_t layout - either way, nothing
                 usable */
  }

  const uint8_t *payload = flash + CONFIG_HEADER_LEN;
  if (Fnv1a(payload, len) != stored_checksum) {
    return 0; /* corrupted/torn write */
  }

  memcpy(data, payload, len);
  return 1;
}

uint8_t FlashConfigStore_Save(const void *data, size_t len) {
  if (!data || len == 0 || (CONFIG_HEADER_LEN + len) > CONFIG_STORE_MAX_LEN) {
    return 0;
  }

  uint8_t buf[CONFIG_STORE_MAX_LEN];
  memset(buf, 0, sizeof(buf)); /* zero-fills the doubleword padding tail too */

  uint32_t magic = CONFIG_MAGIC;
  uint32_t len32 = (uint32_t)len;
  uint32_t checksum = Fnv1a((const uint8_t *)data, len);
  memcpy(buf, &magic, 4);
  memcpy(buf + 4, &len32, 4);
  memcpy(buf + 8, &checksum, 4);
  memcpy(buf + CONFIG_HEADER_LEN, data, len);

  size_t record_len = CONFIG_HEADER_LEN + len;
  size_t padded_len = (record_len + 7u) & ~((size_t)7u); /* doubleword-align */

  uint8_t ok = 1;

  if (HAL_FLASH_Unlock() != HAL_OK) {
    return 0;
  }

  FLASH_EraseInitTypeDef erase = {0};
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks = GetBank(CONFIG_FLASH_ADDR);
  erase.Page = GetPage(CONFIG_FLASH_ADDR);
  erase.NbPages = 1;
  uint32_t page_error = 0xFFFFFFFFu;

  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK || page_error != 0xFFFFFFFFu) {
    ok = 0;
  }

  if (ok) {
    for (size_t offset = 0; offset < padded_len; offset += 8u) {
      uint64_t dword;
      memcpy(&dword, buf + offset, 8);
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                             CONFIG_FLASH_ADDR + offset, dword) != HAL_OK) {
        ok = 0;
        break;
      }
    }
  }

  HAL_FLASH_Lock();

  if (!ok) {
    return 0;
  }

  /* Read back and verify rather than trusting HAL_OK alone - flash wear
     or a marginal supply rail can produce a bit that reads back wrong
     even when the HAL call itself reported success. */
  uint8_t verify[CONFIG_STORE_MAX_LEN - CONFIG_HEADER_LEN];
  return FlashConfigStore_Load(verify, len) && memcmp(verify, data, len) == 0;
}
