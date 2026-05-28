#include "keymap.h"
#include <Preferences.h>
#include <string.h>

static Preferences sPrefs;

// Built-in default translations
static const RemapEntry kDefaults[] = {
  { 0x003D, OutputType::Keyboard, 0x3D },  // TiVo      → KEY 0x3D
  { 0x003E, OutputType::Keyboard, 0x40 },  // Live TV   → KEY 0x40
  { 0x008D, OutputType::Keyboard, 0x41 },  // Guide     → KEY 0x41
  { 0x0209, OutputType::Keyboard, 0x42 },  // Info      → KEY 0x42
  { 0xCE00, OutputType::Keyboard, 0x43 },  // Skip      → KEY 0x43
  { 0x01C8, OutputType::Keyboard, 0x44 },  // Netflix   → KEY 0x44
  { 0x0223, OutputType::Consumer, 0x0223 }, // Home     → CSM pass-through
  { 0x0041, OutputType::Consumer, 0x0041 }, // OK/Select → CSM pass-through
  { 0x0224, OutputType::Keyboard, 0x29 },  // Back      → ESC
};

static RemapEntry sCustom[KEYMAP_MAX_CUSTOM];
static int        sCustomCount = 0;

void keymapInit() {
  sPrefs.begin("keymap", false);
  sCustomCount = (int)sPrefs.getInt("cnt", 0);
  if (sCustomCount < 0 || sCustomCount > KEYMAP_MAX_CUSTOM)
    sCustomCount = 0;
  for (int i = 0; i < sCustomCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "r%d", i);
    sPrefs.getBytes(key, &sCustom[i], sizeof(RemapEntry));
  }
  Serial.printf("[Keymap] Loaded %d custom remap(s).\r\n", sCustomCount);
}

static void nvsSave() {
  sPrefs.putInt("cnt", sCustomCount);
  for (int i = 0; i < sCustomCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "r%d", i);
    sPrefs.putBytes(key, &sCustom[i], sizeof(RemapEntry));
  }
}

bool keymapLookupConsumer(uint16_t srcCode, OutputType& outType, uint16_t& outCode) {
  // Custom overrides checked first
  for (int i = 0; i < sCustomCount; i++) {
    if (sCustom[i].srcCode == srcCode) {
      outType = sCustom[i].outputType;
      outCode = sCustom[i].dstCode;
      return true;
    }
  }
  // Built-in defaults
  for (const auto& e : kDefaults) {
    if (e.srcCode == srcCode) {
      outType = e.outputType;
      outCode = e.dstCode;
      return true;
    }
  }
  // Passthrough as consumer
  outType = OutputType::Consumer;
  outCode = srcCode;
  return false;
}

bool keymapAdd(uint16_t srcCode, OutputType outType, uint16_t dstCode) {
  for (int i = 0; i < sCustomCount; i++) {
    if (sCustom[i].srcCode == srcCode) {
      sCustom[i].outputType = outType;
      sCustom[i].dstCode    = dstCode;
      nvsSave();
      return true;
    }
  }
  if (sCustomCount >= KEYMAP_MAX_CUSTOM) return false;
  sCustom[sCustomCount++] = {srcCode, outType, dstCode};
  nvsSave();
  return true;
}

bool keymapRemove(uint16_t srcCode) {
  for (int i = 0; i < sCustomCount; i++) {
    if (sCustom[i].srcCode == srcCode) {
      memmove(&sCustom[i], &sCustom[i + 1],
              (sCustomCount - i - 1) * sizeof(RemapEntry));
      sCustomCount--;
      nvsSave();
      return true;
    }
  }
  return false;
}

void keymapClearCustom() {
  sCustomCount = 0;
  sPrefs.clear();
  sPrefs.putInt("cnt", 0);
}

int keymapGetCustom(RemapEntry* buf, int maxCount) {
  int n = (sCustomCount < maxCount) ? sCustomCount : maxCount;
  memcpy(buf, sCustom, n * sizeof(RemapEntry));
  return n;
}

int keymapGetDefaultCount() {
  return (int)(sizeof(kDefaults) / sizeof(kDefaults[0]));
}

const RemapEntry* keymapGetDefaultAt(int i) {
  int n = (int)(sizeof(kDefaults) / sizeof(kDefaults[0]));
  if (i < 0 || i >= n) return nullptr;
  return &kDefaults[i];
}
