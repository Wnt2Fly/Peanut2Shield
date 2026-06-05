#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <NimBLEDevice.h>

// Initialise the BLE GATT server (HID + Battery + DevInfo services) and
// start advertising.  Must be called after NimBLEDevice::init().
void hidPeripheralInit();

bool hidPeripheralConnected();

// Returns a string token for the current Shield phase:
// "advertising" | "adv_bonded" | "negotiating" | "ready"
const char* hidGetShieldState();

// True only when the Shield is connected AND has completed CCCD writes
// (i.e. sShieldNegotiating is false). Safe to start WiFi at this point.
bool hidShieldReady();

// Request faster BLE connection parameters from the Shield.
// Must be called at least 3 s after connection to avoid disrupting CCCD setup.
void hidRequestFastParams();

// Rebuild and restart advertising (used after TX power change or bond deletion).
void hidRestartAdvertising();

// Pause peripheral advertising while the TiVo central is connecting/pairing.
void hidPauseForTivoCentral();

// Resume advertising after TiVo central work finishes or aborts.
void hidResumeAfterTivoCentral();

// Send a single key-press on the keyboard report, no modifier.
void hidSendKeyboard(uint8_t modifier, uint8_t keycode);

// Release all keyboard keys.
void hidReleaseKeyboard();

// Forward a raw 8-byte HID keyboard report verbatim.
void hidSendKeyboardRaw(const uint8_t* report8);

// Send a 16-bit consumer usage code.
void hidSendConsumer(uint16_t usage);

// Send consumer release (all-zeros).
void hidReleaseConsumer();

// Disconnect Shield (if connected), delete its BLE bond, and re-advertise.
// Pass the TiVo bond address so it is preserved.
void hidForgetShield(NimBLEAddress tivoAddr, bool hasTivo);

// Returns the Shield's BLE address as a string, or "" if it has never connected.
String hidGetShieldAddr();

// Returns true if a Shield bond address is known (connected or loaded from NVS).
// Cheaper than hidGetShieldAddr() — avoids String allocation.
bool hidHasShieldBond();

// Scan NimBLE bond store on boot and pre-populate Shield address if a stored
// bond exists (any bond that is not the TiVo bond). Also restores from NVS.
void hidLoadShieldBond(NimBLEAddress tivoAddr, bool hasTivo);

// Save Shield address to NVS after pairing / CCCD setup completes.
void hidPersistShieldBond();

#if CFG_SHIELD_DEBUG
// Log ESP reset reason once at boot (call after Serial.begin).
void hidShieldDebugLogBootReason();

// Periodic Shield/TiVo/advertising heartbeat; call every loop().
void hidShieldDebugTick(bool tivoConnected, bool tivoReady, bool tivoCentralBusy);
#endif
