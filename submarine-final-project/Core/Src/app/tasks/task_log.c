#include "task_log.h"
#include "app_comm.h"
#include "app_log.h"
#include "app_serial.h"
#include "comm_frame.h"
#include "comm_tags.h"
#include "ff.h"
#include "log_query.h"
#include "rtc_ds1307.h"
#include "tlv.h"
#include <stdio.h>
#include <string.h>

/* SW-FD-LNC-001's Log module: one file per calendar day, named by date,
   with 7 days kept on the card - once an 8th day's file would be
   created, the oldest existing one is deleted. Replaces the earlier
   fixed-name LOG.TXT + 512KB-rotation-to-LOG_OLD.TXT scheme, which had
   no notion of calendar days at all. */
#define LOG_RETENTION_DAYS 7
#define LOG_FILENAME_LEN 13 /* "YYYYMMDD.TXT" + NUL */

#define MAX_CONSEC_WRITE_FAILURES 3    /* remount after this many failed writes in a row */
#define REMOUNT_RETRY_INTERVAL_MSGS 20 /* while filesystem is down, retry every N messages */
#define LOG_POLL_STEP_MS 20u /* how often Task_Log checks LogQuery_Poll()
                                 between waiting for new lines to write -
                                 see AppLog_Wait's header comment */

static FATFS s_fs;
static uint8_t s_mkfs_work[4096]; /* f_mkfs's scratch buffer - must be
                                     static, not on this task's stack */

/* Which calendar day the current log file belongs to, and its filename -
   both rebuilt only when the day actually changes (including across a
   reboot), not on every log line. 0 means "not yet known", which forces
   a rebuild (and a retention pass) before the very first write. */
static uint32_t s_current_ymd = 0;
static char s_current_filename[LOG_FILENAME_LEN];

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

static uint32_t DateTimeToYmd(const RTC_DateTime_t *dt) {
  return (uint32_t)dt->year * 10000u + (uint32_t)dt->month * 100u +
         (uint32_t)dt->day;
}

static void BuildFileName(uint32_t ymd, char *out, size_t out_len) {
  snprintf(out, out_len, "%08lu.TXT", (unsigned long)ymd);
}

/* True only for an exact "YYYYMMDD.TXT" 8.3 name (FF_USE_LFN is off in
   this project, so f_readdir always returns plain uppercase short names
   - matching BuildFileName's own output exactly, no case-folding
   needed). Anything else on the card (a differently-named file, a
   directory) is left alone. */
static uint8_t ParseLogFileName(const char *name, uint32_t *out_ymd) {
  if (strlen(name) != 12) {
    return 0;
  }
  for (int i = 0; i < 8; i++) {
    if (name[i] < '0' || name[i] > '9') {
      return 0;
    }
  }
  if (strcmp(name + 8, ".TXT") != 0) {
    return 0;
  }
  uint32_t ymd = 0;
  for (int i = 0; i < 8; i++) {
    ymd = ymd * 10u + (uint32_t)(name[i] - '0');
  }
  *out_ymd = ymd;
  return 1;
}

/* Proleptic-Gregorian day count (Howard Hinnant's civil_from_days /
   days_from_civil algorithm) - turns a YYYYMMDD value into a single
   comparable/subtractable integer so retention is real calendar-day
   arithmetic (handles month/year rollovers, leap years) rather than a
   RAM-only "day N of 7" counter, which would come back wrong after the
   device was powered off across a gap. Only ever used to compare two
   dates, never displayed. */
static int32_t YmdToDayNumber(uint32_t ymd) {
  int32_t year = (int32_t)(ymd / 10000u);
  int32_t month = (int32_t)((ymd / 100u) % 100u);
  int32_t day = (int32_t)(ymd % 100u);
  int32_t y = year - (month <= 2 ? 1 : 0);
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);          /* 0..399 */
  uint32_t mp = (uint32_t)((month + 9) % 12);        /* 0=Mar .. 11=Feb */
  uint32_t doy = (153u * mp + 2u) / 5u + (uint32_t)day - 1u; /* 0..365 */
  uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;   /* 0..146096 */
  return era * 146097 + (int32_t)doe;
}

/* Deletes every date-named log file more than LOG_RETENTION_DAYS-1 days
   older than today, so at most LOG_RETENTION_DAYS files (today's + the
   previous 6) remain. Runs once per day rollover, not on every line. */
static void EnforceRetention(uint32_t today_ymd) {
  DIR dir;
  if (f_opendir(&dir, "/") != FR_OK) {
    return; /* non-fatal - worst case old files linger an extra day */
  }
  int32_t today_days = YmdToDayNumber(today_ymd);
  FILINFO fno;
  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0') {
    if (fno.fattrib & AM_DIR) {
      continue;
    }
    uint32_t file_ymd;
    if (!ParseLogFileName(fno.fname, &file_ymd)) {
      continue;
    }
    if (today_days - YmdToDayNumber(file_ymd) >= LOG_RETENTION_DAYS) {
      f_unlink(fno.fname);
    }
  }
  f_closedir(&dir);
}

/* Refreshes s_current_filename for "today" (from the RTC, or the same
   2026-01-01 fallback get_fattime() uses if the RTC is unavailable) and
   runs retention exactly once when the day actually changes. Returns the
   filename to write to - always valid even if the RTC read failed. */
static const char *CurrentLogFileName(void) {
  RTC_DateTime_t dt;
  uint32_t ymd;
  if (RTC_GetDateTime(&dt) == HAL_OK) {
    ymd = DateTimeToYmd(&dt);
  } else {
    ymd = 20260101u;
  }

  if (ymd != s_current_ymd) {
    BuildFileName(ymd, s_current_filename, sizeof(s_current_filename));
    EnforceRetention(ymd);
    s_current_ymd = ymd;
  }
  return s_current_filename;
}

static uint8_t AppendLine(const char *filename, const char *text) {
  FIL file;
  FRESULT fr = f_open(&file, filename, FA_OPEN_APPEND | FA_WRITE);
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

/* Reads one line (up to '\n' or EOF) from file into out (out_cap bytes,
   NUL-terminated, \r/\n stripped). FF_USE_STRFUNC is off in this
   project (ffconf.h), so f_gets() isn't linkable - this does the same
   job with plain f_read(), one byte at a time. That is slow, but a
   command-triggered, occasional query is not a place worth spending
   complexity on buffered reads. Returns 1 if any byte (including a lone
   \n for a blank line) was consumed - i.e. there was something to read -
   and 0 only at genuine EOF with nothing left, so a blank line in the
   middle of a file can never be mistaken for end-of-file. */
static uint8_t ReadNextLine(FIL *file, char *out, size_t out_cap) {
  size_t len = 0;
  uint8_t consumed_anything = 0;

  for (;;) {
    char c;
    UINT read = 0;
    if (f_read(file, &c, 1, &read) != FR_OK || read == 0) {
      break;
    }
    consumed_anything = 1;
    if (c == '\n') {
      break;
    }
    if (c != '\r' && len + 1 < out_cap) {
      out[len++] = c;
    }
  }

  out[len] = '\0';
  return consumed_anything;
}

/* Sends every line of filename back over the comm link as its own
   MSG_TYPE_DATA_REPORT frame (TAG_LOG_LINE = the raw line text). Missing
   file, empty file, or a mid-read failure just yields fewer lines, not
   an error - the caller (HandleLogQuery) always sends the end-of-results
   marker regardless. */
static void StreamFileLines(const char *filename) {
  FIL file;
  if (f_open(&file, filename, FA_READ) != FR_OK) {
    return;
  }

  char line[APP_LOG_MSG_LEN];
  while (ReadNextLine(&file, line, sizeof(line))) {
    size_t len = strlen(line);
    if (len == 0) {
      continue; /* blank line - nothing to send, but keep reading */
    }
    uint8_t payload[COMM_FRAME_MAX_PAYLOAD];
    size_t offset = 0;
    if (TLV_Encode(payload, sizeof(payload), &offset, TAG_LOG_LINE,
                   (uint16_t)len, (const uint8_t *)line)) {
      AppComm_Post(APP_COMM_PRIO_LOW, MSG_TYPE_DATA_REPORT, payload,
                   (uint16_t)offset);
    }
  }

  f_close(&file);
}

/* GET_DATA and GET_EVENTS (app_command.c) both land here: Task_Event's
   daily log lines are currently the only historical record this system
   keeps (Task_Monitor only reports a mode *change*, not a continuous
   measurement stream - see task_monitor.c), so there is no separate
   "data" log distinct from "events" to tell the two commands apart by
   yet. Both replay the same date-range of log lines; a future Log
   module enhancement that adds a genuinely continuous measurement
   stream would be the place to make GET_DATA return something
   different. Streams oldest-file-first, terminated by one empty (0-byte
   payload) MSG_TYPE_DATA_REPORT frame marking "no more results". */
static void HandleLogQuery(const LogQueryRequest_t *req) {
  uint32_t ymds[LOG_RETENTION_DAYS];
  int count = 0;

  DIR dir;
  if (f_opendir(&dir, "/") == FR_OK) {
    FILINFO fno;
    while (count < LOG_RETENTION_DAYS && f_readdir(&dir, &fno) == FR_OK &&
           fno.fname[0] != '\0') {
      if (fno.fattrib & AM_DIR) {
        continue;
      }
      uint32_t ymd;
      if (!ParseLogFileName(fno.fname, &ymd)) {
        continue;
      }
      if (ymd < req->start_ymd || ymd > req->end_ymd) {
        continue;
      }
      ymds[count++] = ymd;
    }
    f_closedir(&dir);
  }

  /* Insertion sort ascending (oldest first) - count is at most
     LOG_RETENTION_DAYS (7), so this is plenty fast as-is. */
  for (int i = 1; i < count; i++) {
    uint32_t key = ymds[i];
    int j = i - 1;
    while (j >= 0 && ymds[j] > key) {
      ymds[j + 1] = ymds[j];
      j--;
    }
    ymds[j + 1] = key;
  }

  for (int i = 0; i < count; i++) {
    char filename[LOG_FILENAME_LEN];
    BuildFileName(ymds[i], filename, sizeof(filename));
    StreamFileLines(filename);
  }

  AppComm_Post(APP_COMM_PRIO_LOW, MSG_TYPE_DATA_REPORT, NULL, 0);
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
    LogQueryRequest_t query;
    if (LogQuery_Poll(&query)) {
      /* Serviced regardless of fs_ready - HandleLogQuery degrades to "0
         results, here's the end marker" on its own if the card is down,
         which is a better outcome than leaving the requester waiting
         forever for a reply that will never come. */
      HandleLogQuery(&query);
      continue;
    }

    if (AppLog_Wait(&msg, LOG_POLL_STEP_MS) != osOK) {
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
      const char *filename = CurrentLogFileName();
      uint8_t ok = AppendLine(filename, msg.text);
      if (ok) {
        consec_failures = 0;
      } else if (++consec_failures >= MAX_CONSEC_WRITE_FAILURES) {
        Serial_Print("[LOG] repeated write failures - remounting\r\n");
        fs_ready = Remount();
        consec_failures = 0;
        msgs_since_remount_attempt = 0;
        if (fs_ready) {
          filename = CurrentLogFileName(); /* re-derive post-remount */
          ok = AppendLine(filename, msg.text); /* retry this one message */
        }
      }
      len = snprintf(line, sizeof(line), "[LOG] %s to %s: %s\r\n",
                     ok ? "written" : "WRITE FAILED", filename, msg.text);
    } else {
      len = snprintf(line, sizeof(line), "[LOG] %s\r\n", msg.text);
    }

    if (len > 0) {
      Serial_Print(line);
    }
  }
}
