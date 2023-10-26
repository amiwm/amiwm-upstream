#ifndef CLIENT_H
#define CLIENT_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _Client {
  struct _Client *next;
  Scrn *scr;
  struct _Icon *icon;
  Window window, parent;
  Window close, drag, iconify, zoom, depth, resize;
  Window clicked;
  Colormap colormap;
  int x, y, pwidth, pheight, dragw, framewidth, frameheight;
  int zoomx, zoomy, zoomw, zoomh;
  int old_bw, proto, state, gravity, reparenting;
  int active;
  XTextProperty title;
  XSizeHints sizehints;
} Client;

extern Client *clients;

extern Client *getclient(Window);
extern Client *getclientbyicon(Window);
extern Client *createclient(Window);
extern void rmclient(Client *);
extern void flushclients(void);

#endif
