#ifndef LANG_TABLE_H
#define LANG_TABLE_H

typedef struct {
  unsigned char font_index;
  unsigned char date_name_id;
} LangDef;

extern LangDef lang_table[];
extern int num_langs;

#endif
