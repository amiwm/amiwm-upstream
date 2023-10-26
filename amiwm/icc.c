#include "drawinfo.h"
#include "screen.h"
#include "icc.h"
#include "icon.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

extern void createiconicon(Icon *, XWMHints *);
extern void destroyiconicon(Icon *);
extern void setclientstate(Client *, int);

Atom wm_state, wm_change_state, wm_protocols, wm_delete, wm_take_focus, wm_colormaps, wm_name, wm_normal_hints, wm_hints, wm_icon_name, amiwm_screen;

extern Display *dpy;

void init_atoms()
{
  wm_state = XInternAtom(dpy, "WM_STATE", False);
  wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
  wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  wm_colormaps = XInternAtom(dpy, "WM_COLORMAP_WINDOWS", False);
  wm_name = XInternAtom(dpy, "WM_NAME", False);
  wm_normal_hints = XInternAtom(dpy, "WM_NORMAL_HINTS", False);
  wm_hints = XInternAtom(dpy, "WM_HINTS", False);
  wm_icon_name = XInternAtom(dpy, "WM_ICON_NAME", False);
  amiwm_screen = XInternAtom(dpy, "AMIWM_SCREEN", False);
}

void setstringprop(Window w, Atom a, char *str)
{
    XTextProperty txtp;

    txtp.value=str;
    txtp.encoding=XA_STRING;
    txtp.format=8;
    txtp.nitems=strlen(str);
    XSetTextProperty(dpy, w, &txtp, a);
}

void sendcmessage(Window w, Atom a, long x)
{
  XEvent ev;
  int status;

  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = w;
  ev.xclient.message_type = a;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = x;
  ev.xclient.data.l[1] = CurrentTime;
  if(!(XSendEvent(dpy, w, False, 0L, &ev)))
    XBell(dpy, 100);
}

unsigned long _getprop(Window w, Atom a, Atom type, long len, char **p)
{
  Atom real_type;
  int format;
  unsigned long n, extra;
  int status;
  
  status = XGetWindowProperty(dpy, w, a, 0L, len, False, type, &real_type, &format,
			      &n, &extra, (unsigned char **)p);
  if (status != Success || *p == 0)
    return -1;
  if (n == 0)
    XFree((void*) *p);
  return n;
}

void getproto(Client *c)
{
  Atom *p;
  int i;
  long n;
  Window w;

  w = c->window;
  c->proto &= ~(Pdelete|Ptakefocus);
  if ((n = _getprop(w, wm_protocols, XA_ATOM, 20L, (char**)&p)) <= 0)
    return;

  for (i = 0; i < n; i++)
    if (p[i] == wm_delete)
      c->proto |= Pdelete;
    else if (p[i] == wm_take_focus)
      c->proto |= Ptakefocus;
  
  XFree((char *) p);
}

void propertychange(Client *c, Atom a)
{
  extern void checksizehints(Client *);
  extern void newicontitle(Client *);

  if(a==wm_name) {
    if(c->title.value)
      XFree(c->title.value);
    XGetWMName(dpy, c->window, &c->title);
    if(c->drag) {
      XClearWindow(dpy, c->drag);
      redraw(c, c->drag);
    }
  } else if(a==wm_normal_hints) {
    checksizehints(c);
  } else if(a==wm_hints) {
    XWMHints *xwmh;
    if((xwmh=XGetWMHints(dpy, c->window))) {
      if((xwmh->flags&(IconWindowHint|IconPixmapHint))&&c->icon) {
	destroyiconicon(c->icon);
	createiconicon(c->icon, xwmh);
      }
      if((xwmh->flags&IconPositionHint)&&c->icon&&c->icon->window) {
	XMoveWindow(dpy, c->icon->window, xwmh->icon_x, xwmh->icon_y);
	adjusticon(c->icon);
      }
      XFree(xwmh);
    }
  } else if(a==wm_protocols) {
    getproto(c);
  } else if(a==wm_icon_name) {
    if(c->icon) newicontitle(c);
  } else if(a==wm_state) {
    if(c->parent==c->scr->root) {
      getstate(c);
      if(c->state==NormalState)
	c->state=WithdrawnState;
    }
  }
}

void handle_client_message(Client *c, XClientMessageEvent *xcme)
{
  if(xcme->message_type == wm_change_state) {
    int state=xcme->data.l[0];
    if(state==IconicState)
      if(c->state!=IconicState) {
	if(!(c->icon))
	  createicon(c);
	XUnmapWindow(dpy, c->parent);
	XUnmapWindow(dpy, c->window);
	adjusticon(c->icon);
	XMapWindow(dpy, c->icon->window);
	XMapWindow(dpy, c->icon->labelwin);
	setclientstate(c, IconicState);
      } else ;
    else
      if(c->state==IconicState && c->icon) {
	Icon *i=c->icon;
	if(i->labelwin)
	  XUnmapWindow(dpy, i->labelwin);
	if(i->window)
	  XUnmapWindow(dpy, i->window);
	deselecticon(i);
	XMapWindow(dpy, c->window);
	if(c->parent!=c->scr->root)
	  XMapRaised(dpy, c->parent);
	setclientstate(c, NormalState);
      }
  }
}
