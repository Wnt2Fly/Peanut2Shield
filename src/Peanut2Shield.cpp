#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_timer.h>
#include "config.h"
#include "devlog.h"
#include "keymap.h"
#include "hid_peripheral.h"

// ---- WS2812 RGB LED (single pixel on GPIO CFG_LED_PIN) ----
// Waveshare ESP32-S3-Zero uses RGB channel order (not GRB). NEO_GRB makes
// "purple" advertising colour render as cyan on this board.
static Adafruit_NeoPixel sLed(1, CFG_LED_PIN, NEO_RGB + NEO_KHZ800);

// Set pixel to an explicit 32-bit colour (0 = off).
static inline void ledWrite(uint32_t color) {
  sLed.setPixelColor(0, color);
  sLed.show();
}

// Shorthand colour constants — keep all magic RGB values in one place.
static const uint32_t kColShield   = Adafruit_NeoPixel::Color(CFG_LED_COLOR_SHIELD);
static const uint32_t kColTivo     = Adafruit_NeoPixel::Color(CFG_LED_COLOR_TIVO);
static const uint32_t kColReady    = Adafruit_NeoPixel::Color(CFG_LED_COLOR_READY);
static const uint32_t kColReset    = Adafruit_NeoPixel::Color(CFG_LED_COLOR_RESET);
static const uint32_t kColActivity = Adafruit_NeoPixel::Color(CFG_LED_COLOR_ACTIVITY);
static const uint32_t kColBoot     = Adafruit_NeoPixel::Color(CFG_LED_COLOR_BOOT);

// ============================================================
// LED state machine
// ============================================================

enum class LedPattern : uint8_t {
  Off,
  SlowBlink,         // 500/500 ms — advertising, waiting for Shield to pair
  DoubleFlash,       // orange flash-flash … repeat — BOOT 4 s warning
  DoubleFlashShield, // purple flash-flash … repeat — BOOT 8 s Shield cleared
  ReadyOnce,         // 3 quick green flashes then → sLedAfterOnce
  ConfirmOnce,       // 3 quick flashes in sConfirmColor then → chain or BootHold
  Ready,             // steady green — both devices connected and translating
  BootBlink,         // yellow 500/500 ms — BLE stack initialising at boot
  BootHold,          // fast yellow blink — BOOT held, counting
  Activity,          // single CFG_LED_FLASH_MS pulse — button forwarded to Shield
};

struct LedStep { uint16_t ms; bool on; };

// DoubleFlash: flash, gap, flash, long-pause (repeating) — one pair per cycle
static const LedStep kDoubleFlashSteps[] = {
  { CFG_LED_DBL_FLASH_MS, true  },
  { CFG_LED_DBL_GAP_MS,   false },
  { CFG_LED_DBL_FLASH_MS, true  },
  { CFG_LED_DBL_PAUSE_MS, false },
};
static constexpr int kDoubleFlashN =
    (int)(sizeof(kDoubleFlashSteps) / sizeof(kDoubleFlashSteps[0]));

// ReadyOnce: three equally-spaced flashes (one-shot, then → sLedAfterOnce)
static const LedStep kReadyOnceSteps[] = {
  { CFG_LED_READY_FLASH_MS, true  },
  { CFG_LED_READY_GAP_MS,   false },
  { CFG_LED_READY_FLASH_MS, true  },
  { CFG_LED_READY_GAP_MS,   false },
  { CFG_LED_READY_FLASH_MS, true  },
  { CFG_LED_READY_GAP_MS,   false },
};
static constexpr int kReadyOnceN =
    (int)(sizeof(kReadyOnceSteps) / sizeof(kReadyOnceSteps[0]));

static LedPattern    sLedBase         = LedPattern::SlowBlink;
static LedPattern    sLedCurrent      = LedPattern::SlowBlink;
static LedPattern    sLedAfterOnce    = LedPattern::Ready;
static int           sLedStep         = 0;
static unsigned long sLedAt           = 0;
static uint32_t      sConfirmColor    = 0;
static LedPattern    sLedChainNext    = LedPattern::Off;
static bool          sFactoryLedSeq   = false;
static bool          sBtnLedOverride  = false;
static unsigned long sBtnAt           = 0;
static uint8_t       sBtnLevel          = 0;   // 0=start, 1=4s, 2=5s, 3=8s, 4=10s
static bool          sBtnHeld           = false;

static void ledSetBase(LedPattern p) {
  if (p == sLedBase && sLedCurrent != LedPattern::Activity) return;
  sLedBase = p;
  if (sLedCurrent == LedPattern::Activity) return;
  sLedCurrent = p;
  sLedStep    = 0;
  sLedAt      = millis();
}

// Force a pattern immediately (used by button actions — ignores early-exit guard).
static void ledForce(LedPattern p) {
  sLedBase = p;
  if (sLedCurrent != LedPattern::Activity) {
    sLedCurrent = p;
    sLedStep    = 0;
    sLedAt      = millis();
  }
}

// BOOT-button feedback; blocks syncLedState until released or animation finishes.
static void ledForceButton(LedPattern p) {
  sBtnLedOverride = true;
  ledForce(p);
}

static void ledConfirmOnce(uint32_t color) {
  sConfirmColor = color;
  ledForceButton(LedPattern::ConfirmOnce);
}

static void resumeBootHoldIfPressed() {
  if (sBtnHeld && sBtnLevel < 4) ledForceButton(LedPattern::BootHold);
}

static void startFactoryLedSequence() {
  sFactoryLedSeq  = true;
  sConfirmColor   = kColReset;
  sLedChainNext   = LedPattern::ReadyOnce;
  sLedAfterOnce   = LedPattern::SlowBlink;
  ledForceButton(LedPattern::ConfirmOnce);
}

// Single activity flash (highest priority). Returns to base when done.
// Safe to call from BLE callback context.
static void ledActivity() {
  sLedCurrent = LedPattern::Activity;
  sLedAt      = millis();
  ledWrite(kColActivity);
}

// Advance the LED state machine — must be called every loop() iteration.
static void ledTick() {
  unsigned long now = millis();
  unsigned long age = now - sLedAt;

  switch (sLedCurrent) {

    case LedPattern::Off:
      ledWrite(0);
      break;

    case LedPattern::Activity:
      if (age >= CFG_LED_FLASH_MS) {
        ledWrite(0);
        sLedCurrent = sLedBase;
        sLedStep    = 0;
        sLedAt      = now;
      }
      break;

    case LedPattern::SlowBlink:
      if (sLedStep == 0) {
        ledWrite(kColShield);
        if (age >= CFG_LED_BLINK_ON_MS) { sLedStep = 1; sLedAt = now; }
      } else {
        ledWrite(0);
        if (age >= CFG_LED_BLINK_OFF_MS) { sLedStep = 0; sLedAt = now; }
      }
      break;

    case LedPattern::DoubleFlash: {
      const LedStep& s = kDoubleFlashSteps[sLedStep];
      ledWrite(s.on ? kColTivo : 0);
      if (age >= s.ms) { sLedStep = (sLedStep + 1) % kDoubleFlashN; sLedAt = now; }
      break;
    }

    case LedPattern::DoubleFlashShield: {
      const LedStep& s = kDoubleFlashSteps[sLedStep];
      ledWrite(s.on ? kColShield : 0);
      if (age >= s.ms) { sLedStep = (sLedStep + 1) % kDoubleFlashN; sLedAt = now; }
      break;
    }

    case LedPattern::ConfirmOnce: {
      const LedStep& s = kReadyOnceSteps[sLedStep];
      ledWrite(s.on ? sConfirmColor : 0);
      if (age >= s.ms) {
        sLedAt = now;
        if (++sLedStep >= kReadyOnceN) {
          sLedStep = 0;
          if (sLedChainNext != LedPattern::Off) {
            LedPattern next = sLedChainNext;
            sLedChainNext = LedPattern::Off;
            sLedCurrent   = next;
            sLedBase      = next;
          } else if (sBtnHeld) {
            resumeBootHoldIfPressed();
          } else {
            sBtnLedOverride = false;
          }
        }
      }
      break;
    }

    case LedPattern::ReadyOnce: {
      const LedStep& s = kReadyOnceSteps[sLedStep];
      ledWrite(s.on ? kColReady : 0);
      if (age >= s.ms) {
        sLedAt = now;
        if (++sLedStep >= kReadyOnceN) {
          sLedBase    = sLedAfterOnce;
          sLedCurrent = sLedAfterOnce;
          sLedStep    = 0;
          if (sFactoryLedSeq) {
            sFactoryLedSeq  = false;
            sBtnLedOverride = false;
          }
        }
      }
      break;
    }

    case LedPattern::Ready:
      ledWrite(kColReady);
      break;

    case LedPattern::BootBlink:
      if (sLedStep == 0) {
        ledWrite(kColBoot);
        if (age >= CFG_LED_BLINK_ON_MS) { sLedStep = 1; sLedAt = now; }
      } else {
        ledWrite(0);
        if (age >= CFG_LED_BLINK_OFF_MS) { sLedStep = 0; sLedAt = now; }
      }
      break;

    case LedPattern::BootHold:
      if (sLedStep == 0) {
        ledWrite(kColBoot);
        if (age >= CFG_LED_BOOT_HOLD_ON_MS) { sLedStep = 1; sLedAt = now; }
      } else {
        ledWrite(0);
        if (age >= CFG_LED_BOOT_HOLD_OFF_MS) { sLedStep = 0; sLedAt = now; }
      }
      break;
  }
}

// Hardware timer keeps boot blink alive while NimBLE init blocks the loop task.
static esp_timer_handle_t sLedTimer = nullptr;

static void ledTimerCb(void* /*arg*/) { ledTick(); }

static void ledPumpStart() {
  if (sLedTimer) return;
  const esp_timer_create_args_t args = {
      .callback = &ledTimerCb,
      .arg      = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name     = "ledpump",
      .skip_unhandled_events = true,
  };
  if (esp_timer_create(&args, &sLedTimer) == ESP_OK) {
    esp_timer_start_periodic(sLedTimer, 10 * 1000);  // 10 ms
  }
}

static void ledPumpStop() {
  if (!sLedTimer) return;
  esp_timer_stop(sLedTimer);
  esp_timer_delete(sLedTimer);
  sLedTimer = nullptr;
}

// ============================================================
// Central (TiVo remote) state
// ============================================================

static NimBLEClient*           pClient      = nullptr;
static bool                    doConnect    = false;
static NimBLEAddress           sPendingAddr;
static bool                    sHavePendingAddr = false;
static NimBLEAddress           sBondedAddr;
static bool                    sHasBond     = false;
static bool                    sTivoBondTrusted = false;
static unsigned long           sTivoReadyAt = 0;
static unsigned long           sReconnectAt = 0;

static Preferences sTivoPrefs;

static const NimBLEUUID HID_SERVICE("1812");
static const NimBLEUUID REPORT_CHAR("2A4D");
static const NimBLEUUID REPORT_MAP_CHAR("2A4B");
static const NimBLEUUID PROTOCOL_MODE_CHAR("2A4E");

static bool sTivoConnecting = false;
static bool sTivoReady      = false;
static bool sTivoPendingSecure = false;
static bool sTivoNeedSetup  = false;
static unsigned long sTivoSecureAt = 0;
static unsigned long sTivoRetryAt = 0;
static unsigned long sShieldReadyAt = 0;
static unsigned long sBootMs = 0;
static bool sWasShieldReadyForTivo = false;
static bool sTivoSuppressReconnect = false;
static bool sPendingForgetTiVo = false;
static unsigned long sScanLogAt = 0;

static bool subscribeReports(NimBLERemoteService* hid);
static void printHex(const uint8_t* data, size_t len);

static void scheduleTivoRetry();
static void releaseTivoClient(NimBLEClient* client);

// Solid orange during blocking BLE work so the LED does not freeze off.
static void ledHoldForBleWork() {
  if (!sBtnLedOverride) ledWrite(kColTivo);
}

static bool tivoCentralBusy() {
  return sTivoConnecting || doConnect || sHavePendingAddr || sTivoPendingSecure ||
         sTivoNeedSetup || (pClient != nullptr && !sTivoReady);
}

// Called from onDisconnect only — link is already down; do not disconnect again.
static void releaseTivoClient(NimBLEClient* client) {
  sTivoPendingSecure = false;
  sTivoNeedSetup     = false;
  sHavePendingAddr   = false;
  if (pClient == client) pClient = nullptr;
  sTivoConnecting = false;
  sTivoReady      = false;
  hidResumeAfterTivoCentral();
  if (client) NimBLEDevice::deleteClient(client);
}

// Drop the central client from loop / button paths (disconnect defers delete to onDisconnect).
static void cleanupTivoClient() {
  NimBLEClient* client = pClient;
  pClient = nullptr;
  sTivoPendingSecure = false;
  sTivoNeedSetup     = false;
  sHavePendingAddr   = false;
  sTivoConnecting = false;
  sTivoReady      = false;
  hidResumeAfterTivoCentral();
  if (!client) return;
  if (client->isConnected()) client->disconnect();
  else NimBLEDevice::deleteClient(client);
}

static bool setupTivoHid(NimBLEClient* client) {
  NimBLERemoteService* hid = client->getService(HID_SERVICE);
  if (!hid) {
    DEV_LOGLN("[Central] No HID service found.");
    return false;
  }

  NimBLERemoteCharacteristic* rmap = hid->getCharacteristic(REPORT_MAP_CHAR);
  if (rmap && rmap->canRead()) {
    std::string m = rmap->readValue();
    DEV_LOGF("[Central] Report Map len=%u\r\n", m.length());
    printHex((const uint8_t*)m.data(), m.length());
  }

  NimBLERemoteCharacteristic* proto = hid->getCharacteristic(PROTOCOL_MODE_CHAR);
  if (proto && proto->canWrite()) {
    uint8_t mode = 0x01;
    proto->writeValue(&mode, 1, false);
  }

  if (!subscribeReports(hid)) {
    DEV_LOGLN("[Central] No reports subscribed.");
    return false;
  }

  return true;
}

static void saveTiVoBond(NimBLEClient* client) {
  if (!client) return;
  sBondedAddr = client->getPeerAddress();
  sHasBond    = true;
  sTivoBondTrusted = true;
  sTivoPrefs.begin(CFG_NVS_TIVO_NS, false);
  sTivoPrefs.putString(CFG_NVS_TIVO_ADDR, sBondedAddr.toString().c_str());
  sTivoPrefs.putBool(CFG_NVS_TIVO_TRUSTED, true);
  sTivoPrefs.end();
  DEV_LOGF("[Central] TiVo bond confirmed: %s\r\n", sBondedAddr.toString().c_str());
}

static void invalidateTiVoBond(const char* reason) {
  DEV_LOGF("[Central] Clearing TiVo bond — %s\r\n", reason);
  if (sHasBond) {
    NimBLEDevice::deleteBond(sBondedAddr);
    sHasBond = false;
  }
  sTivoBondTrusted = false;
  sReconnectAt     = 0;
  sTivoPrefs.begin(CFG_NVS_TIVO_NS, false);
  if (sTivoPrefs.isKey(CFG_NVS_TIVO_ADDR))
    sTivoPrefs.remove(CFG_NVS_TIVO_ADDR);
  if (sTivoPrefs.isKey(CFG_NVS_TIVO_TRUSTED))
    sTivoPrefs.remove(CFG_NVS_TIVO_TRUSTED);
  sTivoPrefs.end();
}

static void scheduleTivoRetry() {
  sTivoRetryAt = millis() + CFG_TIVO_RETRY_MS;
}

// Shield side is "done" when CCCD negotiation finished, or bonded but disconnected.
static bool shieldDoneForTivo() {
#if CFG_DEBUG_TIVO_ONLY
  return true;
#else
  return hidShieldReady() ||
         (hidHasShieldBond() && !hidPeripheralConnected());
#endif
}

// Orange double-flash = waiting for TiVo pairing.
static bool ledWaitingForTivo() {
  if (sTivoReady && sTivoBondTrusted) return false;
#if CFG_DEBUG_TIVO_ONLY
  // DEBUG: no Shield — deep orange whenever TiVo is not fully confirmed (incl. while scanning).
  return true;
#else
  if (!hidShieldReady()) return false;
  if (!sWasShieldReadyForTivo) return false;
  if (millis() - sShieldReadyAt < CFG_TIVO_POST_SHIELD_MS) return false;
  return true;
#endif
}

// Start TiVo central scan or reconnect only after Shield pairing is complete.
static void tryStartTiVoCentral() {
  if (!shieldDoneForTivo()) return;
  if (tivoCentralBusy()) return;
  if (sTivoReady) return;
  if (sTivoRetryAt && millis() < sTivoRetryAt) return;

#if !CFG_DEBUG_TIVO_ONLY
  // Let Shield reconnect to the peripheral before TiVo central pauses advertising.
  if (hidHasShieldBond() && !hidShieldReady() && sBootMs &&
      (millis() - sBootMs) < CFG_SHIELD_RECONNECT_BOOT_MS) {
    return;
  }

  // Let Shield finish CCCD + initial conn-param update before central work.
  if (hidShieldReady()) {
    if (!sWasShieldReadyForTivo) {
      sWasShieldReadyForTivo = true;
      sShieldReadyAt         = millis();
    }
    if (millis() - sShieldReadyAt < CFG_TIVO_POST_SHIELD_MS) return;
  } else {
    sWasShieldReadyForTivo = false;
  }
#endif

  if (sHasBond && sTivoBondTrusted) {
    if (!doConnect) {
#if CFG_DEBUG_TIVO_ONLY
      DEV_LOGLN("[Central] DEBUG — reconnecting to trusted TiVo bond...");
#else
      DEV_LOGLN("[Central] Reconnecting to trusted TiVo bond...");
#endif
      doConnect = true;
    }
    return;
  }

  if (sHasBond && !sTivoBondTrusted) {
    static bool sWarnedUntrusted = false;
    if (!sWarnedUntrusted) {
      sWarnedUntrusted = true;
      DEV_LOGLN("[Central] Unconfirmed TiVo addr — reset remote, any btn for pairing.");
    }
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan && !scan->isScanning()) {
#if CFG_DEBUG_TIVO_ONLY
    DEV_LOGLN("[Central] DEBUG — scanning for TiVo (pairing mode on remote).");
#else
    DEV_LOGLN("[Central] Shield ready — scanning for TiVo (pairing mode on remote).");
#endif
    DEV_LOGLN("[Central] Remote: Power+TiVo → Vol− x3 → TiVo → any button (pairing mode).");
    sScanLogAt = millis();
    scan->start(0, nullptr, false);
  }
}

// ============================================================
// LED sync — set background pattern from BLE state automatically.
// Skips if a button-driven special pattern is active.
// ============================================================

static void syncLedState() {
  // BOOT hold / confirmation animations take priority over BLE state.
  if (sBtnLedOverride) return;
  if (sLedCurrent == LedPattern::ReadyOnce)  return;
  if (sLedCurrent == LedPattern::ConfirmOnce) return;

#if CFG_DEBUG_TIVO_ONLY
  bool bothReady = sTivoReady && sTivoBondTrusted;
#else
  bool bothReady = hidShieldReady() && sTivoReady && sTivoBondTrusted;
#endif
  bool waitingForTivo = ledWaitingForTivo();

  static bool sWasBothReady = false;

  if (bothReady && !sWasBothReady) {
    // Both just became ready — play 3-flash confirmation then steady green
    sLedAfterOnce = LedPattern::Ready;
    sLedBase      = LedPattern::ReadyOnce;
    sLedCurrent   = LedPattern::ReadyOnce;
    sLedStep      = 0;
    sLedAt        = millis();
  } else if (bothReady) {
    ledSetBase(LedPattern::Ready);
  } else if (!bothReady) {
    ledSetBase(waitingForTivo ? LedPattern::DoubleFlash : LedPattern::SlowBlink);
  }

  sWasBothReady = bothReady;
}

// ============================================================
// Display helpers
// ============================================================

static void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) DEV_LOGF("%02X ", data[i]);
  DEV_LOGLN();
}

static const struct { uint16_t code; const char* name; } kTiVoNames[] = {
  { 0x009C, "Chan Up" },   { 0x009D, "Chan Down" },
  { 0x0082, "Input" },     { 0x008D, "Guide" },
  { 0x003D, "TiVo" },      { 0xCE00, "Skip" },
  { 0x003E, "Live TV" },   { 0x01C8, "Netflix" },
  { 0x0209, "Info" },
};

static const struct { uint16_t code; const char* name; } kUsageNames[] = {
  { 0x0030, "Power" },       { 0x0040, "Menu" },
  { 0x0041, "Menu Pick" },   { 0x0042, "Menu Up" },
  { 0x0043, "Menu Down" },   { 0x0044, "Menu Left" },
  { 0x0045, "Menu Right" },  { 0x0082, "Mute" },
  { 0x009C, "Play" },        { 0x009D, "Pause" },
  { 0x00B5, "Next Track" },  { 0x00B6, "Prev Track" },
  { 0x00CD, "Play/Pause" },  { 0x00E2, "Mute" },
  { 0x00E9, "Volume Increment" }, { 0x00EA, "Volume Decrement" },
  { 0x0183, "AL Media Select" },  { 0x0184, "AL Home" },
  { 0x0221, "AC Search" },   { 0x0222, "AC Go To" },
  { 0x0223, "AC Home" },     { 0x0224, "AC Back" },
  { 0x0225, "AC Forward" },  { 0x0226, "AC Stop" },
};

static const struct { uint8_t code; const char* name; } kKeyNames[] = {
  { 0x1E,"1" },{ 0x1F,"2" },{ 0x20,"3" },{ 0x21,"4" },{ 0x22,"5" },
  { 0x23,"6" },{ 0x24,"7" },{ 0x25,"8" },{ 0x26,"9" },{ 0x27,"0" },
  { 0x28,"Enter" },{ 0x29,"Esc" },{ 0x2A,"Backspace" },
  { 0x4A,"Home" },{ 0x4D,"End" },{ 0x4F,"Right" },
  { 0x50,"Left" },{ 0x51,"Down" },{ 0x52,"Up" },
};

static const char* lookupUsage(uint16_t code) {
  for (auto& e : kTiVoNames)  if (e.code == code) return e.name;
  for (auto& e : kUsageNames) if (e.code == code) return e.name;
  return nullptr;
}
static const char* lookupKey(uint8_t code) {
  for (auto& e : kKeyNames) if (e.code == code) return e.name;
  return nullptr;
}

// ============================================================
// HID notify callback (central side)
// ============================================================

// Dedup state — intentionally NOT cleared on key-up to absorb the TiVo's
// rapid keydown→keyup→keydown bounce (auto-repeat initiation).
static uint8_t       sLastData[20];
static size_t        sLastLen        = 0;
static bool          sKeyIsDown      = false;
static unsigned long sKeyDownAt      = 0;
static unsigned long sKbReleaseAt    = 0;
static unsigned long sShieldParamsAt = 0;
static bool          sWasShieldReady = false;

// kAllZeroGuardMs: ignore same-length all-zero reports arriving within this window
//   of a keydown — they are the other consumer characteristic's idle value.
// kBounceMsGuard: minimum gap before the same code is accepted again after key-up,
//   absorbing the TiVo's rapid keydown→keyup→keydown auto-repeat initiation.
static constexpr unsigned long kAllZeroGuardMs = CFG_ALL_ZERO_GUARD_MS;
static constexpr unsigned long kBounceGuardMs  = CFG_BOUNCE_GUARD_ACTION_MS;

void hidNotifyCallback(
    NimBLERemoteCharacteristic* pChar,
    uint8_t* pData, size_t length, bool isNotify)
{
  bool allZero = true;
  for (size_t i = 0; i < length; i++) {
    if (pData[i] != 0x00) { allZero = false; break; }
  }

  if (allZero) {
    if (length == sLastLen) {
      bool lastWasNav = (sLastLen >= 2 && sLastLen != 8 &&
                         sLastData[0] >= 0x42 && sLastData[0] <= 0x45 &&
                         sLastData[1] == 0x00);
      if (lastWasNav || millis() - sKeyDownAt >= kAllZeroGuardMs) {
        sKeyIsDown = false;
        hidReleaseConsumer();
      }
    }
    return;
  }

  unsigned long now = millis();

  // Suppress hold-repeat: same data while key is physically held
  if (sKeyIsDown && length == sLastLen && memcmp(pData, sLastData, length) == 0) return;

  // Nav keys (Menu Up/Down/Left/Right) bypass the bounce guard so auto-repeat works
  bool isNavKey = false;
  if (length >= 2 && length != 8) {
    uint16_t u = pData[0] | (pData[1] << 8);
    isNavKey = (u >= 0x0042 && u <= 0x0045);
  }

  // Suppress bounce: same code arriving too soon after key-up
  if (!isNavKey && !sKeyIsDown && length == sLastLen &&
      memcmp(pData, sLastData, length) == 0 &&
      (now - sKeyDownAt) < kBounceGuardMs) {
    return;
  }

  sKeyIsDown = true;
  sKeyDownAt = now;
  sLastLen = (length < sizeof(sLastData)) ? length : sizeof(sLastData);
  memcpy(sLastData, pData, sLastLen);

  // ---- Print to serial ----
  DEV_LOG("BTN raw=[");
  for (size_t i = 0; i < length; i++) DEV_LOGF("%02X ", pData[i]);
  DEV_LOG("] ");

  if (length == 8) {
    bool found = false;
    for (size_t i = 2; i < 8; i++) {
      if (pData[i] == 0) continue;
      const char* n = lookupKey(pData[i]);
      DEV_LOGF("=> Key: %s\r\n", n ? n : "?");
      found = true;
      break;
    }
    if (!found) DEV_LOGLN("=> (no key)");
  } else if (length >= 2) {
    uint16_t usage = pData[0] | (pData[1] << 8);
    const char* n = lookupUsage(usage);
    DEV_LOGF("=> 0x%04X  %s\r\n", usage, n ? n : "(unknown)");
  } else {
    DEV_LOGLN();
  }

  // ---- Translate and forward to Shield ----
#if CFG_DEBUG_TIVO_ONLY
  if (!hidShieldReady()) {
    ledActivity();
    return;
  }
#endif
  // Keyboard translations: forced CFG_KB_PULSE_MS pulse so Shield never sees a held key.
  // Consumer pass-throughs: natural timing — TiVo key-up fires hidReleaseConsumer().
  if (length == 8) {
    hidSendKeyboardRaw(pData);
    sKbReleaseAt = millis() + CFG_KB_PULSE_MS;
  } else if (length >= 2) {
    uint16_t usage = pData[0] | (pData[1] << 8);
    OutputType outType;
    uint16_t   outCode;
    keymapLookupConsumer(usage, outType, outCode);
    if (outType == OutputType::Keyboard) {
      hidSendKeyboard(0x00, (uint8_t)outCode);
      sKbReleaseAt = millis() + CFG_KB_PULSE_MS;
    } else if (usage == 0x0030) {
      // Power key: send as consumer but force a timed release (don't rely on TiVo key-up)
      hidSendConsumer(outCode);
      sKbReleaseAt = millis() + CFG_KB_PULSE_MS;
    } else {
      hidSendConsumer(outCode);
    }
  }

  // Activity flash — overrides current LED pattern briefly, returns to base
  ledActivity();
}

// ============================================================
// Central BLE callbacks
// ============================================================

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* client) override {
    DEV_LOGLN("[Central] Connected to TiVo remote.");
    if (client->getConnInfo().isEncrypted()) {
      DEV_LOGLN("[Central] Link already encrypted — skipping secureConnection.");
      sTivoPendingSecure = false;
      sTivoNeedSetup     = true;
    } else {
      sTivoSecureAt      = millis() + CFG_TIVO_SECURE_DELAY_MS;
      sTivoPendingSecure = true;
    }
  }

  void onDisconnect(NimBLEClient* client) override {
    unsigned long upMs =
        sTivoReadyAt ? (millis() - sTivoReadyAt) : 0;
    DEV_LOGF("[Central] Disconnected from TiVo remote (up %lu ms).\r\n", upMs);
    sTivoReadyAt = 0;
    releaseTivoClient(client);

    if (sTivoSuppressReconnect) {
      sTivoSuppressReconnect = false;
      return;
    }

    if (upMs > 0 && upMs < CFG_TIVO_BOND_MIN_UP_MS) {
      invalidateTiVoBond("link dropped before bond was confirmed");
      scheduleTivoRetry();
      tryStartTiVoCentral();
      return;
    }

    if (sHasBond && sTivoBondTrusted) {
      DEV_LOGLN("[Central] Reconnecting in 3 s...");
      sReconnectAt = millis() + CFG_TIVO_RECONNECT_MS;
    } else {
#if CFG_DEBUG_TIVO_ONLY
      DEV_LOGLN("[Central] No trusted bond — will scan (DEBUG mode).");
#else
      DEV_LOGLN("[Central] No trusted bond — will scan when Shield is ready.");
#endif
      scheduleTivoRetry();
      tryStartTiVoCentral();
    }
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    if (!desc->sec_state.encrypted) {
      DEV_LOGLN("[Central] Encryption failed. Disconnecting.");
      if (pClient) pClient->disconnect();
      scheduleTivoRetry();
      return;
    }
    DEV_LOGLN("[Central] Encrypted / bonded.");
    // GATT must run from loop(), not from this NimBLE callback.
    sTivoPendingSecure = false;
    sTivoNeedSetup     = true;
  }
};

static ClientCallbacks sTivoClientCb;

class AdvertisedCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    if (sHasBond) {
      if (dev->getAddress() != sBondedAddr) return;
    } else {
      // First-time pair: require HID service so we don't grab random "Remote" devices.
      if (!dev->haveServiceUUID() || !dev->isAdvertisingService(HID_SERVICE))
        return;
    }

    DEV_LOGF("[Central] Found remote: %s\r\n", dev->toString().c_str());
    sPendingAddr     = dev->getAddress();
    sHavePendingAddr = true;
    sTivoConnecting  = true;
    doConnect        = true;
    NimBLEDevice::getScan()->stop();
  }
};

// ---- Subscribe to all notifiable 2A4D characteristics ----

bool subscribeReports(NimBLERemoteService* hid) {
  bool any = false;
  auto chars = hid->getCharacteristics(true);
  for (auto& c : *chars) {
    if (!c->getUUID().equals(REPORT_CHAR)) continue;
    if (c->canNotify()) {
      if (c->subscribe(true, hidNotifyCallback)) {
        DEV_LOGLN("[Central] Subscribed to 2A4D.");
        any = true;
      } else {
        DEV_LOGLN("[Central] Subscribe to 2A4D failed.");
      }
    }
  }
  return any;
}

// ---- Connect (pairing + GATT setup finish in onAuthenticationComplete) ----

static bool tivoStartConnect(NimBLEAddress addr) {
  sTivoConnecting    = true;
  sTivoReady         = false;
  sTivoPendingSecure = false;
  sTivoNeedSetup     = false;
  DEV_LOGF("[Central] Connecting to %s ...\r\n", addr.toString().c_str());
  ledHoldForBleWork();

  hidPauseForTivoCentral();

  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(&sTivoClientCb, false);
  pClient->setConnectionParams(CFG_TIVO_CONN_MIN_INTERVAL, CFG_TIVO_CONN_MAX_INTERVAL,
                               CFG_TIVO_CONN_LATENCY, CFG_TIVO_CONN_TIMEOUT);
  pClient->setConnectTimeout(CFG_CONNECT_TIMEOUT_S);

  if (!pClient->connect(addr)) {
    DEV_LOGLN("[Central] Connection failed.");
    cleanupTivoClient();
    scheduleTivoRetry();
    return false;
  }

  if (pClient->getConnInfo().isEncrypted()) {
    sTivoNeedSetup = true;
  }
  return true;
}

static void tivoFinishSetup() {
  if (!pClient || !pClient->isConnected()) {
    DEV_LOGLN("[Central] Client gone before HID setup.");
    cleanupTivoClient();
    scheduleTivoRetry();
    return;
  }

  if (!pClient->getConnInfo().isEncrypted()) {
    DEV_LOGLN("[Central] HID setup deferred — waiting for encryption.");
    sTivoNeedSetup     = false;
    sTivoPendingSecure = true;
    sTivoSecureAt      = millis();
    return;
  }

  ledHoldForBleWork();
  if (!setupTivoHid(pClient)) {
    if (pClient) pClient->disconnect();
    scheduleTivoRetry();
    return;
  }

  sTivoConnecting = false;
  sTivoReady      = true;
  sTivoReadyAt    = millis();
  sTivoRetryAt    = 0;
  DEV_LOGF("[Central] TiVo linked — hold connection %u s to confirm bond...\r\n",
                (unsigned)(CFG_TIVO_BOND_MIN_UP_MS / 1000));
#if CFG_DEBUG_TIVO_ONLY
  DEV_LOGLN("[Central] Press remote buttons — BTN lines should appear on serial.");
#else
  DEV_LOGLN("[Central] Waiting to confirm bond before marking ready.");
  if (!hidShieldReady()) {
    hidResumeAfterTivoCentral();
  }
#endif
}

// Called from loop() — pairing/GATT must not run inside NimBLE callbacks.
static void tivoCentralTick() {
  if (!pClient || !pClient->isConnected()) return;

  if (sTivoPendingSecure) {
    if (millis() < sTivoSecureAt) return;

    sTivoPendingSecure = false;
    if (pClient->getConnInfo().isEncrypted()) {
      DEV_LOGLN("[Central] Already encrypted — proceeding to HID setup.");
      sTivoNeedSetup = true;
      return;
    }

    DEV_LOGLN("[Central] Starting pairing / encryption...");
    ledHoldForBleWork();
    if (!pClient->secureConnection()) {
      DEV_LOGLN("[Central] Pairing/encryption failed.");
      if (pClient) pClient->disconnect();
      scheduleTivoRetry();
    }
    return;
  }

  if (sTivoNeedSetup) {
    sTivoNeedSetup = false;
    DEV_LOGLN("[Central] Discovering HID service / subscribing...");
    tivoFinishSetup();
  }
}

static void tivoBondConfirmTick() {
  if (!sTivoReady || sTivoBondTrusted || !pClient || !pClient->isConnected()) return;
  if (millis() - sTivoReadyAt < CFG_TIVO_BOND_MIN_UP_MS) return;

  saveTiVoBond(pClient);
#if CFG_DEBUG_TIVO_ONLY
  DEV_LOGLN("[Central] TiVo ready (DEBUG — bond confirmed).");
#else
  DEV_LOGLN("[Central] Ready — forwarding to Shield.");
#endif
}

// ============================================================
// Bond management helpers (called from buttonTick)
// ============================================================

static void forgetTiVo() {
  invalidateTiVoBond("user requested (BOOT 5 s)");
  sReconnectAt = 0;
  sTivoRetryAt = 0;
  doConnect    = false;
  sTivoSuppressReconnect = true;
  if (pClient) cleanupTivoClient();
  else sTivoSuppressReconnect = false;
  DEV_LOGLN("[BTN] TiVo bond cleared — will scan.");
#if CFG_DEBUG_TIVO_ONLY
  DEV_LOGLN("[BTN] DEBUG mode — Shield not required for scan.");
#endif
  tryStartTiVoCentral();
}

static void queueForgetTiVo() {
  sPendingForgetTiVo = true;
}

static void forgetShield() {
  hidForgetShield(sBondedAddr, sHasBond);
  DEV_LOGLN("[BTN] Shield bond cleared — re-advertising.");
}

static void factoryReset() {
  queueForgetTiVo();
  forgetShield();
  keymapClearCustom();
  DEV_LOGLN("[BTN] Factory reset complete — all bonds and keymap cleared.");
}

// ============================================================
// Boot button handler (non-blocking, called every loop iteration)
// Cumulative hold thresholds: 4 s → 5 s → 8 s → 10 s
// ============================================================

static void buttonTick() {
  bool pressed = (digitalRead(CFG_BOOT_BTN_PIN) == LOW);

  if (pressed) {
    if (!sBtnHeld) {
      sBtnAt    = millis();
      sBtnLevel = 0;
      sBtnHeld  = true;
      ledForceButton(LedPattern::BootHold);
    }

    unsigned long held = millis() - sBtnAt;

    if (held >= CFG_BTN_FACTORY_MS && sBtnLevel < 4) {
      sBtnLevel = 4;
      factoryReset();
      startFactoryLedSequence();
      DEV_LOGLN("[BTN] 10 s: factory reset.");

    } else if (held >= CFG_BTN_SHIELD_MS && sBtnLevel < 3) {
      sBtnLevel = 3;
      forgetShield();
      ledForceButton(LedPattern::DoubleFlashShield);
      DEV_LOGLN("[BTN] 8 s: Shield bond forgotten.");

    } else if (held >= CFG_BTN_TIVO_MS && sBtnLevel < 2) {
      sBtnLevel = 2;
      queueForgetTiVo();
      ledConfirmOnce(kColShield);
      DEV_LOGLN("[BTN] 5 s: TiVo bond forgotten.");

    } else if (held >= CFG_BTN_WARN_MS && sBtnLevel < 1) {
      sBtnLevel = 1;
      ledForceButton(LedPattern::DoubleFlash);
      DEV_LOGLN("[BTN] 4 s: warning — keep holding for TiVo clear.");
    }

  } else if (sBtnHeld) {
    if (!sFactoryLedSeq && sLedCurrent != LedPattern::ConfirmOnce) {
      sBtnLedOverride = false;
    }
    sBtnHeld  = false;
    sBtnLevel = 0;
    sBtnAt    = 0;
  }
}

// ============================================================
// Arduino setup / loop
// ============================================================

// USB option 1 (app-only) keeps the NVS partition. Old BLE controller config there
// can make esp_bt_controller_init fail with "Config struct mismatch" / ESP_ERR_NO_MEM.
// Erase NVS when firmware version changes or NVS is corrupt.
static void peanutEnsureNvsCompatible() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    DEV_LOGLN("[BOOT] NVS corrupt — erasing");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    err = ESP_OK;
  }
  if (err != ESP_OK) {
    DEV_LOGLN("[BOOT] NVS init failed — erasing");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  nvs_handle_t h;
  char stored[16] = {};
  bool needErase = false;
  if (nvs_open(CFG_NVS_SYS_NS, NVS_READWRITE, &h) != ESP_OK) {
    needErase = true;
  } else {
    size_t len = sizeof(stored);
    if (nvs_get_str(h, CFG_NVS_SYS_FWVER, stored, &len) != ESP_OK ||
        strcmp(stored, CFG_FIRMWARE_VERSION) != 0) {
      needErase = true;
    }
    nvs_close(h);
  }

  if (needErase) {
    DEV_LOGF("[BOOT] NVS sync for %s — erasing (re-pair Shield + TiVo once)\r\n",
                  CFG_FIRMWARE_VERSION);
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    if (nvs_open(CFG_NVS_SYS_NS, NVS_READWRITE, &h) == ESP_OK) {
      nvs_set_str(h, CFG_NVS_SYS_FWVER, CFG_FIRMWARE_VERSION);
      nvs_set_u32(h, CFG_NVS_SYS_BLELAY, CFG_NVS_BLE_LAYOUT);
      nvs_commit(h);
      nvs_close(h);
    }
  }
}

void setup() {
  sBootMs = millis();

  sLed.begin();
  sLed.setBrightness(CFG_LED_BRIGHTNESS);
  ledForce(LedPattern::BootBlink);
  ledPumpStart();

  pinMode(CFG_BOOT_BTN_PIN, INPUT_PULLUP);

  if (ESP.getPsramSize() < 512 * 1024) {
    ledPumpStop();
    while (true) {
      ledWrite(kColReset);
      delay(400);
      ledWrite(0);
      delay(400);
    }
  }

  peanutEnsureNvsCompatible();

  NimBLEDevice::init(CFG_BLE_DEVICE_NAME);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  hidPeripheralInit();
  keymapInit();

  ledSetBase(LedPattern::SlowBlink);
  ledTick();
  ledPumpStop();

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  gDevLogReady = true;

  DEV_LOGF("\n=== TiVo BLE HID Translator %s ready ===\r\n", CFG_FIRMWARE_VERSION);

#if CFG_SHIELD_DEBUG
  hidShieldDebugLogBootReason();
#endif

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvertisedCallbacks(), false);
  scan->setInterval(CFG_SCAN_INTERVAL);
  scan->setWindow(CFG_SCAN_WINDOW);
  scan->setActiveScan(true);

  // Restore TiVo bond address from NVS
  sTivoPrefs.begin(CFG_NVS_TIVO_NS, true);
  String addrStr = sTivoPrefs.getString(CFG_NVS_TIVO_ADDR, "");
  sTivoBondTrusted = sTivoPrefs.getBool(CFG_NVS_TIVO_TRUSTED, false);
  sTivoPrefs.end();

  if (addrStr.length() > 0) {
    sBondedAddr = NimBLEAddress(addrStr.c_str());
    sHasBond    = true;
    if (sTivoBondTrusted) {
      DEV_LOGF("[Central] Trusted TiVo bond: %s\r\n", addrStr.c_str());
    } else {
      DEV_LOGF("[Central] Unconfirmed TiVo address in NVS: %s\r\n", addrStr.c_str());
      DEV_LOGLN("[Central] Reset remote if needed; any button for pairing mode.");
    }
  } else {
#if CFG_DEBUG_TIVO_ONLY
    DEV_LOGLN("[Central] No TiVo bond. DEBUG mode — TiVo scan starts at boot.");
#else
    DEV_LOGLN("[Central] No TiVo bond. Pair Shield first; then pair TiVo remote.");
#endif
  }

  // Load Shield bond so the peripheral knows which bond to preserve
  hidLoadShieldBond(sBondedAddr, sHasBond);
}

void loop() {
  // ---- LED and button (non-blocking) ----
  buttonTick();
  ledTick();

  if (sPendingForgetTiVo) {
    sPendingForgetTiVo = false;
    forgetTiVo();
  }

  {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan && scan->isScanning() && shieldDoneForTivo() && !sTivoReady &&
        !tivoCentralBusy() && (millis() - sScanLogAt >= 15000)) {
      sScanLogAt = millis();
      DEV_LOGLN("[Central] Still scanning — put remote in pairing mode.");
      DEV_LOGLN("[Central]   Power+TiVo → Vol− x3 → TiVo → any button.");
    }
  }

  syncLedState();
  tryStartTiVoCentral();
  tivoCentralTick();
  tivoBondConfirmTick();

  // ---- Shield CCCD edge — schedule fast BLE params ----
  {
    bool isReady = hidShieldReady();
    if (isReady && !sWasShieldReady) {
      sShieldParamsAt = millis() + CFG_SHIELD_FAST_PARAMS_DELAY_MS;
      DEV_LOGLN("[HID] Shield CCCD confirmed — fast params in 1 s.");
    }
#if CFG_SHIELD_DEBUG
    if (!isReady && sWasShieldReady) {
      DEV_LOGF("[HID-DBG] Shield left ready (state=%s)\r\n", hidGetShieldState());
    }
#endif
    sWasShieldReady = isReady;
  }

#if CFG_SHIELD_DEBUG
  hidShieldDebugTick(pClient != nullptr && pClient->isConnected(),
                       sTivoReady, tivoCentralBusy());
#endif

  if (sShieldParamsAt && millis() >= sShieldParamsAt) {
    sShieldParamsAt = 0;
    hidRequestFastParams();
  }

  // ---- Pulse-release for translated keys and forced-release consumer keys ----
  if (sKbReleaseAt && millis() >= sKbReleaseAt) {
    sKbReleaseAt = 0;
    hidReleaseKeyboard();
    hidReleaseConsumer();  // no-op when consumer is already at zero
  }

  // ---- Timed reconnect after TiVo disconnect ----
  if (sReconnectAt && millis() >= sReconnectAt) {
    sReconnectAt = 0;
    doConnect    = true;
  }

  if (doConnect) {
    doConnect = false;

    NimBLEAddress addr;
    bool hasAddr = false;

    if (sHasBond) {
      addr    = sBondedAddr;
      hasAddr = true;
    } else if (sHavePendingAddr) {
      addr    = sPendingAddr;
      hasAddr = true;
      sHavePendingAddr = false;
    }

    if (hasAddr) {
      if (!tivoStartConnect(addr)) {
        DEV_LOGLN("[Central] Connect failed — retrying shortly.");
        tryStartTiVoCentral();
      }
    }
  }

  if (sTivoRetryAt && millis() >= sTivoRetryAt) {
    sTivoRetryAt = 0;
    tryStartTiVoCentral();
  }

  delay(1);
}
