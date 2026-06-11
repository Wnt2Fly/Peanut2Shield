#pragma once

// =============================================================================
// config.h - All compile-time constants for TiVo BLE HID Translator
// Edit this file to customise behaviour without touching any other source file.
// =============================================================================

// -----------------------------------------------------------------------------
// BLE device identity
// -----------------------------------------------------------------------------

// Name broadcast by the ESP32 over BLE (visible during Shield pairing scan)
#define CFG_BLE_DEVICE_NAME  "Peanut2Shield"

// Firmware version (serial banner, README, USB kit VERSION.txt)
#define CFG_FIRMWARE_VERSION  "v1.06"

// BLE appearance value advertised to Android TV.
// 0x0180 = Generic Remote Control - avoids the PIN-entry flow triggered by
// the Keyboard appearance (0x03C1) on Android TV.
#define CFG_BLE_APPEARANCE   0x0180

// -----------------------------------------------------------------------------
// BLE advertising intervals (units of 0.625 ms)
// Fast intervals improve discoverability during pairing.
// -----------------------------------------------------------------------------

#define CFG_ADV_MIN_INTERVAL  32   // 32 x 0.625 ms = 20 ms
#define CFG_ADV_MAX_INTERVAL  64   // 64 x 0.625 ms = 40 ms

// -----------------------------------------------------------------------------
// BLE connection parameters
// Applied on both the Central (TiVo) and Peripheral (Shield) links.
// Interval units = 1.25 ms; timeout units = 10 ms.
// -----------------------------------------------------------------------------

#define CFG_CONN_MIN_INTERVAL  6   //  6 x 1.25 ms =  7.5 ms
#define CFG_CONN_MAX_INTERVAL  12  // 12 x 1.25 ms = 15.0 ms
#define CFG_CONN_LATENCY       0   // peripheral may not skip any events
#define CFG_CONN_TIMEOUT       51  // 51 x 10 ms = 510 ms — Shield peripheral link

// TiVo central link: longer supervision timeout (510 ms is too short — remote drops).
#define CFG_TIVO_CONN_MIN_INTERVAL  12  // 15 ms
#define CFG_TIVO_CONN_MAX_INTERVAL    24  // 30 ms
#define CFG_TIVO_CONN_LATENCY          0
#define CFG_TIVO_CONN_TIMEOUT        400  // 400 x 10 ms = 4 s

// Seconds before a central connection attempt is abandoned
#define CFG_CONNECT_TIMEOUT_S  10

// -----------------------------------------------------------------------------
// BLE scan parameters (Central side - scanning for the TiVo remote)
// Low duty cycle (~15%) leaves airtime for the peripheral advertising.
// -----------------------------------------------------------------------------

#define CFG_SCAN_INTERVAL  200  // 200 x 0.625 ms = 125 ms scan period
#define CFG_SCAN_WINDOW     30  //  30 x 0.625 ms = 18.75 ms active per period

// -----------------------------------------------------------------------------
// Timing delays (milliseconds)
// -----------------------------------------------------------------------------

// Duration of the forced key-release pulse sent after a keyboard-translated
// keydown. Prevents the Shield from auto-repeating F-key / ESC mappings.
#define CFG_KB_PULSE_MS  30

// How long the activity LED stays lit after each forwarded keydown event.
#define CFG_LED_FLASH_MS  80

// Delay after TiVo disconnects before attempting to reconnect.
#define CFG_TIVO_RECONNECT_MS  3000

// Delay before retrying TiVo connect/scan after a failed attempt.
#define CFG_TIVO_RETRY_MS  2000

// Pause after BLE connect before calling secureConnection (lets link stabilize).
#define CFG_TIVO_SECURE_DELAY_MS  400

// Connection must stay up this long before the TiVo bond is saved / trusted.
#define CFG_TIVO_BOND_MIN_UP_MS  5000

// Wait after Shield is ready before starting TiVo scan/connect (lets CCCD + conn params settle).
#define CFG_TIVO_POST_SHIELD_MS  1500

// After reboot, keep advertising for this long before TiVo central reconnects (Shield bond in NVS).
// TiVo connect pauses peripheral advertising; without this window Shield often never reconnects.
#define CFG_SHIELD_RECONNECT_BOOT_MS  8000

// Delay after the Shield's first CCCD write before requesting fast BLE params.
// Gives the BLE stack time to finish service discovery before changing intervals.
#define CFG_SHIELD_FAST_PARAMS_DELAY_MS  1000

// Settling delay inside restartAdvertising() after stop().
#define CFG_ADV_RESTART_SETTLE_MS  100

// Settling delay in hidForgetShield() after bond deletion, before re-advertising.
#define CFG_BOND_DELETE_SETTLE_MS  300

// -----------------------------------------------------------------------------
// Debug: TiVo-only test mode (set to 0 for normal operation)
// When 1: pretends Shield pairing is complete — starts TiVo scan/connect at boot
// without waiting for Shield CCCD. Keys log on serial; HID forward only if Shield
// is actually connected. Reflash after changing.
// -----------------------------------------------------------------------------
#define CFG_DEBUG_TIVO_ONLY  0

// Extra serial logging for Shield connect/disconnect and periodic link health.
// Set to 0 to disable heartbeat and [HID-DBG] lines (normal [HID] lines remain).
#define CFG_SHIELD_DEBUG              1
#define CFG_SHIELD_DEBUG_HEARTBEAT_MS  30000

// -----------------------------------------------------------------------------
// Duplicate / bounce suppression
// The TiVo remote has two 4-byte consumer characteristics (IDs 0x0C and 0x10).
// When one fires a keydown the other simultaneously fires all-zeros (idle value).
// -----------------------------------------------------------------------------

// Ignore same-length all-zero notifications arriving within this window of a
// keydown. They are the other characteristic's idle report, not a true key-up.
#define CFG_ALL_ZERO_GUARD_MS  50

// Bounce guard for action buttons.
// Absorbs the TiVo's rapid keydown->keyup->keydown auto-repeat initiation.
#define CFG_BOUNCE_GUARD_ACTION_MS  0
// -----------------------------------------------------------------------------
// Hardware: Activity LED  (Waveshare ESP32-S3-Zero - WS2812 RGB LED on GPIO 21)
// Driven via Adafruit_NeoPixel (NEO_RGB channel order in Peanut2Shield.cpp).
// -----------------------------------------------------------------------------

#define CFG_LED_PIN        21   // WS2812 serial data line
#define CFG_LED_BRIGHTNESS 15   // 0-255 global brightness scaling (applied by NeoPixel driver)

// LED colours — R, G, B at full (255) scale.
// All channels are divided by CFG_LED_BRIGHTNESS/255 at runtime,
// so change CFG_LED_BRIGHTNESS to make everything brighter/dimmer uniformly.
#define CFG_LED_COLOR_BOOT      255, 180,   0   // yellow  — boot blink (BLE init)
#define CFG_LED_COLOR_SHIELD    180,   0, 255   // purple  — advertising, waiting for Shield
#define CFG_LED_COLOR_TIVO      255,  42,   0   // deep orange — waiting for TiVo remote
#define CFG_LED_COLOR_READY       0, 255,   0   // green   — both devices connected
#define CFG_LED_COLOR_RESET     255,   0,   0   // red     — factory-reset hold in progress
#define CFG_LED_COLOR_ACTIVITY  255, 255, 255   // white   — button forwarded to Shield

// -----------------------------------------------------------------------------
// LED pattern timing (all durations in milliseconds)
// -----------------------------------------------------------------------------

// SlowBlink: steady 500/500 ms blink while advertising, waiting for Shield
#define CFG_LED_BLINK_ON_MS     500
#define CFG_LED_BLINK_OFF_MS    500

// DoubleFlash: flash-flash ... short-pause ... flash-flash ... long-pause (repeat)
// Pattern indicates: Shield bonded, waiting for TiVo remote
#define CFG_LED_DBL_FLASH_MS     80  // width of each individual flash
#define CFG_LED_DBL_GAP_MS       80  // gap between the two flashes in each pair
#define CFG_LED_DBL_PAUSE_MS    200  // short pause between double-flash groups
#define CFG_LED_DBL_CYCLE_MS   1000  // long end-of-cycle pause before repeating

// BootHold: fast yellow blink while BOOT is held before a threshold fires
#define CFG_LED_BOOT_HOLD_ON_MS   250
#define CFG_LED_BOOT_HOLD_OFF_MS  250

// RapidFlash: 100/100 ms on/off during factory-reset hold
#define CFG_LED_RAPID_ON_MS     100
#define CFG_LED_RAPID_OFF_MS    100

// ReadyOnce: 3 quick confirmation flashes, then steady green (Ready)
#define CFG_LED_READY_FLASH_MS   80  // width of each confirmation flash
#define CFG_LED_READY_GAP_MS     80  // gap between confirmation flashes

// -----------------------------------------------------------------------------
// Hardware: Boot button (GPIO 0) - hold to perform bond-management actions
// Actions fire automatically at cumulative thresholds (no release required).
// -----------------------------------------------------------------------------

#define CFG_BOOT_BTN_PIN          0

// BOOT hold thresholds (cumulative — keep holding to reach each step):
//   start     -> fast yellow blink (counting)
//   4 s       -> orange double-flash (warning only)
//   5 s       -> forget TiVo + purple quick-flash
//   8 s       -> forget Shield + purple double-flash
//  10 s       -> factory reset + red quick-flash → green quick-flash → slow blink
#define CFG_BTN_WARN_MS        4000
#define CFG_BTN_TIVO_MS        5000
#define CFG_BTN_SHIELD_MS      8000
#define CFG_BTN_FACTORY_MS    10000

// -----------------------------------------------------------------------------
// NVS namespaces and keys
// -----------------------------------------------------------------------------

#define CFG_NVS_TIVO_NS      "tivo"
#define CFG_NVS_TIVO_ADDR    "addr"
#define CFG_NVS_TIVO_TRUSTED "trusted"

#define CFG_NVS_SHIELD_NS    "shield"
#define CFG_NVS_SHIELD_ADDR  "addr"

#define CFG_NVS_KEYMAP_NS    "keymap"
#define CFG_NVS_KEYMAP_CNT   "cnt"
#define CFG_NVS_KEYMAP_ROW   "r"    // individual row keys are "r0", "r1", ...

// Bump when sdkconfig BLE/PSRAM layout changes (legacy; fwver string is primary).
#define CFG_NVS_SYS_NS       "peanut2"
#define CFG_NVS_SYS_FWVER    "fwver"
#define CFG_NVS_SYS_BLELAY   "blelay"
#define CFG_NVS_BLE_LAYOUT   2

// -----------------------------------------------------------------------------
// Default keymap translations  (X-macro pattern)
//
// Usage:  CFG_DEFAULT_KEYMAP(X)  where X(src, type, dst) produces one row.
// type must be either  Keyboard  or  Consumer  (becomes OutputType::type).
//
// To change a default mapping, edit the row here - no other file needs touching.
// -----------------------------------------------------------------------------

#define CFG_DEFAULT_KEYMAP(X) \
  X(0x003D, Keyboard, 0x3D)    /* TiVo button   -> KEY F4            */ \
  X(0x003E, Keyboard, 0x40)    /* Live TV       -> KEY F7            */ \
  X(0x008D, Keyboard, 0x41)    /* Guide         -> KEY F8            */ \
  X(0x0209, Keyboard, 0x42)    /* Info          -> KEY F9            */ \
  X(0xCE00, Keyboard, 0x43)    /* Skip          -> KEY F10           */ \
  X(0x01C8, Keyboard, 0x44)    /* Netflix       -> KEY F11           */ \
  X(0x0223, Keyboard, 0x4A)    /* Home          -> KEY Home          */ \
  X(0x0041, Consumer, 0x0041)  /* OK / Select   -> CSM pass-through  */ \
  X(0x0224, Keyboard, 0x29)    /* Back          -> ESC               */

// Compatibility aliases
#define CFG_LED_FLASH_GAP_MS     CFG_LED_DBL_GAP_MS
#define CFG_LED_DOUBLE_PAUSE_MS  CFG_LED_DBL_CYCLE_MS
#define CFG_LED_SLOW_MS          CFG_LED_BLINK_ON_MS
#define CFG_LED_RAPID_MS         CFG_LED_RAPID_ON_MS
#define CFG_BTN_HOLD_TIVO_MS     CFG_BTN_TIVO_MS
#define CFG_BTN_HOLD_SHIELD_MS   CFG_BTN_SHIELD_MS
#define CFG_BTN_HOLD_FACTORY_MS  CFG_BTN_FACTORY_MS
#define CFG_BTN_HOLD_WARN_MS     CFG_BTN_WARN_MS

