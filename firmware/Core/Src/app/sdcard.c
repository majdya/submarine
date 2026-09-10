#include "sdcard.h"
#include "main.h"

#define SD_CMD0 0
#define SD_CMD8 8
#define SD_CMD9 9
#define SD_CMD12 12
#define SD_CMD17 17
#define SD_CMD24 24
#define SD_CMD55 55
#define SD_CMD58 58
#define SD_ACMD41 41

#define SD_R1_IDLE_STATE 0x01u
#define SD_R1_ILLEGAL_CMD 0x04u
#define SD_DATA_TOKEN 0xFEu
#define SD_INIT_TIMEOUT_MS 1000u
#define SD_READY_TIMEOUT_MS 500u

static SD_CardType_t s_card_type = SD_TYPE_UNKNOWN;
static uint8_t s_initialized = 0;
static SD_InitStage_t s_last_fail_stage = SD_INIT_STAGE_NONE;
static uint8_t s_last_r1 = 0;

static void SD_CS_Select(void) {
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}

static void SD_CS_Deselect(void) {
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
}

static uint8_t SD_Xchg(uint8_t out) {
  uint8_t in = 0xFF;
  HAL_SPI_TransmitReceive(&hspi1, &out, &in, 1, HAL_MAX_DELAY);
  return in;
}

/* SPI1's baud rate is set once at MX_SPI1_Init() time and HAL doesn't
   expose a clean "change it live" API without a full re-init (which
   would also touch GPIO unnecessarily) - toggling SPE around a direct
   CR1 BR[2:0] write is the standard, documented way to change it at
   runtime. SD cards require <=400kHz until they leave idle state, then
   can run much faster - this is why the prescaler has to change between
   SD_Init() and normal block I/O.

   This is SPI1 (PA5/6/7 + PB6 CS) because the SD/RTC data-logger shield
   hard-wires its SD slot to Arduino D10-D13 - it cannot be moved to a
   a different SPI peripheral - it just changes SPI1's own prescaler.
   Those same pins overlap the other stacked shield's RGB LED - see the
   note on MX_SPI1_Init() in main.c. */
static void SD_SetPrescaler(uint32_t prescaler) {
  __HAL_SPI_DISABLE(&hspi1);
  MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, prescaler);
  __HAL_SPI_ENABLE(&hspi1);
}

static uint8_t SD_WaitReady(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();
  uint8_t resp;
  do {
    resp = SD_Xchg(0xFF);
  } while (resp != 0xFF && (HAL_GetTick() - start) < timeout_ms);
  return resp;
}

/* Sends a command frame, returns the R1 response byte. For commands with
   a longer reply (R3/R7 - CMD8, CMD58), the caller reads the trailing
   bytes with SD_Xchg(0xFF) right after this returns. */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
  if (cmd != SD_CMD0 && cmd != SD_CMD12) {
    if (SD_WaitReady(SD_READY_TIMEOUT_MS) != 0xFF) {
      return 0xFF;
    }
  }

  uint8_t frame[6] = {
      (uint8_t)(0x40u | cmd), (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
      (uint8_t)(arg >> 8),    (uint8_t)(arg),        crc,
  };
  for (int i = 0; i < 6; i++) {
    SD_Xchg(frame[i]);
  }

  uint8_t resp = 0xFF;
  for (int i = 0; i < 10; i++) {
    resp = SD_Xchg(0xFF);
    if ((resp & 0x80u) == 0u) { /* R1's MSB is always 0 */
      break;
    }
  }
  return resp;
}

static uint8_t SD_SendAppCmd(uint8_t acmd, uint32_t arg) {
  SD_SendCmd(SD_CMD55, 0, 0x01);
  return SD_SendCmd(acmd, arg, 0x01);
}

HAL_StatusTypeDef SD_Init(void) {
  s_initialized = 0;
  s_card_type = SD_TYPE_UNKNOWN;

  /* Init must happen at <=400kHz per spec until the card leaves idle
     state - APB1 is 80MHz here, DIV256 = 312.5kHz. */
  SD_SetPrescaler(SPI_BAUDRATEPRESCALER_256);

  SD_CS_Deselect();
  /* >=74 clock cycles with CS high and MOSI high before the first
     command - lets the card's internal circuitry finish power-up. */
  for (int i = 0; i < 10; i++) {
    SD_Xchg(0xFF);
  }

  SD_CS_Select();
  uint8_t r1 = SD_SendCmd(SD_CMD0, 0, 0x95);
  if (r1 != SD_R1_IDLE_STATE) {
    s_last_fail_stage = SD_INIT_STAGE_CMD0;
    s_last_r1 = r1;
    SD_CS_Deselect();
    return HAL_ERROR; /* card didn't respond / isn't in SPI mode */
  }

  uint8_t is_v2 = 0;
  r1 = SD_SendCmd(SD_CMD8, 0x1AA, 0x87);
  if (r1 == SD_R1_IDLE_STATE) {
    is_v2 = 1;
    uint8_t r7[4];
    for (int i = 0; i < 4; i++) {
      r7[i] = SD_Xchg(0xFF);
    }
    if (r7[2] != 0x01 || r7[3] != 0xAA) {
      s_last_fail_stage = SD_INIT_STAGE_CMD8;
      s_last_r1 = r1;
      SD_CS_Deselect();
      return HAL_ERROR; /* voltage range not supported */
    }
  } else if (r1 & SD_R1_ILLEGAL_CMD) {
    is_v2 = 0; /* SD v1 or MMC - no CMD8 support */
  } else {
    s_last_fail_stage = SD_INIT_STAGE_CMD8;
    s_last_r1 = r1;
    SD_CS_Deselect();
    return HAL_ERROR;
  }

  uint32_t start = HAL_GetTick();
  uint32_t acmd41_arg = is_v2 ? 0x40000000u : 0x00000000u; /* HCS bit */
  for (;;) {
    r1 = SD_SendAppCmd(SD_ACMD41, acmd41_arg);
    if (r1 == 0x00) {
      break;
    }
    if ((HAL_GetTick() - start) > SD_INIT_TIMEOUT_MS) {
      s_last_fail_stage = SD_INIT_STAGE_ACMD41;
      s_last_r1 = r1;
      SD_CS_Deselect();
      return HAL_TIMEOUT;
    }
  }

  if (is_v2) {
    r1 = SD_SendCmd(SD_CMD58, 0, 0x01);
    if (r1 != 0x00) {
      s_last_fail_stage = SD_INIT_STAGE_CMD58;
      s_last_r1 = r1;
      SD_CS_Deselect();
      return HAL_ERROR;
    }
    uint8_t ocr[4];
    for (int i = 0; i < 4; i++) {
      ocr[i] = SD_Xchg(0xFF);
    }
    s_card_type = (ocr[0] & 0x40u) ? SD_TYPE_SD2_SDHC : SD_TYPE_SD2_SDSC;
  } else {
    s_card_type = SD_TYPE_SD1;
  }

  SD_CS_Deselect();
  SD_Xchg(0xFF);

  /* Init done - safe to run faster now. DIV4 (20MHz off 80MHz APB1) sits
     comfortably inside typical SD SPI-mode limits, unlike the
     DIV2/40MHz this project used for the earlier raw-clocking-only
     test. */
  SD_SetPrescaler(SPI_BAUDRATEPRESCALER_4);

  s_initialized = 1;
  s_last_fail_stage = SD_INIT_STAGE_NONE;
  return HAL_OK;
}

SD_CardType_t SD_GetCardType(void) { return s_card_type; }

void SD_GetLastError(SD_InitStage_t *stage, uint8_t *r1) {
  *stage = s_last_fail_stage;
  *r1 = s_last_r1;
}

static HAL_StatusTypeDef SD_BlockAddress(uint32_t lba, uint32_t *out_addr) {
  if (!s_initialized) {
    return HAL_ERROR;
  }
  /* SDHC/SDXC cards address in blocks directly; SDSC cards (and SD1) are
     byte-addressed, so the block index must be multiplied by the block
     size. Callers always pass a plain block index either way. */
  *out_addr =
      (s_card_type == SD_TYPE_SD2_SDHC) ? lba : (lba * SD_BLOCK_SIZE);
  return HAL_OK;
}

HAL_StatusTypeDef SD_ReadBlock(uint32_t lba, uint8_t *buf) {
  uint32_t addr;
  if (SD_BlockAddress(lba, &addr) != HAL_OK) {
    return HAL_ERROR;
  }

  SD_CS_Select();
  HAL_StatusTypeDef status = HAL_ERROR;
  uint8_t r1 = SD_SendCmd(SD_CMD17, addr, 0x01);
  if (r1 == 0x00) {
    uint8_t token;
    uint32_t start = HAL_GetTick();
    do {
      token = SD_Xchg(0xFF);
    } while (token == 0xFF && (HAL_GetTick() - start) < SD_READY_TIMEOUT_MS);

    if (token == SD_DATA_TOKEN) {
      for (uint32_t i = 0; i < SD_BLOCK_SIZE; i++) {
        buf[i] = SD_Xchg(0xFF);
      }
      SD_Xchg(0xFF); /* CRC bytes - ignored, CRC checking is off by default
                         in SPI mode */
      SD_Xchg(0xFF);
      status = HAL_OK;
    }
  }
  SD_CS_Deselect();
  SD_Xchg(0xFF);
  return status;
}

HAL_StatusTypeDef SD_WriteBlock(uint32_t lba, const uint8_t *buf) {
  uint32_t addr;
  if (SD_BlockAddress(lba, &addr) != HAL_OK) {
    return HAL_ERROR;
  }

  SD_CS_Select();
  HAL_StatusTypeDef status = HAL_ERROR;
  uint8_t r1 = SD_SendCmd(SD_CMD24, addr, 0x01);
  if (r1 == 0x00) {
    SD_Xchg(SD_DATA_TOKEN);
    for (uint32_t i = 0; i < SD_BLOCK_SIZE; i++) {
      SD_Xchg(buf[i]);
    }
    SD_Xchg(0xFF); /* dummy CRC - not checked with CRC off */
    SD_Xchg(0xFF);

    uint8_t data_resp = SD_Xchg(0xFF);
    if ((data_resp & 0x1Fu) == 0x05u) { /* data accepted */
      if (SD_WaitReady(SD_READY_TIMEOUT_MS) == 0xFF) {
        status = HAL_OK;
      }
    }
  }
  SD_CS_Deselect();
  SD_Xchg(0xFF);
  return status;
}

/* CMD9 (SEND_CSD) returns the 16-byte CSD register using the same
   start-token protocol as a normal block read. Bit positions and the
   capacity formulas below are the documented SD CSD v1.0/v2.0 layout
   (SD Physical Layer Simplified Spec), not guesswork - CSD structure
   version is bit 127 downward, i.e. the top 2 bits of csd[0]. */
HAL_StatusTypeDef SD_GetSectorCount(uint32_t *out_sectors) {
  if (!s_initialized) {
    return HAL_ERROR;
  }

  uint8_t csd[16];
  SD_CS_Select();
  HAL_StatusTypeDef status = HAL_ERROR;
  uint8_t r1 = SD_SendCmd(SD_CMD9, 0, 0x01);
  if (r1 == 0x00) {
    uint8_t token;
    uint32_t start = HAL_GetTick();
    do {
      token = SD_Xchg(0xFF);
    } while (token == 0xFF && (HAL_GetTick() - start) < SD_READY_TIMEOUT_MS);

    if (token == SD_DATA_TOKEN) {
      for (int i = 0; i < 16; i++) {
        csd[i] = SD_Xchg(0xFF);
      }
      SD_Xchg(0xFF); /* CRC, ignored */
      SD_Xchg(0xFF);
      status = HAL_OK;
    }
  }
  SD_CS_Deselect();
  SD_Xchg(0xFF);

  if (status != HAL_OK) {
    return status;
  }

  uint32_t csize, sectors;
  if ((csd[0] >> 6) == 1) {
    /* CSD version 2.0 - SDHC/SDXC: C_SIZE is bits [69:48] */
    csize = ((uint32_t)(csd[7] & 0x3Fu) << 16) | ((uint32_t)csd[8] << 8) |
            csd[9];
    sectors = (csize + 1u) * 1024u; /* (C_SIZE+1) * 512KB, in 512B sectors */
  } else {
    /* CSD version 1.0 - SDSC: C_SIZE [73:62], C_SIZE_MULT [49:47],
       READ_BL_LEN [83:80] */
    csize = ((uint32_t)(csd[6] & 0x03u) << 10) | ((uint32_t)csd[7] << 2) |
            (csd[8] >> 6);
    uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03u) << 1) | (csd[10] >> 7);
    uint32_t read_bl_len = csd[5] & 0x0Fu;
    uint32_t shift = read_bl_len + c_size_mult + 2u - 9u;
    sectors = (csize + 1u) << shift;
  }

  *out_sectors = sectors;
  return HAL_OK;
}
