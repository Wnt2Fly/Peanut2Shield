#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "keymap.h"
#include "hid_peripheral.h"
#include "web_config.h"

// ---- Central (TiVo remote) state ----
static NimBLEClient*           pClient      = nullptr;
static NimBLEAdvertisedDevice* targetDevice = nullptr;
static bool                    doConnect    = false;
static NimBLEAddress           sBondedAddr;
static bool                    sHasBond     = false;
static unsigned long           sReconnectAt = 0;

// Fine-grained connection state for the live status UI
// (read from the HTTP task; written from loop/callbacks — simple bools are safe)
static bool sTivoConnecting = false;  // true while connectAndSubscribe() is running
static bool sTivoReady      = false;  // true once subscribed and forwarding

static const NimBLEUUID HID_SERVICE("1812");
static const NimBLEUUID REPORT_CHAR("2A4D");
static const NimBLEUUID REPORT_MAP_CHAR("2A4B");
static const NimBLEUUID HID_INFO_CHAR("2A4A");
static const NimBLEUUID PROTOCOL_MODE_CHAR("2A4E");

// NVS key for the bonded TiVo remote address
static Preferences sTivoPrefs;

// ---- Display helpers ----

static void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
  Serial.println();
}

// TiVo-specific button name overrides (physical label differs from HID usage name)
static const struct { uint16_t code; const char* name; } kTiVoNames[] = {
  { 0x009C, "Chan Up" },
  { 0x009D, "Chan Down" },
  { 0x0082, "Input" },
  { 0x008D, "Guide" },
  { 0x003D, "TiVo" },
  { 0xCE00, "Skip" },
  { 0x003E, "Live TV" },
  { 0x01C8, "Netflix" },
  { 0x0209, "Info" },
};

// Generic HID Consumer Control usage names
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

// ---- Activity LED ----
// GPIO 8 = built-in blue LED on ESP32-C3 Super Mini (active HIGH).
// Change to LOW if your board has an active-low LED.
#define LED_PIN     8
#define LED_ON      LOW   // ESP32-C3 Super Mini built-in LED is active-low
#define LED_OFF     HIGH

// ---- HID notify callback (central side) ----

// Dedup state — intentionally NOT cleared on key-up to absorb the TiVo's
// rapid keydown→keyup→keydown bounce (auto-repeat initiation).
static uint8_t       sLastData[20];
static size_t        sLastLen           = 0;
static bool          sKeyIsDown         = false;  // true while the key is physically held
static unsigned long sKeyDownAt         = 0;      // millis() when last keydown was forwarded
static unsigned long sKbReleaseAt       = 0;      // non-zero while keyboard pulse-release pending
static unsigned long sLedOffAt          = 0;      // non-zero while LED flash is active
static unsigned long sShieldParamsAt    = 0;      // non-zero: fire hidRequestFastParams at time
static bool          sWasShieldConn     = false;

// Minimum gap (ms) before the same code is accepted again after key-up.
// Absorbs the TiVo's ~20-40 ms keydown→keyup→keydown bounce without
// blocking intentional rapid taps (typical human double-tap ~200 ms+).
static constexpr unsigned long kBounceMsGuard = 80;

void hidNotifyCallback(
    NimBLERemoteCharacteristic* pChar,
    uint8_t* pData, size_t length, bool isNotify)
{
  bool allZero = true;
  for (size_t i = 0; i < length; i++) {
    if (pData[i] != 0x00) { allZero = false; break; }
  }

  if (allZero) {
    // Only treat as key-up when the length matches the last non-zero report.
    // The TiVo has 32-byte keyboard characteristics that fire all-zeros constantly
    // while consumer (4-byte) keys are held. If we let those reset sKeyIsDown the
    // hold-repeat guard breaks, causing the second consumer characteristic (0x10)
    // to slip through as a duplicate keydown on every single press.
    if (length == sLastLen) {
      sKeyIsDown = false;
      hidReleaseConsumer();
    }
    return;
  }

  unsigned long now = millis();

  // Suppress hold-repeat: same data while key is physically held
  if (sKeyIsDown && length == sLastLen && memcmp(pData, sLastData, length) == 0) return;

  // Suppress bounce: same data arriving within kBounceMsGuard ms of a key-up
  if (!sKeyIsDown && length == sLastLen &&
      memcmp(pData, sLastData, length) == 0 &&
      (now - sKeyDownAt) < kBounceMsGuard) {
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
    // HID keyboard report: scan keycodes in slots 2-7
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
  // Keyboard translations: forced 30 ms pulse so Shield never sees a held key.
  // Consumer pass-throughs: natural timing — held until TiVo's own key-up fires.
  if (length == 8) {
    hidSendKeyboardRaw(pData);
    sKbReleaseAt = millis() + 30;
  } else if (length >= 2) {
    uint16_t usage = pData[0] | (pData[1] << 8);
    OutputType outType;
    uint16_t   outCode;
    keymapLookupConsumer(usage, outType, outCode);
    if (outType == OutputType::Keyboard) {
      hidSendKeyboard(0x00, (uint8_t)outCode);
      sKbReleaseAt = millis() + 30;
    } else {
      hidSendConsumer(outCode);
      // No forced release — TiVo key-up notification triggers hidReleaseConsumer()
    }
  }

  // Flash activity LED for 80 ms on every forwarded keydown
  digitalWrite(LED_PIN, LED_ON);
  sLedOffAt = millis() + 80;
}

// ---- Central BLE callbacks ----

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* client) override {
    Serial.println("[Central] Connected to TiVo remote.");
  }

  void onDisconnect(NimBLEClient* client) override {
    Serial.println("[Central] Disconnected from TiVo remote.");
    pClient          = nullptr;
    targetDevice     = nullptr;
    sTivoConnecting  = false;
    sTivoReady       = false;

    if (sHasBond) {
      Serial.println("[Central] Reconnecting in 3 s...");
      sReconnectAt = millis() + 3000;
    } else {
      Serial.println("[Central] No bond — restarting scan.");
      NimBLEDevice::getScan()->start(0, nullptr, false);
    }
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    if (!desc->sec_state.encrypted) {
      Serial.println("[Central] Encryption failed. Disconnecting.");
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
    Serial.println("[Central] Encrypted / bonded.");

    // Persist the TiVo address separately so it survives alongside Shield bonds
    NimBLEClient* c = NimBLEDevice::getClientByID(desc->conn_handle);
    if (c) {
      sBondedAddr = c->getPeerAddress();
      sHasBond    = true;
      sTivoPrefs.begin("tivo", false);
      sTivoPrefs.putString("addr", sBondedAddr.toString().c_str());
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
  pClient->setConnectionParams(6, 12, 0, 51);
  pClient->setConnectTimeout(10);

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

  // Read and dump Report Map for reference
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

// ---- Status accessors (called from web UI) ----

bool tivoHasBond()    { return sHasBond; }
bool tivoIsConnected(){ return pClient != nullptr && pClient->isConnected(); }
String tivoGetAddr()  {
  if (!sHasBond) return "";
  return String(sBondedAddr.toString().c_str());
}

// Returns a string token describing the current TiVo connection phase.
// Called from the HTTP task — reads simple booleans, no locking needed.
const char* tivoGetState() {
  if (sTivoReady)      return "ready";
  if (sTivoConnecting) return "connecting";
  if (sHasBond)        return "reconnecting";
  return "scanning";
}

// ---- Re-pair helpers (called from web UI) ----

void repairTiVo() {
  Serial.println("[Central] Re-pair requested — forgetting TiVo bond.");

  // Disconnect if currently connected
  if (pClient) {
    pClient->disconnect();
    // onDisconnect callback will null pClient and clear targetDevice
  }

  // Delete NimBLE bond
  if (sHasBond) {
    NimBLEDevice::deleteBond(sBondedAddr);
  }

  // Clear stored address from NVS
  sTivoPrefs.begin("tivo", false);
  sTivoPrefs.remove("addr");
  sTivoPrefs.end();

  sHasBond     = false;
  sReconnectAt = 0;
  doConnect    = false;

  Serial.println("[Central] TiVo bond cleared. Put remote in pairing mode: TiVo + Back.");
  NimBLEDevice::getScan()->start(0, nullptr, false);
}

void repairShield() {
  Serial.println("[HID] Re-pair Shield requested.");
  hidForgetShield(sBondedAddr, sHasBond);
}

// ---- Arduino setup / loop ----

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== TiVo BLE HID Translator ===");

  // Activity LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  NimBLEDevice::init("TiVo-Bridge");
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  // Start BLE peripheral (advertise HID keyboard to Shield)
  hidPeripheralInit();

  // Load translation keymap from NVS
  keymapInit();

  // Start WiFi AP + web config server
  webConfigInit();

  // Set up central scanner
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvertisedCallbacks(), false);
  scan->setInterval(100);
  scan->setWindow(50);
  scan->setActiveScan(true);

  // Restore TiVo bond address (stored separately to avoid confusion with Shield bond)
  sTivoPrefs.begin("tivo", true);
  String addrStr = sTivoPrefs.getString("addr", "");
  sTivoPrefs.end();

  if (addrStr.length() > 0) {
    sBondedAddr = NimBLEAddress(addrStr.c_str());
    sHasBond    = true;
    Serial.printf("[Central] Stored TiVo bond: %s — connecting...\r\n", addrStr.c_str());
    doConnect = true;
  } else {
    Serial.println("[Central] No TiVo bond. Put remote in pairing mode: TiVo + Back.");
    scan->start(0, nullptr, false);
  }

  // Pre-populate Shield bond state from NimBLE's stored bonds so the web UI
  // shows "Bonded — advertising" immediately on reboot rather than "Not paired".
  hidLoadShieldBond(sBondedAddr, sHasBond);
}

void loop() {
  // Service HTTP requests
  webConfigLoop();

  // Keyboard pulse-release for translated keys
  if (sKbReleaseAt && millis() >= sKbReleaseAt) {
    sKbReleaseAt = 0;
    hidReleaseKeyboard();
  }

  // Activity LED off timer
  if (sLedOffAt && millis() >= sLedOffAt) {
    sLedOffAt = 0;
    digitalWrite(LED_PIN, LED_OFF);
  }

  // Detect Shield connect edge and schedule a delayed connection-param update.
  // We cannot call updateConnParams immediately in onConnect because Android
  // has not finished writing CCCDs yet; 3 s gives it plenty of time.
  {
    bool isConn = hidPeripheralConnected();
    if (isConn && !sWasShieldConn) {
      sShieldParamsAt = millis() + 3000;
    }
    sWasShieldConn = isConn;
  }

  // Fire the delayed connection-param request
  if (sShieldParamsAt && millis() >= sShieldParamsAt) {
    sShieldParamsAt = 0;
    hidRequestFastParams();
  }

  // Timed reconnect after TiVo disconnect
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
        Serial.println("[Central] Connect failed — scanning...");
        NimBLEDevice::getScan()->start(0, nullptr, false);
      }
    }
  }

  delay(1);
}
