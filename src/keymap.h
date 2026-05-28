#pragma once
#include <stdint.h>

enum class OutputType : uint8_t { Consumer = 0, Keyboard = 1 };

struct RemapEntry {
  uint16_t   srcCode;
  OutputType outputType;
  uint16_t   dstCode;
};

#define KEYMAP_MAX_CUSTOM 20

void              keymapInit();

// Looks up a consumer usage code. Always sets outType/outCode.
// Returns true if a custom or default mapping was found; false = passthrough.
bool              keymapLookupConsumer(uint16_t srcCode,
                                       OutputType& outType, uint16_t& outCode);

bool              keymapAdd(uint16_t srcCode, OutputType outType, uint16_t dstCode);
bool              keymapRemove(uint16_t srcCode);
void              keymapClearCustom();
int               keymapGetCustom(RemapEntry* buf, int maxCount);
int               keymapGetDefaultCount();
const RemapEntry* keymapGetDefaultAt(int i);
