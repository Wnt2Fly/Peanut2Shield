#include <esp_log.h>
#include <stdarg.h>
#include <Arduino.h>

// ESP-IDF mirrors logs to USB Serial/JTAG (secondary console). When USB is
// plugged into a PC but no terminal is reading, those writes block inside
// esp_bt_controller_init / NimBLEDevice::init — setup never finishes and the
// LED stays solid yellow. Opening the COM port drains the buffer and boot
// continues. This must run before initArduino() in app_main.
static int peanutNullLog(const char* fmt, va_list args) {
  (void)fmt;
  (void)args;
  return 0;
}

static void peanutSilenceConsole() {
  esp_log_set_vprintf(&peanutNullLog);
  esp_log_level_set("*", ESP_LOG_NONE);
  ets_install_putc1(nullptr);
}

__attribute__((constructor(101))) static void peanutSilenceLogsEarly() {
  peanutSilenceConsole();
}

// Belt-and-suspenders — initArduino() runs esp_log_level_set before initVariant().
void initVariant() {
  peanutSilenceConsole();
  Serial.setTxTimeoutMs(0);
  Serial.setDebugOutput(false);
}
