#include "task_log.h"
#include "app_log.h"
#include "app_serial.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define LOG_FILE_NAME "LOG.TXT"
#define LOG_FILE_OLD_NAME "LOG_OLD.TXT"
#define LOG_FILE_MAX_BYTES (512UL * 1024UL) /* rotate once LOG.TXT reaches this size */

#define MAX_CONSEC_WRITE_FAILURES 3    /* remount after this many failed writes in a row */
#define REMOUNT_RETRY_INTERVAL_MSGS 20 /* while filesystem is down, retry every N messages */

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

  /* No FAT filesystem on the card. Formatting is destructive by nature;
     this only runs when there is genuinely no filesystem to begin with,
     and it's reported below rather than done silently. */
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

/* Unmounts (if mounted) and re-runs MountOrFormat. Used both to recover
   from a run of write failures (e.g. a card that dropped out and came
   back) and to periodically retry while the filesystem is known down
   (e.g. card was reinserted after being pulled). f_mount's own return
   value on the unmount call is irrelevant here - we're about to try a
   fresh mount regardless of whether anything was mounted before. */
static uint8_t Remount(void) {
  f_mount(NULL, "", 0);
  return MountOrFormat();
}

/* Keeps LOG.TXT from growing without bound: once it reaches the size cap,
   the previous rotation (if any) is dropped and the current file becomes
   LOG_OLD.TXT, so at most ~2x the cap is ever on the card for this log.
   Failures here are non-fatal - if rotation can't happen for some reason,
   AppendLine still tries to write to (an oversized) LOG.TXT rather than
   losing the message. */
static void RotateIfNeeded(void) {
  FILINFO fno;
  if (f_stat(LOG_FILE_NAME, &fno) != FR_OK) {
    return; /* doesn't exist yet (or a stat error) - nothing to rotate */
  }
  if (fno.fsize < LOG_FILE_MAX_BYTES) {
    return;
  }
  f_unlink(LOG_FILE_OLD_NAME); /* fine if it doesn't exist - result unchecked */
  if (f_rename(LOG_FILE_NAME, LOG_FILE_OLD_NAME) == FR_OK) {
    Serial_Print("[LOG] LOG.TXT reached size cap - rotated to LOG_OLD.TXT\r\n");
  }
}

static uint8_t AppendLine(const char *text) {
  RotateIfNeeded();

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

  uint32_t consec_failures = 0;
  uint32_t msgs_since_remount_attempt = 0;

  for (;;) {
    if (AppLog_Wait(&msg) != osOK) {
      continue;
    }

    if (!fs_ready) {
      /* Filesystem is down - don't hammer the card on every message, but
         periodically check whether it has come back (e.g. reinserted). */
      if (++msgs_since_remount_attempt >= REMOUNT_RETRY_INTERVAL_MSGS) {
        msgs_since_remount_attempt = 0;
        Serial_Print("[LOG] retrying filesystem mount...\r\n");
        fs_ready = Remount();
        if (fs_ready) {
          Serial_Print("[LOG] filesystem recovered\r\n");
          consec_failures = 0;
        }
      }
    }

    char line[APP_LOG_MSG_LEN + 32];
    int len;

    if (fs_ready) {
      uint8_t ok = AppendLine(msg.text);
      if (ok) {
        consec_failures = 0;
      } else if (++consec_failures >= MAX_CONSEC_WRITE_FAILURES) {
        Serial_Print("[LOG] repeated write failures - remounting\r\n");
        fs_ready = Remount();
        consec_failures = 0;
        msgs_since_remount_attempt = 0;
        if (fs_ready) {
          ok = AppendLine(msg.text); /* retry this one message after remount */
        }
      }
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
