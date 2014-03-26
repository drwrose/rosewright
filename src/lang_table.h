#ifndef LANG_TABLE_H
#define LANG_TABLE_H

typedef struct {
  const char *locale_name;
  unsigned char font_index;
  unsigned char weekday_name_id;
  unsigned char month_name_id;
} LangDef;

extern LangDef lang_table[];
extern int num_langs;

// These symbols are duplicated in make_lang.py.
//#define MAX_DAY_NAME 7
#define MAX_DAY_NAME 13
#define NUM_DAY_NAMES 12  // Enough for 12 months

// This structure is read from the appropriate resource file
// identified in lang_table, above.
typedef struct {
  const char day_names[NUM_DAY_NAMES][MAX_DAY_NAME];
} DayNames;

#endif
