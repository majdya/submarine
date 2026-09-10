/*-----------------------------------------------------------------------/
/ Low level disk I/O module glue for FatFs - NOT vendored from ChaN's
/ FatFs distribution. This implements the 5 functions diskio.h declares,
/ calling this project's own sdcard.c driver (Core/Src/app/sdcard.c).
/ Kept separate from that driver per this project's driver/diagnostic
/ separation standard: this file is glue between two layers, not a
/ diagnostic/print layer, so it still does no printing of its own.
/-----------------------------------------------------------------------*/

#include "ff.h"
#include "diskio.h"
#include "rtc_ds1307.h"
#include "sdcard.h"

static uint8_t s_status = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv) {
  if (pdrv != 0) {
    return STA_NOINIT;
  }
  return s_status;
}

DSTATUS disk_initialize(BYTE pdrv) {
  if (pdrv != 0) {
    return STA_NOINIT;
  }
  s_status = (SD_Init() == HAL_OK) ? 0 : STA_NOINIT;
  return s_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
  if (pdrv != 0 || s_status == STA_NOINIT) {
    return RES_NOTRDY;
  }
  for (UINT i = 0; i < count; i++) {
    if (SD_ReadBlock((uint32_t)sector + i, buff + (i * SD_BLOCK_SIZE)) !=
        HAL_OK) {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
  if (pdrv != 0 || s_status == STA_NOINIT) {
    return RES_NOTRDY;
  }
  for (UINT i = 0; i < count; i++) {
    if (SD_WriteBlock((uint32_t)sector + i, buff + (i * SD_BLOCK_SIZE)) !=
        HAL_OK) {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
  if (pdrv != 0) {
    return RES_PARERR;
  }
  switch (cmd) {
    case CTRL_SYNC:
      /* Every SD_WriteBlock() call already waits for the card's busy
         signal to clear before returning - there is no write cache in
         this driver to flush. */
      return RES_OK;

    case GET_SECTOR_SIZE:
      *(WORD *)buff = SD_BLOCK_SIZE;
      return RES_OK;

    case GET_SECTOR_COUNT: {
      uint32_t sectors;
      if (SD_GetSectorCount(&sectors) != HAL_OK) {
        return RES_ERROR;
      }
      *(LBA_t *)buff = sectors;
      return RES_OK;
    }

    case GET_BLOCK_SIZE:
      /* Real erase-block (AU) size needs the SD Status register
         (ACMD13), which this driver doesn't implement - 1 tells FatFs
         "unknown", which it handles by using a safe default alignment
         rather than a wrong one. */
      *(DWORD *)buff = 1;
      return RES_OK;

    default:
      return RES_PARERR;
  }
}

/* FatFs calls this (FF_FS_NORTC == 0) to timestamp file create/write
   operations. Packed per FatFs's format: bit31:25 year-1980, bit24:21
   month, bit20:16 day, bit15:11 hour, bit10:5 minute, bit4:0 second/2.
   Reads the DS1307 fresh each call - FatFs only calls this on f_open/
   f_write/f_mkdir, never in a hot loop, so the extra I2C transaction is
   fine. Falls back to a fixed placeholder date if the read fails, rather
   than returning garbage or blocking. */
DWORD get_fattime(void) {
  RTC_DateTime_t dt;
  if (RTC_GetDateTime(&dt) != HAL_OK || dt.year < 1980) {
    dt.year = 2026;
    dt.month = 1;
    dt.day = 1;
    dt.hour = 0;
    dt.min = 0;
    dt.sec = 0;
  }
  return ((DWORD)(dt.year - 1980) << 25) | ((DWORD)dt.month << 21) |
         ((DWORD)dt.day << 16) | ((DWORD)dt.hour << 11) |
         ((DWORD)dt.min << 5) | ((DWORD)(dt.sec / 2));
}
