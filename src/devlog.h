#pragma once
#include <Arduino.h>

// Gated until setup() finishes — avoids USB Serial/JTAG blocking boot.
extern bool gDevLogReady;

// HWCDC (bool)Serial is "host has drained TX" — often false even with a monitor
// open, which hid all logs. With setTxTimeoutMs(0), writes are non-blocking, so
// only require that setup enabled logging.
inline bool devLogConnected() {
  return gDevLogReady;
}

#define DEV_LOGF(...) do { if (devLogConnected()) Serial.printf(__VA_ARGS__); } while (0)
#define DEV_LOGLN(s)  do { if (devLogConnected()) Serial.println(s); } while (0)
#define DEV_LOG(s)    do { if (devLogConnected()) Serial.print(s); } while (0)
