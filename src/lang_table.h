#ifndef LANG_TABLE_H
#define LANG_TABLE_H

typedef struct {
  const char *locale_name;
  unsigned char font_index;
  unsigned char weekday_name_id;
  unsigned char month_name_id;
  unsigned char ampm_name_id;
} LangDef;

extern LangDef lang_table[];
extern int num_langs;

#endif
