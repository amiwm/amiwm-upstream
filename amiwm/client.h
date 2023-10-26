#ifndef CLIENT_H
#define CLIENT_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _Client {
  struct _Client *next;
  Window window, parent, iconwin, iconlabelwin;
  Window close, drag, icon, zoom, depth, resize;
  Window clicked;
  GC gc;
  Colormap colormap;
  Pixmap iconpm;
  int x, y, pwidth, pheight, dragw, framewidth, frameheight, iconlabelwidth;
  int zoomx, zoomy, zoomw, zoomh;
  int old_bw, proto, state, gravity, reparenting;
  int active;
  XTextProperty title, iconlabel;
  XSizeHints sizehints;
} Client;

extern Client *clients;

extern Client *getclient(Window);
extern Client *getclientbyicon(Window);
extern Client *createclient(Window);
extern void rmclient(Client *);
extern void flushclients(void);

#endif
