#ifndef APP_SDCARD_H
#define APP_SDCARD_H

#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SD_BLOCK_SIZE 512u

typedef enum {
  SD_TYPE_UNKNOWN = 0,
  SD_TYPE_MMC,
  SD_TYPE_SD1,      /* SD v1 (SDSC only, no CMD8 support) */
  SD_TYPE_SD2_SDSC, /* SD v2, standard capacity, byte addressing */
  SD_TYPE_SD2_SDHC, /* SD v2, high/extended capacity, block addressing */
} SD_CardType_t;

/* Full SPI-mode init sequence (CMD0/CMD8/ACMD41/CMD58). Runs at <=400kHz
   as the spec requires, then raises SPI1's clock for normal operation.
   Must succeed before SD_ReadBlock/SD_WriteBlock are used. */
HAL_StatusTypeDef SD_Init(void);

typedef enum {
  SD_INIT_STAGE_NONE = 0, /* no failure recorded (last init succeeded, or
                             none attempted yet) */
  SD_INIT_STAGE_CMD0,     /* card didn't answer GO_IDLE_STATE at all -
                             usually means nothing is wired/powered */
  SD_INIT_STAGE_CMD8,
  SD_INIT_STAGE_ACMD41,
  SD_INIT_STAGE_CMD58,
} SD_InitStage_t;

/* Which step SD_Init() failed at last time, plus the raw R1 byte seen
   there - lets a diagnostic layer report something more useful than
   just HAL_ERROR/HAL_TIMEOUT. */
void SD_GetLastError(SD_InitStage_t *stage, uint8_t *r1);

SD_CardType_t SD_GetCardType(void);

/* lba is a block/sector number (not a byte address) - SD_Init() already
   accounts for SDSC (byte-addressed) vs SDHC/SDXC (block-addressed)
   cards internally, so callers always pass a plain block index either
   way. buf must be exactly SD_BLOCK_SIZE bytes. */
HAL_StatusTypeDef SD_ReadBlock(uint32_t lba, uint8_t *buf);
HAL_StatusTypeDef SD_WriteBlock(uint32_t lba, const uint8_t *buf);

/* Reads and parses the card's CSD register (CMD9) to get its real
   capacity in 512-byte sectors - needed before formatting or writing
   near the card's actual limit, rather than assuming a size. */
HAL_StatusTypeDef SD_GetSectorCount(uint32_t *out_sectors);

#ifdef __cplusplus
}
#endif

#endif /* APP_SDCARD_H */
