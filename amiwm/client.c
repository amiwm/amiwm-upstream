#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>

#include "client.h"
#include "icc.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

extern Display *dpy;
extern Window root;
extern int bh;

Client *clients=NULL;

Client *getclient(Window w)
{
  Client *c;

  if (w == 0 || w == root) return 0;
  for (c = clients; c; c = c->next)
    if (c->window == w || c->parent == w || c->close == w || c->drag == w ||
	c->icon == w || c->zoom == w || c->depth == w || c->resize == w)
      return c;
  return 0;
}

Client *getclientbyicon(Window w)
{
  Client *c;
  
  if (w == 0 || w == root) return 0;
  for (c = clients; c; c = c->next)
    if (c->iconwin == w || c->iconlabelwin == w)
      return c;
  return 0;
}

void grav_map_frame_to_win(Client *c, int x0, int y0, int *x, int *y)
{
  switch(c->gravity) {
  case EastGravity:
  case NorthEastGravity:
  case SouthEastGravity:
    *x=x0+c->framewidth-c->old_bw-c->old_bw; break;
  case CenterGravity:
  case NorthGravity:
  case SouthGravity:
    *x=x0+4-c->old_bw; break;
  case WestGravity:
  case NorthWestGravity:
  case SouthWestGravity:
  default:
    *x=x0; break;
  }
  switch(c->gravity) {
  case SouthGravity:
  case SouthEastGravity:
  case SouthWestGravity:
    *y=y0+c->frameheight-c->old_bw-c->old_bw; break;
  case CenterGravity:
  case EastGravity:
  case WestGravity:
    *y=y0+bh-c->old_bw; break;
  case NorthGravity:
  case NorthEastGravity:
  case NorthWestGravity:
  default:
    *y=y0; break;
  }
}

void grav_map_win_to_frame(Client *c, int x0, int y0, int *x, int *y)
{
  switch(c->gravity) {
  case EastGravity:
  case NorthEastGravity:
  case SouthEastGravity:
    *x=x0-c->framewidth+c->old_bw+c->old_bw; break;
  case CenterGravity:
  case NorthGravity:
  case SouthGravity:
    *x=x0-4+c->old_bw; break;
  case WestGravity:
  case NorthWestGravity:
  case SouthWestGravity:
  default:
    *x=x0; break;
  }
  switch(c->gravity) {
  case SouthGravity:
  case SouthEastGravity:
  case SouthWestGravity:
    *y=y0-c->frameheight+c->old_bw+c->old_bw; break;
  case CenterGravity:
  case EastGravity:
  case WestGravity:
    *y=y0-bh+c->old_bw; break;
  case NorthGravity:
  case NorthEastGravity:
  case NorthWestGravity:
  default:
    *y=y0; break;
  }
}

void sendconfig(Client *c)
{
  extern int bh;
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.event = c->window;
  ce.window = c->window;
/*
   grav_map_frame_to_win(c, c->x, c->y, &ce.x, &ce.y);
*/
  ce.x = c->x+4;
  ce.y = c->y+bh;
  ce.width = c->pwidth-c->framewidth;
  ce.height = c->pheight-c->frameheight;
  ce.border_width = c->old_bw;
  ce.above = None;
  ce.override_redirect = 0;
  XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent*)&ce);
}

void checksizehints(Client *c)
{
  long supplied;

  XGetWMNormalHints(dpy, c->window, &c->sizehints, &supplied);
  if(!(c->sizehints.flags&PMinSize))
    c->sizehints.min_width=c->sizehints.min_height=0;
  if(!(c->sizehints.flags&PMaxSize))
    c->sizehints.max_width=c->sizehints.max_height=1<<30;
  if(!(c->sizehints.flags&PResizeInc))
    c->sizehints.width_inc=c->sizehints.height_inc=1;
  if(c->sizehints.flags&PBaseSize) {
    c->sizehints.min_width=c->sizehints.base_width;
    c->sizehints.min_height=c->sizehints.base_height;
  }
  if(c->sizehints.min_width<1) c->sizehints.min_width=1;
  if(c->sizehints.min_height<1) c->sizehints.min_height=1;
  c->sizehints.base_width=c->sizehints.min_width;
  c->sizehints.base_height=c->sizehints.min_height;
  if(c->sizehints.flags&PWinGravity) c->gravity=c->sizehints.win_gravity;
}

void setclientstate(Client *c, int state)
{
  long data[2];

  data[0] = (long) state;
  data[1] = (long) None;

  c->state = state;
  XChangeProperty(dpy, c->window, wm_state, wm_state, 32,
		  PropModeReplace, (unsigned char *)data, 2);
}

void getstate(Client *c)
{
  long *data=NULL;

  if(_getprop(c->window, wm_state, wm_state, 2l, (char **)&data)>0) {
    c->state=*data;
    XFree((char *)data);
  }
}

Client *createclient(Window w)
{
  XWindowAttributes attr, attr2;
  Client *c;
  extern int bh;

  if(w==0 || w==root) return 0;
  if(c=getclient(w)) return c;

  XGetWindowAttributes(dpy, w, &attr);

  c = (Client *)calloc(1, sizeof(Client));
  c->window = w;
  c->parent = root;
  c->old_bw = attr.border_width;
  c->next = clients;
  c->state = WithdrawnState;
  c->gravity = NorthWestGravity;
  c->reparenting = 0;
  XSelectInput(dpy, c->window, PropertyChangeMask);
  XGetWMName(dpy, c->window, &c->title);
  checksizehints(c);
  c->zoomx=c->zoomy=0;
  XGetWindowAttributes(dpy, root, &attr2);
  if(c->sizehints.width_inc) {
    c->zoomw=attr2.width-c->sizehints.base_width-22;
    c->zoomw-=c->zoomw%c->sizehints.width_inc;
    c->zoomw+=c->sizehints.base_width;
    if(c->zoomw>c->sizehints.max_width)
      c->zoomw=c->sizehints.max_width;
    if(c->zoomw<c->sizehints.min_width)
      c->zoomw=c->sizehints.min_width;
  } else
    c->zoomw=attr.width;
  if(c->sizehints.height_inc) {
    c->zoomh=attr2.height-c->sizehints.base_height-bh-10;
    c->zoomh-=c->zoomh%c->sizehints.height_inc;
    c->zoomh+=c->sizehints.base_height;
    if(c->zoomh>c->sizehints.max_height)
      c->zoomh=c->sizehints.max_height;
    if(c->zoomh<c->sizehints.min_height)
      c->zoomh=c->sizehints.min_height;
  } else
    c->zoomh=attr.height;
  return clients = c;
}

void rmclient(Client *c)
{
    Client *cc;
    int i;

    if (c == clients)
      clients = c->next;
    else
      if(cc = clients)
	for (; cc->next; cc = cc->next)
	  if (cc->next == c) {
            cc->next = cc->next->next;
	    break;
	  }

    if(c->title.value)
      XFree(c->title.value);
    if(c->parent != root)
      XDestroyWindow(dpy, c->parent);
    if(c->iconwin) {
      Window r,p,*ch;
      unsigned int nc;
      if(XQueryTree(dpy, c->iconwin, &r, &p, &ch, &nc)) {
	if(nc) {
	  XWindowAttributes attr;
	  XGetWindowAttributes(dpy, c->iconwin, &attr);
	  while(nc--) {
	    Window w=ch[nc];
	    XUnmapWindow(dpy, w);
	    XReparentWindow(dpy, w, root, attr.x+4, attr.y+4);
	    XRemoveFromSaveSet(dpy, w);
	  }
	}
	XFree(ch);
      }
      XDestroyWindow(dpy, c->iconwin);
    }
    if(c->iconlabelwin)
      XDestroyWindow(dpy, c->iconlabelwin);
    if(c->iconlabel.value)
      XFree(c->iconlabel.value);
    free(c);
}

void flushclients()
{
  unsigned int i, nwins;
  Window dw1, dw2, *wins;
  Client *c;

  XQueryTree(dpy, root, &dw1, &dw2, &wins, &nwins);
  for(i=0; i<nwins; i++)
    if((c=getclient(wins[i]))&&wins[i]==c->parent) {
      int x,y;
      grav_map_frame_to_win(c, c->x, c->y, &x, &y);
      XReparentWindow(dpy, c->window, root, x, y);
      XSetWindowBorderWidth(dpy, c->window, c->old_bw);
      XRemoveFromSaveSet(dpy, c->window);
      wins[i]=c->window;
      rmclient(c);
    }
/*
  if(nwins) {
    for(i=0; i<(nwins>>1); i++) {
      Window w=wins[i];
      wins[i]=wins[nwins-1-i];
      wins[nwins-1-i]=w;
    }
    XRestackWindows(dpy, wins, nwins);
  }
*/
  XFree((void *) wins);
  while(c=clients) {
    if(c->parent != root) {
      int x,y;
      grav_map_frame_to_win(c, c->x, c->y, &x, &y);
      XReparentWindow(dpy, c->window, root, x, y);
      XSetWindowBorderWidth(dpy, c->window, c->old_bw);
      XRemoveFromSaveSet(dpy, c->window);
    }
    rmclient(c);
  }
}
