#ifndef PREFS_H
#define PREFS_H

extern struct prefs_struct {
  int fastquit;
  int sizeborder;
  int forcemove;
  int borderwidth;
  int autoraise;
  char *icondir, *module_path, *defaulticon;
} prefs;

#define FM_MANUAL 0
#define FM_ALWAYS 1
#define FM_AUTO 2

#endif
