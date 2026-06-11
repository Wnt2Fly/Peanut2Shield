#pragma once
#include <Arduino.h>

// Gated until setup() finishes — avoids USB Serial/JTAG blocking boot.
extern bool gDevLogReady;

inline bool devLogConnected() {
  return gDevLogReady && (bool)Serial;
}

#define DEV_LOGF(...) do { if (devLogConnected()) Serial.printf(__VA_ARGS__); } while (0)
#define DEV_LOGLN(s)  do { if (devLogConnected()) Serial.println(s); } while (0)
#define DEV_LOG(s)    do { if (devLogConnected()) Serial.print(s); } while (0)
