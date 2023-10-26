extern struct prefs_struct {
  int fastquit;
  int sizeborder;
  int forcemove;
  char *icondir, *defaulticon;
} prefs;

#define FM_MANUAL 0
#define FM_ALWAYS 1
#define FM_AUTO 2
