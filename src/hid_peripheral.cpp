#include "hid_peripheral.h"
#include <NimBLEDevice.h>
#include <Arduino.h>

// HID Report Descriptor
//   Report ID 1 — standard keyboard  (8 bytes: modifier, reserved, key[6])
//   Report ID 2 — consumer control   (2 bytes: usage LE16)
static const uint8_t kReportDesc[] = {
  // ---- Keyboard (Report ID 1) ----
  0x05, 0x01,         // Usage Page (Generic Desktop)
  0x09, 0x06,         // Usage (Keyboard)
  0xA1, 0x01,         // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    // Modifier byte
    0x05, 0x07,       //   Usage Page (Key Codes)
    0x19, 0xE0,       //   Usage Minimum (L-Ctrl)
    0x29, 0xE7,       //   Usage Maximum (R-GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1 bit)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    // Reserved byte
    0x75, 0x08,       //   Report Size (8 bits)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x01,       //   Input (Constant)
    // Key array
    0x75, 0x08,       //   Report Size (8 bits)
    0x95, 0x06,       //   Report Count (6)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0xFF,       //   Usage Maximum (255)
    0x81, 0x00,       //   Input (Data, Array)
  0xC0,               // End Collection

  // ---- Consumer Control (Report ID 2) ----
  0x05, 0x0C,         // Usage Page (Consumer)
  0x09, 0x01,         // Usage (Consumer Control)
  0xA1, 0x01,         // Collection (Application)
    0x85, 0x02,       //   Report ID (2)
    0x19, 0x00,       //   Usage Minimum (0)
    0x2A, 0xFF, 0x03, //   Usage Maximum (1023)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x03, //   Logical Maximum (1023)
    0x75, 0x10,       //   Report Size (16 bits)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x00,       //   Input (Data, Array)
  0xC0,               // End Collection
};

static NimBLEServer*         pServer              = nullptr;
static NimBLECharacteristic* pKbReport            = nullptr;
static NimBLECharacteristic* pCsReport            = nullptr;
static bool                  sShieldConn          = false;
static NimBLEAddress         sShieldAddr;
static bool                  sHasShieldAddr       = false;
static uint16_t              sShieldConnHandle    = BLE_HS_CONN_HANDLE_NONE;
// true between onConnect and the first CCCD write — Shield is doing service discovery
static bool                  sShieldNegotiating   = false;

// Log every CCCD write so we can confirm the Shield is enabling notifications
class ReportCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic* pChar,
                   ble_gap_conn_desc*    desc,
                   uint16_t             subValue) override {
    Serial.printf("[HID] CCCD write — subValue=0x%04X "
                  "(1=notify enabled, 0=disabled)\r\n", subValue);
    // First CCCD write means Android has finished service discovery
    if (subValue & 0x0001) sShieldNegotiating = false;
  }
};
static ReportCallbacks sReportCbs;

class PeriphCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, ble_gap_conn_desc* desc) override {
    sShieldConn         = true;
    sShieldNegotiating  = true;   // waiting for Android to write CCCDs
    sShieldAddr         = NimBLEAddress(desc->peer_id_addr);
    sHasShieldAddr      = true;
    sShieldConnHandle   = desc->conn_handle;
    Serial.printf("[HID] Shield connected: %s handle=%u\r\n",
                  sShieldAddr.toString().c_str(), sShieldConnHandle);
    NimBLEDevice::getAdvertising()->stop();
    // Connection params are updated after a 3 s delay via hidRequestFastParams()
    // called from loop() — doing it immediately here blocks Android's CCCD setup.
  }
  void onDisconnect(NimBLEServer* s) override {
    sShieldConn        = false;
    sShieldNegotiating = false;
    sShieldConnHandle  = BLE_HS_CONN_HANDLE_NONE;
    Serial.println("[HID] Shield disconnected — re-advertising.");
    NimBLEDevice::getAdvertising()->start();
  }
};

void hidPeripheralInit() {
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new PeriphCallbacks());

  // ---- HID Service 0x1812 ----
  NimBLEService* pHid = pServer->createService("1812");

  // Protocol Mode — boot(0) / report(1); we stay in report mode
  NimBLECharacteristic* pProto = pHid->createCharacteristic(
      "2A4E", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);
  uint8_t proto = 0x01;
  pProto->setValue(&proto, 1);

  // Report Map
  NimBLECharacteristic* pMap = pHid->createCharacteristic(
      "2A4B", NIMBLE_PROPERTY::READ);
  pMap->setValue(kReportDesc, sizeof(kReportDesc));

  // HID Information  [bcdHID=1.11, country=0, flags=NormallyConnectable]
  NimBLECharacteristic* pInfo = pHid->createCharacteristic(
      "2A4A", NIMBLE_PROPERTY::READ);
  uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x02};
  pInfo->setValue(hidInfo, sizeof(hidInfo));

  // HID Control Point
  NimBLECharacteristic* pCtrl = pHid->createCharacteristic(
      "2A4C", NIMBLE_PROPERTY::WRITE_NR);
  uint8_t ctrlVal = 0;
  pCtrl->setValue(&ctrlVal, 1);

  // Keyboard Input Report — Report ID 1, type Input (0x01)
  pKbReport = pHid->createCharacteristic(
      "2A4D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pKbReport->setCallbacks(&sReportCbs);
  {
    NimBLEDescriptor* d = pKbReport->createDescriptor(
        "2908", NIMBLE_PROPERTY::READ, 2);
    uint8_t ref[] = {0x01, 0x01};
    d->setValue(ref, sizeof(ref));
  }
  {
    uint8_t zeros[8] = {};
    pKbReport->setValue(zeros, sizeof(zeros));
  }

  // Consumer Input Report — Report ID 2, type Input (0x01)
  pCsReport = pHid->createCharacteristic(
      "2A4D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pCsReport->setCallbacks(&sReportCbs);
  {
    NimBLEDescriptor* d = pCsReport->createDescriptor(
        "2908", NIMBLE_PROPERTY::READ, 2);
    uint8_t ref[] = {0x02, 0x01};
    d->setValue(ref, sizeof(ref));
  }
  {
    uint8_t zeros[2] = {};
    pCsReport->setValue(zeros, sizeof(zeros));
  }

  pHid->start();

  // ---- Battery Service 0x180F ----
  NimBLEService* pBatt = pServer->createService("180F");
  NimBLECharacteristic* pBattLvl = pBatt->createCharacteristic(
      "2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  uint8_t batt = 100;
  pBattLvl->setValue(&batt, 1);
  pBatt->start();

  // ---- Device Information Service 0x180A ----
  NimBLEService* pDev = pServer->createService("180A");
  NimBLECharacteristic* pPnp = pDev->createCharacteristic(
      "2A50", NIMBLE_PROPERTY::READ);
  // vendor_id_source=BT(1), vendor=0xFFFF, product=0x0001, version=0x0001
  uint8_t pnp[] = {0x01, 0xFF, 0xFF, 0x01, 0x00, 0x01, 0x00};
  pPnp->setValue(pnp, sizeof(pnp));
  pDev->start();

  // ---- Advertising ----
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID("1812");
  pAdv->setAppearance(0x03C1);  // Keyboard
  pAdv->setScanResponse(true);
  pAdv->start();

  Serial.println("[HID] Peripheral ready — advertising as 'TiVo-Bridge'.");
}

bool hidPeripheralConnected() { return sShieldConn; }

// Returns a string token describing the current Shield connection phase.
const char* hidGetShieldState() {
  if (sShieldConn) {
    return sShieldNegotiating ? "negotiating" : "ready";
  }
  return sHasShieldAddr ? "adv_bonded" : "advertising";
}

// Called from loop() 3 s after Shield connects to request faster BLE intervals.
// Doing this immediately in onConnect blocks Android's CCCD setup.
void hidRequestFastParams() {
  if (!sShieldConn || sShieldConnHandle == BLE_HS_CONN_HANDLE_NONE) return;
  pServer->updateConnParams(sShieldConnHandle, 6, 12, 0, 51);
  Serial.println("[HID] Requested fast conn params (7.5-15 ms) from Shield.");
}

String hidGetShieldAddr() {
  if (!sHasShieldAddr) return "";
  return String(sShieldAddr.toString().c_str());
}

void hidLoadShieldBond(NimBLEAddress tivoAddr, bool hasTivo) {
  int n = NimBLEDevice::getNumBonds();
  for (int i = 0; i < n; i++) {
    NimBLEAddress addr = NimBLEDevice::getBondedAddress(i);
    if (hasTivo && addr == tivoAddr) continue;  // skip TiVo bond
    sShieldAddr    = addr;
    sHasShieldAddr = true;
    Serial.printf("[HID] Found stored Shield bond: %s\r\n", addr.toString().c_str());
    return;
  }
  Serial.println("[HID] No stored Shield bond found.");
}

void hidSendKeyboard(uint8_t modifier, uint8_t keycode) {
  if (!pKbReport || !sShieldConn) return;
  uint8_t r[8] = {modifier, 0x00, keycode, 0, 0, 0, 0, 0};
  pKbReport->setValue(r, sizeof(r));
  pKbReport->notify();
  Serial.printf("[HID] KB notify mod=0x%02X key=0x%02X\r\n", modifier, keycode);
}

void hidReleaseKeyboard() {
  if (!pKbReport) return;
  uint8_t r[8] = {};
  pKbReport->setValue(r, sizeof(r));
  if (sShieldConn) pKbReport->notify();
}

void hidSendKeyboardRaw(const uint8_t* report8) {
  if (!pKbReport || !sShieldConn) return;
  pKbReport->setValue(report8, 8);
  pKbReport->notify();
}

void hidSendConsumer(uint16_t usage) {
  if (!pCsReport || !sShieldConn) return;
  uint8_t r[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
  pCsReport->setValue(r, sizeof(r));
  pCsReport->notify();
  Serial.printf("[HID] CSM notify 0x%04X\r\n", usage);
}

void hidReleaseConsumer() {
  if (!pCsReport) return;
  uint8_t r[2] = {};
  pCsReport->setValue(r, sizeof(r));
  if (sShieldConn) pCsReport->notify();
}

// Rebuild and start advertising with the correct HID service data.
// Called after bond deletion or TX power change so the advertising PDU is
// always fresh — NimBLE can drop it after stack-level resets.
static void restartAdvertising();   // forward declaration for hidForgetShield
void hidRestartAdvertising() { restartAdvertising(); }

static void restartAdvertising() {
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->stop();
  delay(100);
  // Do NOT call addServiceUUID/setAppearance here — those are already configured
  // in hidPeripheralInit() and persist in the NimBLEAdvertising object after stop().
  // Calling addServiceUUID again would add a duplicate 0x1812 entry, producing a
  // malformed advertising PDU that Android TV silently ignores.
  pAdv->setMinInterval(32);   // 32 × 0.625 ms = 20 ms  — fast discovery
  pAdv->setMaxInterval(64);   // 64 × 0.625 ms = 40 ms
  pAdv->start();
  Serial.printf("[HID] Advertising restarted — isAdvertising=%d\r\n",
                pAdv->isAdvertising() ? 1 : 0);
}

void hidForgetShield(NimBLEAddress tivoAddr, bool hasTivo) {
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->stop();

  // Disconnect the Shield using its known address (avoids wrong-handle bug)
  if (sShieldConn && pServer && sHasShieldAddr) {
    NimBLEConnInfo info = pServer->getPeerInfo(sShieldAddr);
    uint16_t handle = info.getConnHandle();
    if (handle != BLE_HS_CONN_HANDLE_NONE) {
      pServer->disconnect(handle);
      Serial.printf("[HID] Disconnecting Shield handle %u\r\n", handle);
    }
    sShieldConn        = false;
    sShieldNegotiating = false;
    sShieldConnHandle  = BLE_HS_CONN_HANDLE_NONE;
  }

  // Delete all BLE bonds except the TiVo remote's bond
  int n = NimBLEDevice::getNumBonds();
  int deleted = 0;
  for (int i = n - 1; i >= 0; i--) {
    NimBLEAddress addr = NimBLEDevice::getBondedAddress(i);
    if (hasTivo && addr == tivoAddr) continue;
    NimBLEDevice::deleteBond(addr);
    Serial.printf("[HID] Deleted bond: %s\r\n", addr.toString().c_str());
    deleted++;
  }
  sHasShieldAddr = false;
  Serial.printf("[HID] Shield bond cleared (%d removed).\r\n", deleted);

  delay(300);  // let BLE stack settle after bond deletion
  restartAdvertising();
  Serial.println("[HID] Ready — pair from Shield now.");
}
