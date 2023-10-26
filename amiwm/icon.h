#ifndef ICON_H
#define ICON_H

#include "client.h"

typedef struct _Icon {
  struct _Icon *next, *nextselected;
  Scrn *scr;
  Client *client;
  Window window, labelwin, innerwin;
  Pixmap iconpm, secondpm;
  XTextProperty label;
  int x, y, width, height;
  int labelwidth;
  int selected, mapped;
} Icon;

#endif
