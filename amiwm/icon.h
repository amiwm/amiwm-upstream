#ifndef ICON_H
#define ICON_H

#include "client.h"

typedef struct _Icon {
  struct _Icon *next, *nextselected;
  Scrn *scr;
  Client *client;
  struct module *module;
  Window parent, window, labelwin, innerwin;
  Pixmap iconpm, secondpm;
  XTextProperty label;
  int x, y, width, height;
  int labelwidth;
  int selected, mapped;
} Icon;

extern void redrawicon(Icon *, Window);
extern void rmicon(Icon *);
extern void createicon(Client *);
extern void createiconicon(Icon *i, XWMHints *);
extern void destroyiconicon(Icon *);
extern void cleanupicons();
extern void createdefaulticons();
extern void adjusticon(Icon *);
extern void selecticon(Icon *);
extern void deselecticon(Icon *);

#endif
