LangDef lang_table[16] = {
  { "en_US", { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", } }, // 0 = English
  { "fr_FR", { "Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", } }, // 1 = French
  { "it_IT", { "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab", } }, // 2 = Italian
  { "es_ES", { "dom", "lun", "mar", "\x6d\x69\xc3\xa9", "jue", "vie", "\x73\xc3\xa1\x62", } }, // 3 = Spanish
  { "pt_PT", { "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "\x53\xc3\xa1\x62", } }, // 4 = Portuguese
  { "de_DE", { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa", } }, // 5 = German
  { "nl_NL", { "zo", "ma", "di", "wo", "do", "vr", "za", } }, // 6 = Dutch
  { "da_DK", { "\x53\xc3\xb8\x6e", "Man", "Tir", "Ons", "Tor", "Fre", "\x4c\xc3\xb8\x72", } }, // 7 = Danish
  { "sv_SE", { "\x53\xc3\xb6\x6e", "\x4d\xc3\xa5\x6e", "Tis", "Ons", "Tor", "Fre", "\x4c\xc3\xb6\x72", } }, // 8 = Swedish
  { "no_NO", { "\x73\xc3\xb8\x6e", "man", "tir", "ons", "tor", "fre", "\x6c\xc3\xb8\x72", } }, // 9 = Norwegian
  { "is_IS", { "sun", "\x6d\xc3\xa1\x6e", "\xc3\xbe\x72\x69", "\x6d\x69\xc3\xb0", "fim", "\x66\xc3\xb6\x73", "lau", } }, // 10 = Icelandic
  { "el_GR", { "\xce\x9a\xcf\x85\xcf\x81", "\xce\x94\xce\xb5\xcf\x85", "\xce\xa4\xcf\x81\xce\xb9", "\xce\xa4\xce\xb5\xcf\x84", "\xce\xa0\xce\xb5\xce\xbc", "\xce\xa0\xce\xb1\xcf\x81", "\xce\xa3\xce\xb1\xce\xb2", } }, // 11 = Greek
  { "hu_HU", { "Vas", "\x48\xc3\xa9\x74", "Ked", "Sze", "\x43\x73\xc3\xbc", "\x50\xc3\xa9\x6e", "Szo", } }, // 12 = Hungarian
  { "ru_RU", { "\xd0\xb2\xd1\x81", "\xd0\xbf\xd0\xbd", "\xd0\xb2\xd1\x82", "\xd1\x81\xd1\x80", "\xd1\x87\xd1\x82", "\xd0\xbf\xd1\x82", "\xd1\x81\xd0\xb1", } }, // 13 = Russian
  { "pl_PL", { "ndz", "pon", "wto", "\xc5\x9b\x72\x6f", "czw", "ptk", "sob", } }, // 14 = Polish
  { "cs_CZ", { "ne", "po", "\xc3\xba\x74", "st", "\xc4\x8d\x74", "\x70\xc3\xa1", "so", } }, // 15 = Czech
};

int num_langs = 16;

// Requires 72 chars: [67, 68, 70, 71, 72, 74, 75, 76, 77, 79, 80, 81, 83, 84, 86, 87, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 114, 115, 116, 117, 118, 119, 120, 122, 225, 229, 233, 240, 246, 248, 250, 252, 254, 269, 347, 916, 922, 928, 931, 932, 945, 946, 949, 953, 956, 961, 964, 965, 1073, 1074, 1085, 1087, 1088, 1089, 1090, 1095]
