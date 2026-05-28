#include "web_config.h"
#include "keymap.h"
#include "hid_peripheral.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Arduino.h>

// Status/action helpers defined in other translation units
extern void        repairTiVo();
extern void        repairShield();
extern bool        tivoHasBond();
extern bool        tivoIsConnected();
extern String      tivoGetAddr();
extern const char* tivoGetState();
extern const char* hidGetShieldState();

static WebServer   sHttp(80);
static Preferences sWifiPrefs;
static Preferences sCfgPrefs;

// Cached STA credentials (loaded from NVS at boot)
static String        sStaSsid;
static String        sStaPass;
static bool          sApDisabled      = false;
static int           sApTimeoutMin    = 0;        // 0 = always on
static unsigned long sLastHttpActivity = 0;       // millis() of last HTTP request
static String        sBlePowerLabel   = "P0";    // current TX power label

// BLE TX power level table — use integer casts because ESP_PWR_LVL_P0 is not
// defined on ESP32-C3 (the IDF names it ESP_PWR_LVL_N0 there). Numeric values
// 0–7 map to -12, -9, -6, -3, 0, +3, +6, +9 dBm universally.
struct PowerEntry { const char* label; esp_power_level_t level; };
static const PowerEntry kPowerLevels[] = {
  {"N12", (esp_power_level_t)0}, {"N9",  (esp_power_level_t)1},
  {"N6",  (esp_power_level_t)2}, {"N3",  (esp_power_level_t)3},
  {"P0",  (esp_power_level_t)4}, {"P3",  (esp_power_level_t)5},
  {"P6",  (esp_power_level_t)6}, {"P9",  (esp_power_level_t)7},
};
static const int kNumPowerLevels = (int)(sizeof(kPowerLevels)/sizeof(kPowerLevels[0]));

static void applyBlePower(const String& label) {
  for (int i = 0; i < kNumPowerLevels; i++) {
    if (label == kPowerLevels[i].label) {
      NimBLEDevice::setPower(kPowerLevels[i].level);
      Serial.printf("[Web] BLE TX power → %s\r\n", label.c_str());
      return;
    }
  }
  NimBLEDevice::setPower((esp_power_level_t)4);  // fallback: 0 dBm
}

static void apEnable() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("TiVoTranslator", "tivotivo");
  sApDisabled = false;
  Serial.println("[Web] SoftAP re-enabled.");
}

static void apDisable() {
  WiFi.softAPdisconnect(false);  // disconnect clients, keep radio
  sApDisabled = true;
  Serial.printf("[Web] SoftAP disabled. Reachable at http://%s\r\n",
                WiFi.localIP().toString().c_str());
}

// ---- Button definitions ----

struct TiVoButton { uint16_t code; const char* label; };

static const TiVoButton kAllButtons[] = {
  { 0x0030, "Power"      }, { 0x003D, "TiVo"       }, { 0x003E, "Live TV"    },
  { 0x008D, "Guide"      }, { 0x0209, "Info"        }, { 0x0082, "Input"      },
  { 0x00E2, "Mute"       }, { 0x00E9, "Vol Up"      }, { 0x00EA, "Vol Down"   },
  { 0x009C, "Chan Up"    }, { 0x009D, "Chan Down"   }, { 0xCE00, "Skip"       },
  { 0x01C8, "Netflix"    }, { 0x0221, "Search"      }, { 0x0223, "Home"       },
  { 0x0224, "Back"       }, { 0x0041, "OK / Select" }, { 0x0042, "Nav Up"     },
  { 0x0043, "Nav Down"   }, { 0x0044, "Nav Left"    }, { 0x0045, "Nav Right"  },
};

// ---- Friendly-name helpers ----

struct NameEntry { const char* key; const char* label; };

static const NameEntry kNames[] = {
  // Keyboard keys
  {"K0x28","Enter"},  {"K0x29","ESC"},    {"K0x2a","Backspace"},{"K0x2b","Tab"},
  {"K0x2c","Space"},  {"K0x4a","Home"},   {"K0x4b","Page Up"},  {"K0x4d","End"},
  {"K0x4e","Page Dn"},{"K0x4f","→"},      {"K0x50","←"},        {"K0x51","↓"},
  {"K0x52","↑"},
  {"K0x3a","F1"},{"K0x3b","F2"},{"K0x3c","F3"},{"K0x3d","F4"},
  {"K0x3e","F5"},{"K0x3f","F6"},{"K0x40","F7"},{"K0x41","F8"},
  {"K0x42","F9"},{"K0x43","F10"},{"K0x44","F11"},{"K0x45","F12"},
  {"K0x1e","1"},{"K0x1f","2"},{"K0x20","3"},{"K0x21","4"},{"K0x22","5"},
  {"K0x23","6"},{"K0x24","7"},{"K0x25","8"},{"K0x26","9"},{"K0x27","0"},
  // Consumer codes
  {"C0x30","Power"},{"C0x3d","TiVo"},{"C0x3e","Live TV"},
  {"C0x41","OK/Select"},{"C0x42","Nav Up"},{"C0x43","Nav Down"},
  {"C0x44","Nav Left"},{"C0x45","Nav Right"},
  {"C0x82","Input"},{"C0x8d","Guide"},
  {"C0x9c","Chan Up"},{"C0x9d","Chan Down"},
  {"C0xe2","Mute"},{"C0xe9","Vol Up"},{"C0xea","Vol Down"},
  {"C0x1c8","Netflix"},{"C0x209","Info"},
  {"C0x221","Search"},{"C0x223","Home"},{"C0x224","Back"},
  {"C0xce00","Skip"},
};

// Key options for the remap dropdown (KEY type)
struct KeyOption { uint8_t code; const char* name; };
static const KeyOption kKeyOptions[] = {
  {0x29,"ESC"},   {0x28,"Enter"}, {0x2A,"Backspace"},{0x2B,"Tab"},   {0x2C,"Space"},
  {0x3A,"F1"},    {0x3B,"F2"},    {0x3C,"F3"},   {0x3D,"F4"},
  {0x3E,"F5"},    {0x3F,"F6"},    {0x40,"F7"},   {0x41,"F8"},
  {0x42,"F9"},    {0x43,"F10"},   {0x44,"F11"},  {0x45,"F12"},
  {0x4A,"Home"},  {0x4D,"End"},   {0x4B,"Pg Up"},{0x4E,"Pg Dn"},
  {0x52,"↑"},     {0x51,"↓"},     {0x50,"←"},    {0x4F,"→"},
  {0x27,"0"},{0x1E,"1"},{0x1F,"2"},{0x20,"3"},{0x21,"4"},
  {0x22,"5"},{0x23,"6"},{0x24,"7"},{0x25,"8"},{0x26,"9"},
};
static const int kNumKeyOptions = (int)(sizeof(kKeyOptions)/sizeof(kKeyOptions[0]));

static String labelFor(char prefix, uint32_t code) {
  char key[16];
  snprintf(key, sizeof(key), "%c0x%x", prefix, code);
  for (const auto& e : kNames)
    if (strcasecmp(e.key, key) == 0) return String(e.label);
  snprintf(key, sizeof(key), "0x%04X", (unsigned)code);
  return String(key);
}

static String dstLabel(OutputType t, uint16_t code) {
  return labelFor(t == OutputType::Keyboard ? 'K' : 'C', code);
}

// ---- CSS / HTML helpers ----

static const char kCss[] =
  "body{font-family:system-ui,sans-serif;max-width:960px;margin:2rem auto;"
  "padding:0 1rem;background:#0f172a;color:#e2e8f0}"
  "h1{color:#a78bfa;margin-bottom:.15rem}"
  ".sub{color:#94a3b8;font-size:.87em;margin-bottom:1rem}"
  // Tabs
  ".tabs{display:flex;gap:0;border-bottom:2px solid #1e293b;margin-bottom:1.5rem}"
  ".tab{background:none;border:none;color:#94a3b8;padding:.6rem 1.4rem;font-size:.95em;"
  "cursor:pointer;border-bottom:2px solid transparent;margin-bottom:-2px;border-radius:.3rem .3rem 0 0}"
  ".tab:hover{color:#e2e8f0;background:#1e293b55}"
  ".tab.on{color:#a78bfa;border-bottom-color:#a78bfa;background:#1e293b}"
  ".pane{display:none}.pane.on{display:block}"
  // Table
  "table{width:100%;border-collapse:collapse;margin:.5rem 0 1.5rem}"
  "th{background:#1e293b;padding:.5rem .7rem;text-align:left;color:#94a3b8;font-weight:500;font-size:.85em}"
  "td{padding:.4rem .7rem;border-bottom:1px solid #1e293b;vertical-align:middle}"
  "tr:hover td{background:#1e293b55}"
  // Badges
  ".kbadge,.cbadge{display:inline-block;padding:.1rem .4rem;border-radius:.25rem;"
  "font-size:.72em;font-weight:700;margin-right:.3rem;vertical-align:middle}"
  ".kbadge{background:#1d4ed8}.cbadge{background:#065f46}"
  ".tag-cust{background:#3b1d6e;color:#c4b5fd;font-size:.7em;padding:.1rem .35rem;"
  "border-radius:.2rem;margin-left:.3rem;vertical-align:middle}"
  ".tag-def{background:#1e3a5f;color:#7dd3fc;font-size:.7em;padding:.1rem .35rem;"
  "border-radius:.2rem;margin-left:.3rem;vertical-align:middle}"
  ".ok{color:#4ade80}.err{color:#f87171}.warn{color:#fbbf24}"
  // Inline remap form
  "form.ifrm{display:flex;gap:.3rem;align-items:center;flex-wrap:nowrap}"
  "form.inline{display:inline}"
  "select.ts{width:68px;padding:.25rem .3rem;font-size:.8em;"
  "background:#0f172a;color:#e2e8f0;border:1px solid #334155;border-radius:.25rem}"
  "input.tc{width:72px;padding:.25rem .4rem;font-size:.8em;"
  "background:#0f172a;color:#e2e8f0;border:1px solid #334155;border-radius:.25rem}"
  ".set{background:#14532d;color:#86efac;border:none;border-radius:.25rem;"
  "padding:.25rem .55rem;font-size:.78em;cursor:pointer;white-space:nowrap}"
  ".set:hover{background:#166534}"
  ".rst{background:#7f1d1d;color:#fca5a5;border:none;border-radius:.25rem;"
  "padding:.25rem .55rem;font-size:.78em;cursor:pointer;white-space:nowrap}"
  ".rst:hover{background:#991b1b}"
  // Cards / forms
  ".card{background:#1e293b;border-radius:.5rem;padding:1.25rem;margin:1rem 0}"
  "label{display:block;color:#cbd5e1;font-size:.87em;margin:.6rem 0 .2rem}"
  "input.wide,select.wide{width:100%;background:#0f172a;color:#e2e8f0;"
  "border:1px solid #334155;border-radius:.3rem;padding:.4rem .6rem;"
  "box-sizing:border-box;font-size:.9em}"
  ".row{display:flex;gap:.75rem;flex-wrap:wrap}"
  ".row>div{flex:1;min-width:160px}"
  ".btn{margin-top:.9rem;padding:.5rem 1.25rem;border:none;border-radius:.3rem;"
  "cursor:pointer;font-size:.9em}"
  ".save{background:#065f46;color:#6ee7b7}"
  ".disc{background:#78350f;color:#fbbf24;font-size:.82em}"
  ".note{color:#94a3b8;font-size:.85em;margin:.3rem 0 0}"
  ".dim{color:#cbd5e1}";

static String htmlHead(const String& title) {
  return "<!DOCTYPE html><html><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>" + title + "</title>"
         "<style>" + String(kCss) + "</style></head><body>";
}

// ---- WiFi status helpers ----

static String staStatusBadge() {
  switch (WiFi.status()) {
    case WL_CONNECTED:
      return "<span class='ok'>&#x25CF; Connected — " +
             WiFi.localIP().toString() + "</span>";
    case WL_NO_SSID_AVAIL:
      return "<span class='err'>&#x25CF; SSID not found</span>";
    case WL_CONNECT_FAILED:
      return "<span class='err'>&#x25CF; Wrong password</span>";
    case WL_DISCONNECTED:
      return sStaSsid.length() > 0
               ? "<span class='warn'>&#x25CF; Connecting to " + sStaSsid + "&#8230;</span>"
               : "<span class='dim'>&#x25CF; Not configured</span>";
    default:
      return "<span class='dim'>&#x25CF; Not configured</span>";
  }
}

// ---- Route handlers ----

// Lightweight JSON status endpoint — polled by the Devices tab every second.
static void handleStatus() {
  String tivoAddr   = tivoGetAddr();
  String shieldAddr = hidGetShieldAddr();
  String j = "{";
  j += "\"tivo\":{\"state\":\"";   j += tivoGetState();    j += "\",\"addr\":\""; j += tivoAddr;   j += "\"},";
  j += "\"shield\":{\"state\":\""; j += hidGetShieldState(); j += "\",\"addr\":\""; j += shieldAddr; j += "\"},";
  j += "\"wifi\":{\"connected\":";
  j += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  j += ",\"ip\":\""; j += WiFi.localIP().toString(); j += "\"}}";
  sHttp.sendHeader("Cache-Control", "no-cache");
  sHttp.send(200, "application/json", j);
}

static void handleRoot() {
  sLastHttpActivity = millis();  // reset AP timeout timer on every page load

  // Pre-load custom overrides for badge + reset-button logic
  RemapEntry custom[KEYMAP_MAX_CUSTOM];
  int nc = keymapGetCustom(custom, KEYMAP_MAX_CUSTOM);

  String h = htmlHead("TiVo Bridge Config");
  h += "<h1>TiVo &#8594; Shield Translator</h1>"
       "<p class='sub'>AP: 192.168.4.1 &nbsp;&#x2022;&nbsp; SSID: TiVoTranslator / tivotivo</p>"
       "<div class='tabs'>"
       "<button class='tab' data-tab='map' onclick='showTab(\"map\")'>Button Map</button>"
       "<button class='tab' data-tab='wifi' onclick='showTab(\"wifi\")'>WiFi</button>"
       "<button class='tab' data-tab='devices' onclick='showTab(\"devices\")'>Devices</button>"
       "</div>";

  // ---- WiFi settings ----
  bool staOk = (WiFi.status() == WL_CONNECTED);
  h += "<div id='pane-wifi' class='pane'>"
       "<h2>WiFi (Home Network)</h2><div class='card'>"
       "<p>Home network: " + staStatusBadge() + "</p>"
       "<p>SoftAP (TiVoTranslator):&nbsp;";
  if (sApDisabled) {
    h += "<span class='warn'>&#x25CF; Disabled</span> &nbsp;"
         "<form class='inline' action='/apon' method='get'>"
         "<button class='btn' style='background:#14532d;color:#86efac;"
         "padding:.2rem .7rem;font-size:.8em;margin-top:0'>Enable AP</button></form>";
  } else {
    h += "<span class='ok'>&#x25CF; On &mdash; 192.168.4.1</span>";
    if (staOk)
      h += " &nbsp;<form class='inline' action='/apoff' method='get' "
           "onsubmit='return confirm(\"Disable the AP? Use your home network IP to reach this page.\")'>"
           "<button class='btn' style='background:#78350f;color:#fbbf24;"
           "padding:.2rem .7rem;font-size:.8em;margin-top:0'>Disable AP</button></form>";
    else
      h += "&nbsp;<span class='dim' style='font-size:.8em'>"
           "(connect to home network first to disable)</span>";
  }
  h += "</p>";
  if (staOk) {
    h += "<p class='note'>Home IP: <strong>http://" + WiFi.localIP().toString() + "</strong></p>"
         "<form action='/wifidisconnect' method='get' style='margin-top:.6rem'>"
         "<button class='btn disc'>Disconnect &amp; Forget</button></form>";
  } else {
    h += "<form action='/wifisave' method='get'><div class='row'>"
         "<div><label>SSID</label>"
         "<input class='wide' name='ssid' placeholder='Your WiFi name'";
    if (sStaSsid.length()) h += " value='" + sStaSsid + "'";
    h += " required></div>"
         "<div><label>Password</label>"
         "<input class='wide' name='pass' type='password' placeholder='WiFi password'></div>"
         "</div><button class='btn save'>Save &amp; Connect</button></form>";
    if (sStaSsid.length())
      h += "<form action='/wifidisconnect' method='get' style='margin-top:.4rem'>"
           "<button class='btn disc'>Forget Saved Network</button></form>";
  }
  h += "</div></div>"; // close .card then pane-wifi

  // ---- Button map (unified — every button, inline edit) ----
  h += "<div id='pane-map' class='pane'>"
       "<h2>Button Map</h2>"
       "<table>"
       "<tr><th>Button</th><th>Source</th><th>Mapped To</th>"
       "<th style='min-width:260px'>Remap</th></tr>";

  int numButtons = sizeof(kAllButtons) / sizeof(kAllButtons[0]);
  for (int i = 0; i < numButtons; i++) {
    uint16_t src = kAllButtons[i].code;
    const char* btnName = kAllButtons[i].label;

    // Resolve current effective mapping
    OutputType curType;
    uint16_t   curCode;
    keymapLookupConsumer(src, curType, curCode);
    bool isKb = (curType == OutputType::Keyboard);

    // Check for custom override
    bool hasCustom = false;
    for (int j = 0; j < nc; j++)
      if (custom[j].srcCode == src) { hasCustom = true; break; }

    // Check for built-in default
    bool hasDef = false;
    for (int d = 0; d < keymapGetDefaultCount(); d++)
      if (keymapGetDefaultAt(d)->srcCode == src) { hasDef = true; break; }

    h += "<tr>";

    // Button name
    h += "<td><strong>" + String(btnName) + "</strong></td>";

    // Source code
    h += "<td style='color:#cbd5e1;font-size:.9em'>0x" + String(src, HEX) + "</td>";

    // Current mapping + badge
    h += "<td><span class='" + String(isKb ? "kbadge'>KEY" : "cbadge'>CSM") + "</span>"
         + dstLabel(curType, curCode)
         + " <span style='color:#cbd5e1;font-size:.88em'>0x" + String(curCode, HEX) + "</span>";
    if (hasCustom)
      h += "<span class='tag-cust'>custom</span>";
    else if (hasDef)
      h += "<span class='tag-def'>default</span>";
    h += "</td>";

    // Inline remap form — KEY type shows named dropdown, CSM shows hex input
    String keyOpts;
    for (int j = 0; j < kNumKeyOptions; j++) {
      char vbuf[8]; snprintf(vbuf, sizeof(vbuf), "0x%x", kKeyOptions[j].code);
      keyOpts += "<option value='" + String(vbuf) + "'";
      if (isKb && curCode == kKeyOptions[j].code) keyOpts += " selected";
      keyOpts += ">" + String(kKeyOptions[j].name) + "</option>";
    }
    h += "<td><form class='ifrm' action='/add' method='get'>"
         "<input type='hidden' name='src' value='" + String(src) + "'>"
         "<select class='ts' name='type' onchange='updDst(this)'>"
         "<option value='k'" + String(isKb ? " selected" : "") + ">KEY</option>"
         "<option value='c'" + String(!isKb ? " selected" : "") + ">CSM</option>"
         "</select>"
         // Named key dropdown (visible when KEY is selected)
         "<select class='tc key-sel' name='dst' style='width:90px" +
         String(isKb ? "" : ";display:none") + "'" +
         String(isKb ? "" : " disabled") + ">" + keyOpts + "</select>"
         // Hex input (visible when CSM is selected)
         "<input class='tc csm-inp' name='dst' value='0x" + String(curCode, HEX) + "'"
         " style='" + String(isKb ? "display:none" : "") + "'" +
         String(isKb ? " disabled" : "") + ">"
         "<button type='submit' class='set'>Set</button>"
         "</form>";
    // Reset button always shown; disabled+dimmed when already at default/passthrough
    h += "<form class='inline' action='/remove' method='get'>"
         "<input type='hidden' name='src' value='" + String(src) + "'>";
    if (hasCustom)
      h += "<button type='submit' class='rst'>Reset</button>";
    else
      h += "<button type='submit' class='rst' disabled "
           "style='opacity:.35;cursor:not-allowed'>Reset</button>";
    h += "</form>";
    h += "</td></tr>";
  }
  h += "</table></div>"; // close pane-map

  // ---- Device management ----
  // Status spans are populated immediately with server-side values on load,
  // then kept live by the JS poller (no flicker on first paint).
  {
    const char* ts = tivoGetState();
    const char* ss = hidGetShieldState();
    String tivoAddr   = tivoGetAddr();
    String shieldAddr = hidGetShieldAddr();

    // Inline helper — maps state token to CSS class + text
    auto tivoBadge = [&](const char* st) -> String {
      if (!strcmp(st,"ready"))       return "<span class='ok'>&#x25CF; Connected &mdash; forwarding</span>";
      if (!strcmp(st,"connecting"))  return "<span class='warn'>&#x25CF; Connecting&hellip;</span>";
      if (!strcmp(st,"reconnecting"))return "<span class='warn'>&#x25CF; Reconnecting&hellip;</span>";
      return "<span class='dim'>&#x25CF; Scanning for TiVo&hellip;</span>";
    };
    auto shieldBadge = [&](const char* st) -> String {
      if (!strcmp(st,"ready"))       return "<span class='ok'>&#x25CF; Connected &mdash; ready</span>";
      if (!strcmp(st,"negotiating")) return "<span class='warn'>&#x25CF; Negotiating&hellip;</span>";
      if (!strcmp(st,"adv_bonded"))  return "<span class='warn'>&#x25CF; Bonded &mdash; re-advertising</span>";
      return "<span class='dim'>&#x25CF; Not paired &mdash; advertising</span>";
    };

    h += "<div id='pane-devices' class='pane'>"
         "<h2>Device Management</h2>"
         "<div class='row'>";

    // TiVo card
    h += "<div class='card'>"
         "<p style='font-size:1em;font-weight:600;color:#7dd3fc;margin:.1rem 0 .5rem'>TiVo Remote</p>"
         "<p style='margin:.2rem 0' id='tivo-status'>" + tivoBadge(ts) + "</p>"
         "<p class='note' style='margin:.3rem 0' id='tivo-addr'>";
    if (tivoAddr.length())
      h += "Address: <code style='color:#c4b5fd'>" + tivoAddr + "</code>";
    h += "</p>"
         "<p class='note' style='margin:.5rem 0 .7rem'>To re-pair: press "
         "<strong>TiVo&nbsp;+&nbsp;Back</strong> on the remote after clicking below.</p>"
         "<form action='/repairTivo' method='get' "
         "onsubmit='return confirm(\"Forget TiVo bond and start scanning?\")'>"
         "<button class='btn' style='background:#1e3a5f;color:#7dd3fc;margin-top:0'>"
         "Forget &amp; Re-pair TiVo</button></form>"
         "</div>";

    // Shield card
    h += "<div class='card'>"
         "<p style='font-size:1em;font-weight:600;color:#7dd3fc;margin:.1rem 0 .5rem'>Nvidia Shield TV</p>"
         "<p style='margin:.2rem 0' id='shield-status'>" + shieldBadge(ss) + "</p>"
         "<p class='note' style='margin:.3rem 0' id='shield-addr'>";
    if (shieldAddr.length())
      h += "Address: <code style='color:#c4b5fd'>" + shieldAddr + "</code>";
    h += "</p>"
         "<p class='note' style='margin:.5rem 0 .7rem'>To re-pair: click below, then "
         "go to <strong>Shield Settings &rsaquo; Remote &amp; Accessories</strong>.</p>"
         "<form action='/repairShield' method='get' "
         "onsubmit='return confirm(\"Forget Shield bond and re-advertise?\")'>"
         "<button class='btn' style='background:#1e3a5f;color:#7dd3fc;margin-top:0'>"
         "Forget &amp; Re-pair Shield</button></form>"
         "</div>";

    h += "</div>";  // close device .row
  }

  // ---- Settings row ----
  h += "<div class='row' style='margin-top:.5rem'>";

  // AP Timeout card
  h += "<div class='card'>"
       "<p style='font-size:1em;font-weight:600;color:#7dd3fc;margin:.1rem 0 .4rem'>"
       "WiFi AP Timeout</p>"
       "<p class='note' style='margin-bottom:.6rem'>Shut down the AP after this many "
       "minutes with no web activity. 0 = always on.</p>"
       "<form action='/setaptimeout' method='get' style='display:flex;gap:.5rem;align-items:center'>"
       "<input class='tc' type='number' name='minutes' min='0' max='1440' "
       "value='" + String(sApTimeoutMin) + "' style='width:70px'>"
       "<span class='dim' style='font-size:.85em'>min</span>"
       "<button class='set' type='submit'>Save</button>"
       "</form>"
       "<p class='note' style='margin-top:.5rem'>AP always restarts on reboot. "
       "Hold <strong>Boot (GPIO&nbsp;9)</strong> for 3&nbsp;s to re-enable early.</p>"
       "</div>";

  // BLE TX Power card
  String powerOpts;
  for (int i = 0; i < kNumPowerLevels; i++) {
    powerOpts += "<option value='" + String(kPowerLevels[i].label) + "'";
    if (sBlePowerLabel == kPowerLevels[i].label) powerOpts += " selected";
    powerOpts += ">" + String(kPowerLevels[i].label) + " dBm</option>";
  }
  h += "<div class='card'>"
       "<p style='font-size:1em;font-weight:600;color:#7dd3fc;margin:.1rem 0 .4rem'>"
       "BLE TX Power</p>"
       "<p class='note' style='margin-bottom:.6rem'>Applied immediately and saved for "
       "next boot. Higher = longer range, more power.</p>"
       "<form action='/setblepower' method='get' style='display:flex;gap:.5rem;align-items:center'>"
       "<select class='ts' name='power' style='width:90px'>" + powerOpts + "</select>"
       "<button class='set' type='submit'>Apply</button>"
       "</form>"
       "</div>";

  h += "</div>";  // close settings .row

  // Live indicator line
  h += "<p class='note' style='margin-top:.5rem'>"
       "<span id='poll-dot' style='color:#4ade80;font-size:.8em'>&#x25CF;</span>"
       "<span class='dim' style='font-size:.82em'>&nbsp;Status updates live &mdash; no refresh needed</span>"
       "</p>"
       "</div>"; // close pane-devices

  // ---- JavaScript ----
  h += "<script>"
       // ---- BLE status poller ----
       "var pollTimer=null;"
       // State → [cssClass, displayText]
       "var TS={"
         "'scanning':   ['dim', '&#x25CF; Scanning for TiVo\u2026'],"
         "'reconnecting':['warn','&#x25CF; Reconnecting\u2026'],"
         "'connecting':  ['warn','&#x25CF; Connecting\u2026'],"
         "'ready':       ['ok',  '&#x25CF; Connected \u2014 forwarding']"
       "};"
       "var SS={"
         "'advertising': ['dim', '&#x25CF; Not paired \u2014 advertising'],"
         "'adv_bonded':  ['warn','&#x25CF; Bonded \u2014 re-advertising'],"
         "'negotiating': ['warn','&#x25CF; Negotiating\u2026'],"
         "'ready':       ['ok',  '&#x25CF; Connected \u2014 ready']"
       "};"
       "function setBadge(id,cls,txt){"
         "var e=document.getElementById(id);"
         "if(e)e.innerHTML=\"<span class='\"+cls+\"'>\"+txt+\"</span>\";}"
       "function setAddr(id,addr){"
         "var e=document.getElementById(id);"
         "if(!e)return;"
         "e.innerHTML=addr?\"Address: <code style='color:#c4b5fd'>\"+addr+\"</code>\":'';}"
       "var dotOn=true;"
       "function doPoll(){"
         "fetch('/status').then(function(r){return r.json();}).then(function(d){"
           "var t=TS[d.tivo.state]||['dim','&#x25CF; Unknown'];"
           "setBadge('tivo-status',t[0],t[1]);"
           "setAddr('tivo-addr',d.tivo.addr);"
           "var s=SS[d.shield.state]||['dim','&#x25CF; Unknown'];"
           "setBadge('shield-status',s[0],s[1]);"
           "setAddr('shield-addr',d.shield.addr);"
           // Pulse the live dot
           "var dot=document.getElementById('poll-dot');"
           "if(dot){dot.style.opacity=dotOn?'1':'.25';dotOn=!dotOn;}"
         "}).catch(function(){});}"
       "function startPoll(){if(!pollTimer){doPoll();pollTimer=setInterval(doPoll,1000);}}"
       "function stopPoll(){clearInterval(pollTimer);pollTimer=null;}"
       // ---- Tab switching (localStorage-persistent, starts/stops poller) ----
       "function showTab(n){"
         "document.querySelectorAll('.pane').forEach(function(p){p.classList.remove('on');});"
         "document.querySelectorAll('.tab').forEach(function(t){t.classList.remove('on');});"
         "document.getElementById('pane-'+n).classList.add('on');"
         "document.querySelector('[data-tab=\"'+n+'\"]').classList.add('on');"
         "localStorage.setItem('tab',n);"
         "if(n==='devices')startPoll();else stopPoll();}"
       "var initTab=localStorage.getItem('tab')||'map';"
       "showTab(initTab);"
       // ---- KEY/CSM remap toggle ----
       "function updDst(sel){"
         "var f=sel.closest('form');"
         "var ks=f.querySelector('.key-sel');"
         "var ci=f.querySelector('.csm-inp');"
         "var isK=sel.value==='k';"
         "ks.disabled=!isK;ks.style.display=isK?'':'none';"
         "ci.disabled=isK;ci.style.display=isK?'none':''}"
       "</script>";

  h += "</body></html>";
  sHttp.send(200, "text/html", h);
}

static void handleWifiSave() {
  String ssid = sHttp.arg("ssid");
  String pass = sHttp.arg("pass");

  if (ssid.length() == 0) {
    sHttp.sendHeader("Location", "/");
    sHttp.send(303);
    return;
  }

  // If password field was left blank, reuse stored password
  if (pass.length() == 0) pass = sStaPass;

  sStaSsid = ssid;
  sStaPass = pass;

  sWifiPrefs.begin("wifi", false);
  sWifiPrefs.putString("ssid", ssid);
  sWifiPrefs.putString("pass", pass);
  sWifiPrefs.end();

  Serial.printf("[WiFi] Connecting to '%s'...\r\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleWifiDisconnect() {
  WiFi.disconnect(false);
  sStaSsid = "";
  sStaPass = "";
  sWifiPrefs.begin("wifi", false);
  sWifiPrefs.remove("ssid");
  sWifiPrefs.remove("pass");
  sWifiPrefs.end();
  Serial.println("[WiFi] STA credentials cleared.");
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleApOff() {
  if (WiFi.status() == WL_CONNECTED) {
    sWifiPrefs.begin("wifi", false);
    sWifiPrefs.putBool("apoff", true);
    sWifiPrefs.end();
    apDisable();
  }
  String dest = sApDisabled
    ? "http://" + WiFi.localIP().toString() + "/"
    : "/";
  sHttp.sendHeader("Location", dest);
  sHttp.send(303);
}

static void handleApOn() {
  sWifiPrefs.begin("wifi", false);
  sWifiPrefs.putBool("apoff", false);
  sWifiPrefs.end();
  apEnable();
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleRepairTiVo() {
  repairTiVo();
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleRepairShield() {
  repairShield();
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleAdd() {
  uint16_t srcCode = (uint16_t)strtoul(sHttp.arg("src").c_str(), nullptr, 0);
  uint16_t dstCode = (uint16_t)strtoul(sHttp.arg("dst").c_str(), nullptr, 0);
  OutputType outType = (sHttp.arg("type") == "k") ? OutputType::Keyboard : OutputType::Consumer;
  if (srcCode && dstCode) {
    if (keymapAdd(srcCode, outType, dstCode))
      Serial.printf("[Web] Remap 0x%04X -> %s 0x%04X\r\n", srcCode,
                    outType == OutputType::Keyboard ? "KB" : "CS", dstCode);
    else
      Serial.println("[Web] Remap table full.");
  }
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleRemove() {
  uint16_t srcCode = (uint16_t)strtoul(sHttp.arg("src").c_str(), nullptr, 0);
  if (srcCode) keymapRemove(srcCode);
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleSetApTimeout() {
  int val = sHttp.arg("minutes").toInt();
  if (val < 0) val = 0;
  sApTimeoutMin     = val;
  sLastHttpActivity = millis();  // reset timer when the setting itself changes
  sCfgPrefs.begin("cfg", false);
  sCfgPrefs.putInt("ap_timeout", val);
  sCfgPrefs.end();
  Serial.printf("[Web] AP timeout set to %d min\r\n", val);
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

static void handleSetBlePower() {
  String label = sHttp.arg("power");
  bool valid = false;
  for (int i = 0; i < kNumPowerLevels; i++)
    if (label == kPowerLevels[i].label) { valid = true; break; }
  if (!valid) label = "P0";
  sBlePowerLabel = label;
  applyBlePower(label);
  sCfgPrefs.begin("cfg", false);
  sCfgPrefs.putString("ble_power", label);
  sCfgPrefs.end();
  // setPower() resets the BLE radio — rebuild and restart advertising so the
  // Shield reconnects automatically rather than sitting disconnected.
  hidRestartAdvertising();
  sHttp.sendHeader("Location", "/");
  sHttp.send(303);
}

// ---- Public API ----

void webConfigInit() {
  // AP+STA mode: SoftAP always available as fallback config portal
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("TiVoTranslator", "tivotivo");
  Serial.printf("[Web] SoftAP up — 192.168.4.1\r\n");

  // Load cfg settings (AP timeout, BLE power)
  sCfgPrefs.begin("cfg", true);
  sApTimeoutMin  = (int)sCfgPrefs.getInt("ap_timeout", 0);
  sBlePowerLabel = sCfgPrefs.getString("ble_power", "P0");
  sCfgPrefs.end();
  sLastHttpActivity = millis();

  // Apply stored BLE TX power (NimBLEDevice::init() already called before webConfigInit)
  applyBlePower(sBlePowerLabel);

  // GPIO 9 (Boot button) — held 3 s re-enables the AP
  pinMode(9, INPUT_PULLUP);

  // Try to connect to stored home-network credentials
  sWifiPrefs.begin("wifi", true);
  sStaSsid   = sWifiPrefs.getString("ssid", "");
  sStaPass   = sWifiPrefs.getString("pass", "");
  bool apOff = sWifiPrefs.getBool("apoff", false);
  sWifiPrefs.end();

  if (sStaSsid.length() > 0) {
    Serial.printf("[WiFi] Connecting to stored network '%s'...\r\n", sStaSsid.c_str());
    WiFi.begin(sStaSsid.c_str(), sStaPass.c_str());
    // apOff is applied in webConfigLoop() once STA is confirmed connected
    if (apOff) Serial.println("[Web] AP-disable preference set; will apply once STA connects.");
    sApDisabled = false;  // always start with AP on; disable after STA is confirmed
    (void)apOff;          // stored in NVS, checked in loop
  }

  sHttp.on("/",               HTTP_GET, handleRoot);
  sHttp.on("/status",         HTTP_GET, handleStatus);
  sHttp.on("/wifisave",       HTTP_GET, handleWifiSave);
  sHttp.on("/wifidisconnect", HTTP_GET, handleWifiDisconnect);
  sHttp.on("/apoff",          HTTP_GET, handleApOff);
  sHttp.on("/apon",           HTTP_GET, handleApOn);
  sHttp.on("/repairTivo",     HTTP_GET, handleRepairTiVo);
  sHttp.on("/repairShield",   HTTP_GET, handleRepairShield);
  sHttp.on("/add",            HTTP_GET, handleAdd);
  sHttp.on("/remove",         HTTP_GET, handleRemove);
  sHttp.on("/setaptimeout",   HTTP_GET, handleSetApTimeout);
  sHttp.on("/setblepower",    HTTP_GET, handleSetBlePower);
  sHttp.begin();
  Serial.println("[Web] HTTP server on port 80.");
}

void webConfigLoop() {
  sHttp.handleClient();

  // AP auto-timeout (based on last HTTP activity)
  if (sApTimeoutMin > 0 && !sApDisabled) {
    if (millis() - sLastHttpActivity > (unsigned long)sApTimeoutMin * 60000UL) {
      Serial.printf("[Web] AP idle timeout (%d min) — disabling AP.\r\n", sApTimeoutMin);
      apDisable();
    }
  }

  // GPIO 9 (Boot button): hold 3 s to re-enable AP
  {
    static unsigned long sBtnPressedAt = 0;
    static bool          sBtnHeld      = false;
    if (digitalRead(9) == LOW) {
      if (sBtnPressedAt == 0) sBtnPressedAt = millis();
      if (!sBtnHeld && millis() - sBtnPressedAt >= 3000) {
        sBtnHeld = true;
        if (sApDisabled) {
          Serial.println("[Web] Boot button held 3 s — re-enabling AP.");
          apEnable();
          sLastHttpActivity = millis();  // restart timeout timer
        }
      }
    } else {
      sBtnPressedAt = 0;
      sBtnHeld      = false;
    }
  }

  // Log STA connection event once; apply AP-disable preference on first connect
  static bool sWasConnected = false;
  bool now = (WiFi.status() == WL_CONNECTED);
  if (now && !sWasConnected) {
    Serial.printf("[WiFi] Connected to '%s'  IP: %s\r\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

    // Apply stored AP-disable preference now that STA is up
    sWifiPrefs.begin("wifi", true);
    bool apOff = sWifiPrefs.getBool("apoff", false);
    sWifiPrefs.end();
    if (apOff && !sApDisabled) apDisable();
  }
  if (!now && sWasConnected && sApDisabled) {
    // STA dropped and AP is off — re-enable AP so the device is still reachable
    Serial.println("[WiFi] STA lost while AP was disabled — re-enabling AP.");
    apEnable();
  }
  sWasConnected = now;
}
