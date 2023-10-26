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
  int labelwidth;
  int selected;
} Icon;

#endif
