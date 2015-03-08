#ifndef PEBBLE_COMPAT_H
#define PEBBLE_COMPAT_H

// Some definitions to aid in cross-compatibility between Aplite and Basalt.

// GColor static initializers, slightly different syntax needed on Basalt.
#ifdef PBL_PLATFORM_APLITE

#define GColorBlackInit GColorBlack
#define GColorWhiteInit GColorWhite
#define GColorClearInit GColorClear

#else  // PBL_PLATFORM_APLITE

#define GColorBlackInit { GColorBlackARGB8 }
#define GColorWhiteInit { GColorWhiteARGB8 }
#define GColorClearInit { GColorClearARGB8 }

#endif  // PBL_PLATFORM_APLITE


#endif
