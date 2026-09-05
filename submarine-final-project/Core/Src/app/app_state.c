#include "app_state.h"
#include <string.h>

static AppState_t s_state;
static osMutexId_t s_state_mutex;
static const osMutexAttr_t s_state_mutex_attr = {
    .name = "AppStateMutex",
};

void AppState_Init(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_state.system_healthy = 0; /* Watchdog stays off until Init proves the app is up */
  s_state_mutex = osMutexNew(&s_state_mutex_attr);
}

void AppState_SetSensorReadings(const AppState_t *readings) {
  if (osMutexAcquire(s_state_mutex, osWaitForever) != osOK) {
    return;
  }
  /* Preserve system_healthy - callers pass a readings-only snapshot and
     should not be able to accidentally clear health state through this
     call. */
  uint8_t healthy = s_state.system_healthy;
  s_state = *readings;
  s_state.system_healthy = healthy;
  osMutexRelease(s_state_mutex);
}

void AppState_Get(AppState_t *out) {
  if (osMutexAcquire(s_state_mutex, osWaitForever) != osOK) {
    memset(out, 0, sizeof(*out));
    return;
  }
  *out = s_state;
  osMutexRelease(s_state_mutex);
}

void AppState_SetHealthy(uint8_t healthy) {
  if (osMutexAcquire(s_state_mutex, osWaitForever) != osOK) {
    return;
  }
  s_state.system_healthy = healthy;
  osMutexRelease(s_state_mutex);
}

uint8_t AppState_IsHealthy(void) {
  uint8_t healthy;
  if (osMutexAcquire(s_state_mutex, osWaitForever) != osOK) {
    return 0;
  }
  healthy = s_state.system_healthy;
  osMutexRelease(s_state_mutex);
  return healthy;
}
