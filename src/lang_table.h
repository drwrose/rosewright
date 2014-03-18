#ifndef LANG_TABLE_H
#define LANG_TABLE_H

typedef struct {
  const char *locale_name;
  const char *weekday_names[7];
} LangDef;

extern LangDef lang_table[];
extern int num_langs;

#endif
