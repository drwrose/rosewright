#ifndef PEBBLE_COMPAT_H
#define PEBBLE_COMPAT_H

// Some definitions to aid in cross-compatibility between SDK 2 and 3.

// GColor static initializers, slightly different syntax needed in SDK 2.
#ifdef PBL_SDK_2

#define GColorBlackInit GColorBlack
#define GColorWhiteInit GColorWhite
#define GColorClearInit GColorClear

#else  // PBL_PLATFORM_APLITE

#define GColorBlackInit { GColorBlackARGB8 }
#define GColorWhiteInit { GColorWhiteARGB8 }
#define GColorClearInit { GColorClearARGB8 }

#endif  // PBL_PLATFORM_APLITE


#endif
