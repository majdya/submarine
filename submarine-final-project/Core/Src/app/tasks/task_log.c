#include "task_log.h"
#include "app_log.h"
#include "app_serial.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define LOG_FILE_NAME "LOG.TXT"

static FATFS s_fs;
static uint8_t s_mkfs_work[4096]; /* f_mkfs's scratch buffer - must be
                                     static, not on this task's stack */

static uint8_t MountOrFormat(void) {
  FRESULT fr = f_mount(&s_fs, "", 1);
  if (fr == FR_OK) {
    return 1;
  }
  if (fr != FR_NO_FILESYSTEM) {
    char line[64];
    int len = snprintf(line, sizeof(line), "[LOG] f_mount failed, fr=%d\r\n",
                       (int)fr);
    if (len > 0) {
      Serial_Print(line);
    }
    return 0;
  }

  /* Card has no FAT filesystem on it yet - true here since this card was
     only ever written with the earlier raw-block test, never formatted.
     Formatting is destructive by nature; this only runs when there is no
     filesystem to begin with, and it's reported below rather than done
     silently. */
  Serial_Print(
      "[LOG] SD card has no filesystem - formatting it now (FAT)\r\n");
  MKFS_PARM opt = {.fmt = FM_ANY};
  fr = f_mkfs("", &opt, s_mkfs_work, sizeof(s_mkfs_work));
  if (fr != FR_OK) {
    char line[64];
    int len = snprintf(line, sizeof(line), "[LOG] f_mkfs failed, fr=%d\r\n",
                       (int)fr);
    if (len > 0) {
      Serial_Print(line);
    }
    return 0;
  }

  fr = f_mount(&s_fs, "", 1);
  if (fr != FR_OK) {
    char line[64];
    int len = snprintf(line, sizeof(line),
                       "[LOG] f_mount after format failed, fr=%d\r\n",
                       (int)fr);
    if (len > 0) {
      Serial_Print(line);
    }
    return 0;
  }
  Serial_Print("[LOG] format + mount OK\r\n");
  return 1;
}

static uint8_t AppendLine(const char *text) {
  FIL file;
  FRESULT fr = f_open(&file, LOG_FILE_NAME, FA_OPEN_APPEND | FA_WRITE);
  if (fr != FR_OK) {
    return 0;
  }

  UINT written;
  char line[APP_LOG_MSG_LEN + 4];
  int len = snprintf(line, sizeof(line), "%s\r\n", text);
  uint8_t ok = 0;
  if (len > 0) {
    fr = f_write(&file, line, (UINT)len, &written);
    ok = (fr == FR_OK && written == (UINT)len);
  }
  f_close(&file);
  return ok;
}

void Task_Log(void *argument) {
  (void)argument;
  AppLogMsg_t msg;

  uint8_t fs_ready = MountOrFormat();
  {
    char line[64];
    int len = snprintf(line, sizeof(line), "[LOG] filesystem %s\r\n",
                       fs_ready ? "ready" : "unavailable - UART only");
    if (len > 0) {
      Serial_Print(line);
    }
  }

  for (;;) {
    if (AppLog_Wait(&msg) != osOK) {
      continue;
    }

    char line[APP_LOG_MSG_LEN + 32];
    int len;

    if (fs_ready) {
      uint8_t ok = AppendLine(msg.text);
      len = snprintf(line, sizeof(line), "[LOG] %s to %s: %s\r\n",
                     ok ? "written" : "WRITE FAILED", LOG_FILE_NAME,
                     msg.text);
    } else {
      len = snprintf(line, sizeof(line), "[LOG] %s\r\n", msg.text);
    }

    if (len > 0) {
      Serial_Print(line);
    }
  }
}
