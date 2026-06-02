#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
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
  SlowBlink,    // 500/500 ms — advertising, waiting for Shield to pair
  DoubleFlash,  // flash-flash…pause…flash-flash…long-pause — Shield bonded, no TiVo
  ReadyOnce,    // 3 quick flashes then → sLedAfterOnce
  Ready,        // steady green — both devices connected and translating
  RapidFlash,   // 100/100 ms — factory-reset hold in progress
  Activity,     // single CFG_LED_FLASH_MS pulse — button forwarded to Shield
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
static LedPattern    sLedAfterOnce    = LedPattern::Ready; // target after ReadyOnce finishes
static int           sLedStep         = 0;
static unsigned long sLedAt           = 0;
static unsigned long sFactoryAnimUntil = 0; // non-zero while RapidFlash timer is running

// Change the background pattern. If Activity is active it takes effect when Activity ends.
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

    case LedPattern::ReadyOnce: {
      const LedStep& s = kReadyOnceSteps[sLedStep];
      ledWrite(s.on ? kColReady : 0);
      if (age >= s.ms) {
        sLedAt = now;
        if (++sLedStep >= kReadyOnceN) {
          sLedBase    = sLedAfterOnce;
          sLedCurrent = sLedAfterOnce;
          sLedStep    = 0;
        }
      }
      break;
    }

    case LedPattern::Ready:
      ledWrite(kColReady);
      break;

    case LedPattern::RapidFlash:
      if (sLedStep == 0) {
        ledWrite(kColReset);
        if (age >= CFG_LED_RAPID_ON_MS) { sLedStep = 1; sLedAt = now; }
      } else {
        ledWrite(0);
        if (age >= CFG_LED_RAPID_OFF_MS) { sLedStep = 0; sLedAt = now; }
      }
      break;
  }
}

// ============================================================
// Central (TiVo remote) state
// ============================================================

static NimBLEClient*           pClient      = nullptr;
static NimBLEAdvertisedDevice* targetDevice = nullptr;
static bool                    doConnect    = false;
static NimBLEAddress           sBondedAddr;
static bool                    sHasBond     = false;
static unsigned long           sReconnectAt = 0;

static bool sTivoConnecting = false;
static bool sTivoReady      = false;

static const NimBLEUUID HID_SERVICE("1812");
static const NimBLEUUID REPORT_CHAR("2A4D");
static const NimBLEUUID REPORT_MAP_CHAR("2A4B");
static const NimBLEUUID PROTOCOL_MODE_CHAR("2A4E");

static Preferences sTivoPrefs;

// Shield side is "done" when CCCD negotiation finished, or bonded but disconnected.
static bool shieldDoneForTivo() {
  return hidShieldReady() ||
         (hidHasShieldBond() && !hidPeripheralConnected());
}

// Start TiVo central scan or reconnect only after Shield pairing is complete.
static void tryStartTiVoCentral() {
  if (!shieldDoneForTivo()) return;
  if (sTivoReady || sTivoConnecting || pClient != nullptr) return;

  if (sHasBond) {
    if (!doConnect) {
      Serial.println("[Central] Shield ready — connecting to stored TiVo bond...");
      doConnect = true;
    }
    return;
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan && !scan->isScanning()) {
    Serial.println("[Central] Shield ready — scanning for TiVo (TiVo + Back on remote).");
    scan->start(0, nullptr, false);
  }
}

// ============================================================
// LED sync — set background pattern from BLE state automatically.
// Skips if a button-driven special pattern is active.
// ============================================================

static void syncLedState() {
  // Let RapidFlash (factory-reset hold) and ReadyOnce run to completion
  if (sLedCurrent == LedPattern::RapidFlash) return;
  if (sLedCurrent == LedPattern::ReadyOnce)  return;

  bool bothReady = hidShieldReady() && sTivoReady;
  bool shieldDone = shieldDoneForTivo();

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
    ledSetBase(shieldDone ? LedPattern::DoubleFlash : LedPattern::SlowBlink);
  }

  sWasBothReady = bothReady;
}

// ============================================================
// Display helpers
// ============================================================

static void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
  Serial.println();
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
  memcpy(sLastData, pData, length < sizeof(sLastData) ? length : sizeof(sLastData));
  sLastLen = length;

  // ---- Print to serial ----
  Serial.print("BTN raw=[");
  for (size_t i = 0; i < length; i++) Serial.printf("%02X ", pData[i]);
  Serial.print("] ");

  if (length == 8) {
    bool found = false;
    for (size_t i = 2; i < 8; i++) {
      if (pData[i] == 0) continue;
      const char* n = lookupKey(pData[i]);
      Serial.printf("=> Key: %s\r\n", n ? n : "?");
      found = true;
      break;
    }
    if (!found) Serial.println("=> (no key)");
  } else if (length >= 2) {
    uint16_t usage = pData[0] | (pData[1] << 8);
    const char* n = lookupUsage(usage);
    Serial.printf("=> 0x%04X  %s\r\n", usage, n ? n : "(unknown)");
  } else {
    Serial.println();
  }

  // ---- Translate and forward to Shield ----
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
    Serial.println("[Central] Connected to TiVo remote.");
  }

  void onDisconnect(NimBLEClient* client) override {
    Serial.println("[Central] Disconnected from TiVo remote.");
    pClient         = nullptr;
    targetDevice    = nullptr;
    sTivoConnecting = false;
    sTivoReady      = false;

    if (sHasBond) {
      Serial.println("[Central] Reconnecting in 3 s...");
      sReconnectAt = millis() + CFG_TIVO_RECONNECT_MS;
    } else {
      Serial.println("[Central] No bond — will scan when Shield is ready.");
      tryStartTiVoCentral();
    }
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    if (!desc->sec_state.encrypted) {
      Serial.println("[Central] Encryption failed. Disconnecting.");
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
    Serial.println("[Central] Encrypted / bonded.");

    NimBLEClient* c = NimBLEDevice::getClientByID(desc->conn_handle);
    if (c) {
      sBondedAddr = c->getPeerAddress();
      sHasBond    = true;
      sTivoPrefs.begin(CFG_NVS_TIVO_NS, false);
      sTivoPrefs.putString(CFG_NVS_TIVO_ADDR, sBondedAddr.toString().c_str());
      sTivoPrefs.end();
      Serial.printf("[Central] TiVo bond saved: %s\r\n", sBondedAddr.toString().c_str());
    }
  }
};

class AdvertisedCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    bool match = false;

    if (dev->haveServiceUUID() && dev->isAdvertisingService(HID_SERVICE))
      match = true;

    if (dev->haveName()) {
      std::string n = dev->getName();
      if (n.find("TiVo")   != std::string::npos ||
          n.find("TIVO")   != std::string::npos ||
          n.find("Remote") != std::string::npos ||
          n.find("UE878")  != std::string::npos ||
          n.find("R37023") != std::string::npos)
        match = true;
    }

    if (sHasBond && dev->getAddress() == sBondedAddr)
      match = true;

    if (!match) return;

    Serial.printf("[Central] Found remote: %s\r\n", dev->toString().c_str());
    NimBLEDevice::getScan()->stop();
    targetDevice = new NimBLEAdvertisedDevice(*dev);
    doConnect    = true;
  }
};

// ---- Subscribe to all notifiable 2A4D characteristics ----

bool subscribeReports(NimBLERemoteService* hid) {
  bool any = false;
  auto chars = hid->getCharacteristics(true);
  for (auto& c : *chars) {
    if (!c->getUUID().equals(REPORT_CHAR)) continue;
    if (c->canRead()) {
      std::string v = c->readValue();
      Serial.printf("[Central] Report 2A4D initial len=%u: ", v.length());
      printHex((const uint8_t*)v.data(), v.length());
    }
    if (c->canNotify()) {
      if (c->subscribe(true, hidNotifyCallback)) {
        Serial.println("[Central] Subscribed to 2A4D.");
        any = true;
      } else {
        Serial.println("[Central] Subscribe to 2A4D failed.");
      }
    }
  }
  return any;
}

// ---- Connect and set up HID subscriptions ----

bool connectAndSubscribe(NimBLEAddress addr) {
  sTivoConnecting = true;
  sTivoReady      = false;
  Serial.printf("[Central] Connecting to %s ...\r\n", addr.toString().c_str());

  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallbacks(), false);
  pClient->setConnectionParams(CFG_CONN_MIN_INTERVAL, CFG_CONN_MAX_INTERVAL,
                               CFG_CONN_LATENCY, CFG_CONN_TIMEOUT);
  pClient->setConnectTimeout(CFG_CONNECT_TIMEOUT_S);

  if (!pClient->connect(addr)) {
    Serial.println("[Central] Connection failed.");
    NimBLEDevice::deleteClient(pClient);
    pClient         = nullptr;
    sTivoConnecting = false;
    return false;
  }

  NimBLERemoteService* hid = pClient->getService(HID_SERVICE);
  if (!hid) {
    Serial.println("[Central] No HID service found.");
    pClient->disconnect();
    return false;
  }

  NimBLERemoteCharacteristic* rmap = hid->getCharacteristic(REPORT_MAP_CHAR);
  if (rmap && rmap->canRead()) {
    std::string m = rmap->readValue();
    Serial.printf("[Central] Report Map len=%u\r\n", m.length());
    printHex((const uint8_t*)m.data(), m.length());
  }

  NimBLERemoteCharacteristic* proto = hid->getCharacteristic(PROTOCOL_MODE_CHAR);
  if (proto && proto->canWrite()) {
    uint8_t mode = 0x01;
    proto->writeValue(&mode, 1, false);
  }

  if (!subscribeReports(hid)) {
    Serial.println("[Central] No reports subscribed.");
    return false;
  }

  sTivoConnecting = false;
  sTivoReady      = true;
  Serial.println("[Central] Ready — forwarding to Shield.");
  return true;
}

// ============================================================
// Bond management helpers (called from buttonTick)
// ============================================================

static void forgetTiVo() {
  if (pClient) pClient->disconnect();
  if (sHasBond) NimBLEDevice::deleteBond(sBondedAddr);
  sTivoPrefs.begin(CFG_NVS_TIVO_NS, false);
  sTivoPrefs.remove(CFG_NVS_TIVO_ADDR);
  sTivoPrefs.end();
  sHasBond     = false;
  sReconnectAt = 0;
  doConnect    = false;
  Serial.println("[BTN] TiVo bond cleared — will scan when Shield is ready.");
  tryStartTiVoCentral();
}

static void forgetShield() {
  hidForgetShield(sBondedAddr, sHasBond);
  Serial.println("[BTN] Shield bond cleared — re-advertising.");
}

static void factoryReset() {
  forgetTiVo();
  forgetShield();
  keymapClearCustom();
  Serial.println("[BTN] Factory reset complete — all bonds and keymap cleared.");
}

// ============================================================
// Boot button handler (non-blocking, called every loop iteration)
// Cumulative hold thresholds: 3 s → 6 s → 10 s
// ============================================================

static void buttonTick() {
  static unsigned long sBtnAt    = 0;
  static uint8_t       sBtnLevel = 0;   // 0=idle, 1=3s done, 2=6s done, 3=10s done
  static bool          sBtnHeld  = false;

  bool pressed = (digitalRead(CFG_BOOT_BTN_PIN) == LOW);

  if (pressed) {
    if (!sBtnHeld) {
      sBtnAt    = millis();
      sBtnLevel = 0;
      sBtnHeld  = true;
    }

    unsigned long held = millis() - sBtnAt;

    if (held >= CFG_BTN_FACTORY_MS && sBtnLevel < 3) {
      sBtnLevel = 3;
      factoryReset();
      ledForce(LedPattern::RapidFlash);
      sFactoryAnimUntil = millis() + CFG_FACTORY_ANIM_MS;
      Serial.println("[BTN] 10 s: factory reset.");

    } else if (held >= CFG_BTN_SHIELD_MS && sBtnLevel < 2) {
      sBtnLevel = 2;
      forgetShield();
      ledForce(LedPattern::SlowBlink);    // no Shield bond → slow blink
      Serial.println("[BTN] 6 s: Shield bond forgotten.");

    } else if (held >= CFG_BTN_TIVO_MS && sBtnLevel < 1) {
      sBtnLevel = 1;
      forgetTiVo();
      ledForce(LedPattern::DoubleFlash);  // Shield bonded, no TiVo → double-flash
      Serial.println("[BTN] 3 s: TiVo bond forgotten.");
    }

  } else {
    if (sBtnHeld) {
      if (sBtnLevel == 3) {
        if (sFactoryAnimUntil) {
          // Timer not yet fired — cancel it and start ReadyOnce → SlowBlink immediately
          sFactoryAnimUntil = 0;
          sLedAfterOnce = LedPattern::SlowBlink;
          sLedBase      = LedPattern::ReadyOnce;
          sLedCurrent   = LedPattern::ReadyOnce;
          sLedStep      = 0;
          sLedAt        = millis();
        }
        // If sFactoryAnimUntil == 0, the timer already fired — ReadyOnce is already running
      }
    }
    sBtnHeld  = false;
    sBtnAt    = 0;
  }
}

// ============================================================
// Arduino setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);

  // WS2812 RGB LED — yellow while BLE stack initialises (init before delay so
  // the LED is not dark for the first second while USB serial comes up)
  sLed.begin();
  sLed.setBrightness(CFG_LED_BRIGHTNESS);
  sLed.setPixelColor(0, kColBoot);
  sLed.show();
  sLedAt = millis();

  delay(1000);

  Serial.println("\n=== TiVo BLE HID Translator ===");

  // Boot button
  pinMode(CFG_BOOT_BTN_PIN, INPUT_PULLUP);

  NimBLEDevice::init(CFG_BLE_DEVICE_NAME);
  NimBLEDevice::setSecurityAuth(true, false, true);  // bond=true, MITM=false, SC=true
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  hidPeripheralInit();
  keymapInit();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvertisedCallbacks(), false);
  scan->setInterval(CFG_SCAN_INTERVAL);
  scan->setWindow(CFG_SCAN_WINDOW);
  scan->setActiveScan(true);

  // Restore TiVo bond address from NVS
  sTivoPrefs.begin(CFG_NVS_TIVO_NS, true);
  String addrStr = sTivoPrefs.getString(CFG_NVS_TIVO_ADDR, "");
  sTivoPrefs.end();

  if (addrStr.length() > 0) {
    sBondedAddr = NimBLEAddress(addrStr.c_str());
    sHasBond    = true;
    Serial.printf("[Central] Stored TiVo bond: %s (connects after Shield ready).\r\n",
                  addrStr.c_str());
  } else {
    Serial.println("[Central] No TiVo bond. Pair Shield first; then TiVo + Back.");
  }

  // Load Shield bond so the peripheral knows which bond to preserve
  hidLoadShieldBond(sBondedAddr, sHasBond);
}

void loop() {
  // ---- LED and button (non-blocking) ----
  buttonTick();
  ledTick();

  // RapidFlash timer: transition to ReadyOnce → SlowBlink after CFG_FACTORY_ANIM_MS
  if (sFactoryAnimUntil && millis() >= sFactoryAnimUntil) {
    sFactoryAnimUntil = 0;
    sLedAfterOnce = LedPattern::SlowBlink;
    sLedBase      = LedPattern::ReadyOnce;
    sLedCurrent   = LedPattern::ReadyOnce;
    sLedStep      = 0;
    sLedAt        = millis();
  }

  syncLedState();
  tryStartTiVoCentral();

  // ---- Shield CCCD edge — schedule fast BLE params ----
  {
    bool isReady = hidShieldReady();
    if (isReady && !sWasShieldReady) {
      sShieldParamsAt = millis() + CFG_SHIELD_FAST_PARAMS_DELAY_MS;
      Serial.println("[HID] Shield CCCD confirmed — fast params in 1 s.");
    }
    sWasShieldReady = isReady;
  }

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
    } else if (targetDevice) {
      addr    = targetDevice->getAddress();
      hasAddr = true;
      delete targetDevice;
      targetDevice = nullptr;
    }

    if (hasAddr) {
      if (!connectAndSubscribe(addr)) {
        Serial.println("[Central] Connect failed — will retry scan when appropriate.");
        tryStartTiVoCentral();
      }
    }
  }

  delay(1);
}
