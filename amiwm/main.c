#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#ifdef AMIGAOS
#include <x11/xtrans.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "drawinfo.h"
#include "screen.h"
#include "icon.h"
#include "client.h"
#include "prefs.h"
#include "module.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;

struct timeval {
  long tv_sec;
  long tv_usec;
};

#define fd_set XTransFdset
#undef FD_ZERO
#undef FD_SET
#define FD_ZERO XTransFdZero
#define FD_SET XTransFdSet
#define select XTransSelect
#endif

#define HYSTERESIS 5

typedef struct _DragIcon {
  Icon *icon;
  Window w;
  Pixmap pm;
  int x, y;
} DragIcon;

Display *dpy;
char *progname;
Cursor wm_curs;
int signalled=0, forcemoving=0;
Client *dragclient=NULL, *resizeclient=NULL;
Client *rubberclient=NULL, *clickclient=NULL, *doubleclient=NULL;
Scrn *boundingscr=NULL; Window boundingwin=None;
DragIcon *dragiconlist=NULL;
int numdragicons=0;
Window clickwindow=None;
int rubberx, rubbery, rubberh, rubberw, rubberx0, rubbery0, olddragx, olddragy;
Time last_icon_click=0, last_double=0;
static int initting=0, ignore_badwindow=0;
int dblClickTime=1500;
XContext client_context, screen_context, icon_context, menu_context;
Scrn *dragscreen=NULL, *menuactive=NULL;
static int d_offset=0;
static fd_set master_fd_set;
static int max_fd=0;

unsigned int meta_mask, switch_mask;

static char **main_argv;

extern Scrn *mbdclick, *mbdscr;

extern void reparent(Client *);
extern void redraw(Client *, Window);
extern void redrawclient(Client *);
extern void redrawmenubar(Window);
extern void gadgetclicked(Client *c, Window w, XEvent *e);
extern void gadgetunclicked(Client *c, XEvent *e);
extern void gadgetaborted(Client *c);
extern void clickenter(void);
extern void clickleave(void);
extern void adjusticon(Icon *i);
extern void cleanupicons();
extern void propterychange(Client *c, Atom a);
extern void setclientstate(Client *, int);
extern void menu_on(void);
extern void menu_off(void);
extern void menubar_enter(Window);
extern void menubar_leave(Window);
extern void *getitembyhotkey(char);
extern void menuaction(void *);
extern void screentoback();
extern void openscreen(char *, Window);
extern void realizescreens(void);
extern Scrn *getscreenbyroot(Window);
extern void assimilate(Window, int, int);
extern void selecticon(Icon *);
extern void deselecticon(Icon *);
extern void deselect_all_icons(Scrn *);
extern void reparenticon(Icon *, Scrn *, int, int);
extern void handle_client_message(Client *, XClientMessageEvent *);
extern void handle_module_input(fd_set *);
extern int dispatch_event_to_broker(XEvent *, unsigned long, struct module *);

#ifndef AMIGAOS
void restart_amiwm()
{
  flushmodules();
  flushclients();
  XFlush(dpy);
  XCloseDisplay(dpy);
  execvp(main_argv[0], main_argv);
}
#endif

int handler(Display *d, XErrorEvent *e)
{
  if (initting && (e->request_code == X_ChangeWindowAttributes) &&
      (e->error_code == BadAccess)) {
    fprintf(stderr, "%s: Another window manager is already running.  Not started.\n", progname);
    exit(1);
  }
  
  if (ignore_badwindow &&
      (e->error_code == BadWindow || e->error_code == BadColor))
    return 0;

  if (e->error_code == BadMatch && e->request_code == X_ChangeSaveSet)
    return 0;
  
  XmuPrintDefaultErrorMessage(d, e, stderr);
  
  if (initting) {
    fprintf(stderr, "%s: failure during initialisation; aborting\n",
	    progname);
    exit(1);
  }
  return 0;
}

static struct coevent {
  struct coevent *next;
  struct timeval when;
  void (*what)(void *);
  void *with;
} *eventlist=NULL;

#define FIXUPTV(tv) { \
    while((tv).tv_usec<0) { (tv).tv_usec+=1000000; (tv).tv_sec--; } \
    while((tv).tv_usec>=1000000) { (tv).tv_usec-=1000000; (tv).tv_sec++; } \
}

void remove_call_out(void (*what)(void *), void *with)
{
  struct coevent *ee, **e=&eventlist;

  while(*e && ((*e)->what != what || (*e)->with))
    e=&(*e)->next;
  if(ee=*e) {
    *e=(*e)->next;
    free(ee);
  }
}

#ifdef BSD_STYLE_GETTIMEOFDAY
#define GETTIMEOFDAY(tp) gettimeofday(tp, NULL)
#else
#define GETTIMEOFDAY(tp) gettimeofday(tp)
#endif

void call_out(int howlong, void (*what)(void *), void *with)
{
  struct coevent *ce=malloc(sizeof(struct coevent));
  if(ce) {
    struct coevent **e=&eventlist;
    GETTIMEOFDAY(&ce->when);
    ce->when.tv_sec+=howlong/1000;
    ce->when.tv_usec+=1000*(howlong%1000);
    FIXUPTV(ce->when);
    ce->what=what;
    ce->with=with;
    while(*e && ((*e)->when.tv_sec<ce->when.tv_sec ||
		 ((*e)->when.tv_sec==ce->when.tv_sec &&
		  (*e)->when.tv_usec>=ce->when.tv_usec)))
      e=&(*e)->next;
    ce->next=*e;
    *e=ce;
  }
}

static void call_call_out()
{
  struct timeval now;
  struct coevent *e;
  GETTIMEOFDAY(&now);
  FIXUPTV(now);
  while((e=eventlist) && (e->when.tv_sec<now.tv_sec ||
			  (e->when.tv_sec==now.tv_sec &&
			   e->when.tv_usec<=now.tv_usec))) {
    eventlist=e->next;
    (e->what)(e->with);
    free(e);
  }
}

static void fill_in_call_out(struct timeval *tv)
{
  GETTIMEOFDAY(tv);
  tv->tv_sec=eventlist->when.tv_sec-tv->tv_sec;
  tv->tv_usec=eventlist->when.tv_usec-tv->tv_usec;
  FIXUPTV(*tv);
  if(tv->tv_sec<0)
    tv->tv_sec = tv->tv_usec = 0;
}

void add_fd_to_set(int fd)
{
  FD_SET(fd, &master_fd_set);
  if(fd>=max_fd)
    max_fd=fd+1;
}

void remove_fd_from_set(int fd)
{
  FD_CLR(fd, &master_fd_set);
}

void lookup_keysyms()
{
  int i,j,k,maxsym,mincode,maxcode;
  XModifierKeymap *map=XGetModifierMapping(dpy);
  KeySym *kp, *kmap;
  meta_mask=0, switch_mask=0;
  XDisplayKeycodes(dpy, &mincode, &maxcode);
  kmap=XGetKeyboardMapping(dpy, mincode, maxcode-mincode+1, &maxsym);
  for(i=3; i<8; i++)
    for(j=0; j<map->max_keypermod; j++)
      for(kp=kmap+(map->modifiermap[i*map->max_keypermod+j]-mincode)*maxsym,
	  k=0; k<maxsym; k++)
	switch(*kp++) {
	case XK_Meta_L:
	case XK_Meta_R:
	  meta_mask|=1<<i;
	  break;
	case XK_Mode_switch:
	  switch_mask|=1<<i;
	  break;
	}
  XFree(kmap);
  XFreeModifiermap(map);
}


void restorescreentitle(Scrn *s)
{
  (scr=s)->title=s->deftitle;
  XClearWindow(dpy, s->menubar);
  redrawmenubar(s->menubar);  
}

void wberror(Scrn *s, char *message)
{
  remove_call_out((void(*)(void *))restorescreentitle, s);
  (scr=s)->title=message;
  XClearWindow(dpy, s->menubar);
  redrawmenubar(s->menubar);
  XBell(dpy, 100);
  call_out(2000, (void(*)(void *))restorescreentitle, s);
}

void drawrubber()
{
  if(rubberclient)
    XDrawRectangle(dpy, rubberclient->scr->back, rubberclient->scr->rubbergc,
		   rubberx, rubbery, rubberw-1, rubberh-1);
  else if(boundingwin) {
    const char dash_list[] = { 6, 6 };
    int x=rubberx, y=rubbery, w=rubberw, h=rubberh;
    if(w<0) { x+=w; w=-w; }
    if(h<0) { y+=h; h=-h; }
    if(w>=HYSTERESIS || h>=HYSTERESIS) {
      XSetDashes(dpy, boundingscr->rubbergc, d_offset,
		 dash_list, sizeof(dash_list));
      XSetLineAttributes(dpy, boundingscr->rubbergc, 0, LineOnOffDash,
			 CapButt, JoinMiter);
      XDrawRectangle(dpy, boundingwin, boundingscr->rubbergc, x, y, w, h);
      XSetLineAttributes(dpy, boundingscr->rubbergc, 0, LineSolid,
			 CapButt, JoinMiter);
    }
  }
}

static void move_dashes(void *dontcare)
{
  call_out(50, move_dashes, dontcare);
  drawrubber();
  if((--d_offset)<0)
    d_offset=7;
  drawrubber();
}

void endrubber()
{
  if(rubberclient) {
    drawrubber();
    rubberclient=NULL;
  } else if(boundingwin) {
    drawrubber();
    boundingwin=None;
  }
}

void initrubber(int x0, int y0, Client *c)
{
  endrubber();
  rubberx=c->x;
  rubbery=c->y;
  rubberw=c->pwidth;
  rubberh=c->pheight;
  rubberx0=x0;
  rubbery0=y0;
  rubberclient=c;
}

void abortrubber()
{
  if(rubberclient) {
    endrubber();
    dragclient=resizeclient=NULL;
    XUngrabServer(dpy);
    XUngrabPointer(dpy, CurrentTime);
  } else if(boundingwin) {
    endrubber();
    boundingwin=None;
    boundingscr=NULL;
    XUngrabServer(dpy);
    XUngrabPointer(dpy, CurrentTime);
  }
}

void startbounding(Scrn *s, Window w, XEvent *e)
{
  last_icon_click=e->xbutton.time;
  boundingscr=s;
  boundingwin=w;
  XGrabPointer(dpy, w, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
  XGrabServer(dpy);
  rubberx=e->xbutton.x;
  rubbery=e->xbutton.y;
  rubberx0=e->xbutton.x_root;
  rubbery0=e->xbutton.y_root;
  rubberw=0;
  rubberh=0;
  drawrubber();
  call_out(0, move_dashes, NULL);
}

void endbounding(XEvent *e)
{
  Icon *i;
  XWindowAttributes attr;

  if(boundingscr) {
    remove_call_out(move_dashes, NULL);
    endrubber();
    if(!(e->xbutton.state & ShiftMask))
      deselect_all_icons(boundingscr);

    if(rubberw<0) {
      rubberx+=rubberw;
      rubberw=-rubberw;
    }
    if(rubberh<0) {
      rubbery+=rubberh;
      rubberh=-rubberh;
    }
    if(rubberw>=HYSTERESIS || rubberh>=HYSTERESIS)
      for(i=boundingscr->icons; i; i=i->next)
	if(i->window && XGetWindowAttributes(dpy, i->window, &attr) &&
	   attr.map_state==IsViewable &&
	   attr.x<rubberx+rubberw && attr.y<rubbery+rubberh &&
	   attr.x+attr.width>rubberx && attr.y+attr.height>rubbery)
	  selecticon(i);
    boundingscr=NULL;
    XUngrabServer(dpy);
    XUngrabPointer(dpy, CurrentTime);
  }
}

void startdragging(Client *c, XEvent *e)
{
  scr=c->scr;
  dragclient=c;
  XGrabPointer(dpy, c->drag, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
  XGrabServer(dpy);
  initrubber(e->xbutton.x_root, e->xbutton.y_root, c);
  rubberx0-=rubberx;
  rubbery0-=rubbery;
  if(!forcemoving) {
    if(rubberx+rubberw>scr->width)
      rubberx=scr->width-rubberw;
    if(rubbery+rubberh>scr->height)
      rubbery=scr->height-rubberh;
    if(rubberx<0)
      rubberx=0;
    if(rubbery<0)
      rubbery=0;
  }
  drawrubber();
}

void startscreendragging(Scrn *s, XEvent *e)
{
  dragscreen=s;
  XGrabPointer(dpy, s->menubar, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
  olddragy=rubbery=s->y;
  rubbery0=e->xbutton.y_root-rubbery;
}

void endscreendragging()
{
  Scrn *s;
  if(s=dragscreen) {
#ifndef ASSIMILATE_WINDOWS
    scrsendconfig(dragscreen);
#endif
    dragscreen=NULL;
    XUngrabPointer(dpy, CurrentTime);
  }
}

void abortscreendragging()
{
  if(dragscreen) {
    XMoveWindow(dpy, dragscreen->back, -dragscreen->bw,
		(dragscreen->y=olddragy)-dragscreen->bw);
    endscreendragging();
  }
}

void aborticondragging()
{
  if(numdragicons) {
    int i;
    for(i=0; i<numdragicons; i++) {
      XDestroyWindow(dpy, dragiconlist[i].w);
      if(dragiconlist[i].pm)
	XFreePixmap(dpy, dragiconlist[i].pm);
    }
    numdragicons=0;
    free(dragiconlist);
    dragiconlist=NULL;
    XUngrabPointer(dpy, CurrentTime);
  }
}

void badicondrop()
{
  wberror(dragiconlist[0].icon->scr,
	  "Icons cannot be moved into this window");
  aborticondragging();
}

void endicondragging(XEvent *e)
{
  int i;
  Client *c;

  scr=front;
  for(;;) {
    if(scr->root == e->xbutton.root && e->xbutton.y_root>=scr->y)
      break;
    if((scr=scr->behind)==front) {
      badicondrop();
      return;
    }
  }
  for(c=clients; c; c=c->next)
    if(c->scr == scr && c->state == NormalState && e->xbutton.x_root>=c->x &&
       e->xbutton.y_root>=c->y+scr->y &&
       e->xbutton.x_root<c->x+c->pwidth &&
       e->xbutton.y_root<c->y+c->pheight+scr->y)
      break;
  if(c) {
    badicondrop();
    return;
  }

  for(i=0; i<numdragicons; i++) {
    if(dragiconlist[i].icon->scr!=scr)
      reparenticon(dragiconlist[i].icon, scr,
		   dragiconlist[i].x-4, dragiconlist[i].y-4-scr->y);
    else
      XMoveWindow(dpy, dragiconlist[i].icon->window,
		  dragiconlist[i].x-4, dragiconlist[i].y-4-scr->y);
    adjusticon(dragiconlist[i].icon);
  }
  aborticondragging();
}

void starticondragging(Scrn *scr, XEvent *e)
{
  XWindowAttributes xwa;
  XSetWindowAttributes xswa;
  Icon *i;

  aborticondragging();
  for(i=scr->firstselected; i; i=i->nextselected)
    numdragicons++;
  if(!numdragicons)
    return;

  if(!(dragiconlist=calloc(numdragicons, sizeof(DragIcon)))) {
    numdragicons=0;
    XBell(dpy, 100);
    return;
  }

  for(numdragicons=0, i=scr->firstselected; i; i=i->nextselected) {
    dragiconlist[numdragicons].icon = i;
    XGetWindowAttributes(dpy, i->window, &xwa);
    dragiconlist[numdragicons].x = xwa.x+4;
    dragiconlist[numdragicons].y = xwa.y+4+scr->y;
    xswa.save_under=True;
    xswa.override_redirect=True;
    if(i->innerwin) {
      XGetWindowAttributes(dpy, i->innerwin, &xwa);
      xswa.background_pixmap = dragiconlist[numdragicons].pm =
	XCreatePixmap(dpy, i->innerwin, xwa.width, xwa.height, xwa.depth);
      XCopyArea(dpy, i->innerwin, dragiconlist[numdragicons].pm, scr->gc,
		0, 0, xwa.width, xwa.height, 0, 0);
    } else {
      if(i->secondpm) {
	xswa.background_pixmap = i->secondpm;
	XGetGeometry(dpy, i->secondpm, &xwa.root, &xwa.x, &xwa.y,
		     &xwa.width, &xwa.height,
		     &xwa.border_width, &xwa.depth);
      } else if(i->iconpm) {
	xswa.background_pixmap = i->iconpm;
	XGetGeometry(dpy, i->iconpm, &xwa.root, &xwa.x, &xwa.y,
		     &xwa.width, &xwa.height,
		     &xwa.border_width, &xwa.depth);
      }
      if(xwa.depth!=scr->depth) {
	dragiconlist[numdragicons].pm =
	  XCreatePixmap(dpy, i->window, xwa.width, xwa.height, scr->depth);
	XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
	XSetBackground(dpy, scr->gc, scr->dri.dri_Pens[BACKGROUNDPEN]);
	XCopyPlane(dpy, xswa.background_pixmap, dragiconlist[numdragicons].pm,
		   scr->gc, 0, 0, xwa.width, xwa.height, 0, 0, 1);
	xswa.background_pixmap = dragiconlist[numdragicons].pm;
	xwa.depth = scr->depth;
      }
    }
    xswa.colormap=xwa.colormap;
    dragiconlist[numdragicons].w =
      XCreateWindow(dpy, scr->root,
		    dragiconlist[numdragicons].x,
		    dragiconlist[numdragicons].y,
		    xwa.width, xwa.height, 0,
		    xwa.depth, xwa.class, xwa.visual,
		    CWBackPixmap|CWOverrideRedirect|CWSaveUnder|CWColormap,
		    &xswa);
    XMapRaised(dpy, dragiconlist[numdragicons].w);
    numdragicons++;
  }
  XGrabPointer(dpy, scr->back, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
  olddragx = rubberx0 = e->xbutton.x_root;
  olddragy = rubbery0 = e->xbutton.y_root;
}

void enddragging()
{
  if(dragclient) {
    Client *c=dragclient;
    endrubber();
    if(rubbery<=-(c->scr->bh))
      rubbery=1-(c->scr->bh);
    XMoveWindow(dpy, c->parent, c->x=rubberx, c->y=rubbery);
    dragclient=NULL;
    XUngrabServer(dpy);
    XUngrabPointer(dpy, CurrentTime);
    sendconfig(c);
  }
}

void do_icon_double_click(Scrn *scr)
{
  Icon *i, *next;
  Client *c;

  for(i=scr->firstselected; i; i=next) {
    next=i->nextselected;
    if(i->labelwin)
      XUnmapWindow(dpy, i->labelwin);
    if(i->window)
      XUnmapWindow(dpy, i->window);
    deselecticon(i);
    if(c=(i->client)) {
      XMapWindow(dpy, c->window);
      if(c->parent!=c->scr->root)
	XMapRaised(dpy, c->parent);
      setclientstate(c, NormalState);
    }
  }
}

void startresizing(Client *c, XEvent *e)
{
  resizeclient=c;
  XGrabPointer(dpy, c->resize, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
  XGrabServer(dpy);
  initrubber(e->xbutton.x_root, e->xbutton.y_root, c);
  rubberx0-=rubberw;
  rubbery0-=rubberh;
  drawrubber();
}

void endresizing()
{
  extern void resizeclientwindow(Client *c, int, int);
  if(resizeclient) {
    Client *c=resizeclient;
    endrubber();
    resizeclientwindow(c, rubberw, rubberh);
    resizeclient=NULL;
    XUngrabServer(dpy);
    XUngrabPointer(dpy, CurrentTime);
  }
}

RETSIGTYPE sighandler(int sig)
{
  signalled=1;
  signal(sig, SIG_IGN);
}

static void instcmap(Colormap c)
{
  XInstallColormap(dpy, (c == None) ? scr->cmap : c);
}

void internal_broker(XEvent *e)
{
  int event_loc=(int)e->xany.display;
  e->xany.display=dpy;
  if(event_loc==1) {
    XSendEvent(dpy, e->xany.window, False, 0, e);
  } else switch(e->type) {
  case MappingNotify:
    if(e->xmapping.request==MappingKeyboard ||
       e->xmapping.request==MappingModifier)
      XRefreshKeyboardMapping(&e->xmapping);
    lookup_keysyms(dpy, &meta_mask, &switch_mask);
    break;
  case KeyPress:
    if(e->xkey.state & meta_mask) {
      KeySym ks=XLookupKeysym(&e->xkey,
			      ((e->xkey.state & ShiftMask)?1:0)+
			      ((e->xkey.state & switch_mask)?2:0));
      void *item;
      if(item=getitembyhotkey(ks))
	menuaction(item);
    }
    break;
  }
}

int main(int argc, char *argv[])
{
  XWindowAttributes attr;
  int x_fd;

  main_argv=argv;
  progname=argv[0];
  if(!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "%s: cannot connect to X server %s\n", progname,
	    XDisplayName(NULL));
    exit(1);
  }

  client_context = XUniqueContext();
  screen_context = XUniqueContext();
  icon_context = XUniqueContext();
  menu_context = XUniqueContext();

  wm_curs=XCreateFontCursor(dpy, XC_top_left_arrow);

  FD_ZERO(&master_fd_set);
  FD_SET((x_fd=ConnectionNumber(dpy)), &master_fd_set);
  max_fd=x_fd+1;

  init_modules();
  read_rc_file();

  initting = 1;
  XSetErrorHandler(handler);
  if (signal(SIGTERM, sighandler) == SIG_IGN)
    signal(SIGTERM, SIG_IGN);
  if (signal(SIGINT, sighandler) == SIG_IGN)
    signal(SIGINT, SIG_IGN);
#ifdef SIGHUP
  if (signal(SIGHUP, sighandler) == SIG_IGN)
    signal(SIGHUP, SIG_IGN);
#endif

  init_atoms();

#ifndef AMIGAOS
  if((fcntl(ConnectionNumber(dpy), F_SETFD, 1)) == -1)
    fprintf(stderr, "%s: child cannot disinherit TCP fd\n");
#endif

  lookup_keysyms(dpy, &meta_mask, &switch_mask);

  if(!front)
    openscreen("Workbench Screen", DefaultRootWindow(dpy));
  realizescreens();

  initting = 0;

  while(!signalled) {
    fd_set rfds;
    struct timeval t;
    XEvent event;

    while((!signalled) && QLength(dpy)>0) {
      Client *c; Icon *i;
      int motionx, motiony;
      
      XNextEvent(dpy, &event);
      if(!XFindContext(dpy, event.xany.window, client_context,
		       (XPointer*)&c)) {
	scr=c->scr;
      } else {
	c = NULL;
	if(XFindContext(dpy, event.xany.window, screen_context,
			(XPointer*)&scr))
	  scr=front;
      }
      if(XFindContext(dpy, event.xany.window, icon_context, (XPointer*)&i))
	i=NULL;
      else
	scr=i->scr;
      switch(event.type) {
      case Expose:
	if(!event.xexpose.count) {
	  if(rubberclient || boundingscr) drawrubber();
	  if(c)
	    redraw(c, event.xexpose.window);
	  else if(i)
	    redrawicon(i, event.xexpose.window);
	  else if(scr)
	    redrawmenubar(event.xexpose.window);
	  if(rubberclient || boundingscr) drawrubber();
	}
	break;
      case CreateNotify:
	if(!event.xcreatewindow.override_redirect) {
	  if(!(scr=getscreenbyroot(event.xcreatewindow.parent)))
	    scr=front;
	  createclient(event.xcreatewindow.window);
	}
#ifdef ASSIMILATE_WINDOWS
	else if(XFindContext(dpy, event.xcreatewindow.window, screen_context, (XPointer*)&scr)
		&& (scr=getscreenbyroot(event.xcreatewindow.parent))) {
	  XGetWindowAttributes(dpy, event.xcreatewindow.window, &attr);
	  assimilate(event.xcreatewindow.window, attr.x, attr.y);
	}
#endif
	break;
      case DestroyNotify:
	if(!XFindContext(dpy, event.xdestroywindow.window, client_context,
			 (XPointer*)&c)) {
	  ignore_badwindow = 1;
	  rmclient(c);
	  XSync(dpy, False);
	  ignore_badwindow = 0;
	} else if(event.xdestroywindow.window)
	  XDeleteContext(dpy, event.xdestroywindow.window, screen_context);
	break;
      case UnmapNotify:
	if(c && (event.xunmap.window==c->window)) {
	  if(c->active) {
	    c->active=False;
	    redrawclient(c);
	    if(!menuactive)
	      XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	  }
	  if((!c->reparenting) && c->parent != c->scr->root) {
	    XUnmapWindow(dpy, c->parent);
	    setclientstate(c, WithdrawnState);
	  }
	  c->reparenting = 0;
	}
	break;
      case ConfigureNotify:
      case CirculateNotify:
      case GravityNotify:
      case ReparentNotify:
      case MapNotify:
      case NoExpose:
	break;
      case ClientMessage:
	if(c)
	  handle_client_message(c, &event.xclient);
	break;
      case ColormapNotify:
	if(event.xcolormap.new && c)
	  if(c->colormap!=event.xcolormap.colormap) {
	    c->colormap=event.xcolormap.colormap;
	    if(c->active)
	      instcmap(c->colormap);
	  }
	break;
      case ConfigureRequest:
	if(c && event.xconfigurerequest.window==c->window &&
	   c->parent!=c->scr->root) {
	  extern void resizeclientwindow(Client *c, int, int);
	  if(event.xconfigurerequest.value_mask&CWBorderWidth)
	    c->old_bw=event.xconfigurerequest.border_width;
	  resizeclientwindow(c, (event.xconfigurerequest.value_mask&CWWidth)?
			     event.xconfigurerequest.width+c->framewidth:c->pwidth,
			     (event.xconfigurerequest.value_mask&CWHeight)?
			     event.xconfigurerequest.height+c->frameheight:c->pheight);
	  if((event.xconfigurerequest.value_mask&(CWX|CWY)) &&
	     c->state==WithdrawnState)
	    XMoveWindow(dpy, c->parent,
			c->x=((event.xconfigurerequest.value_mask&CWX)?
			      event.xconfigurerequest.x:c->x),
			c->y=((event.xconfigurerequest.value_mask&CWY)?
			      event.xconfigurerequest.y:c->y));
	} else {
	  if(!XFindContext(dpy, event.xconfigurerequest.window,
			    screen_context, (XPointer *)&scr))
	    if((event.xconfigurerequest.y-=scr->y)<0)
	      event.xconfigurerequest.y=0;
	  XConfigureWindow(dpy, event.xconfigurerequest.window,
			   event.xconfigurerequest.value_mask,
			   (XWindowChanges *)&event.xconfigurerequest.x);
	}
	break;
      case CirculateRequest:
	if(XFindContext(dpy, event.xcirculaterequest.window, client_context, (XPointer*)&c))
	  if(event.xcirculaterequest.place==PlaceOnTop)
	    XRaiseWindow(dpy, event.xcirculaterequest.window);
	  else {
	    Client *c2;
	    Window r,p,*children;
	    unsigned int nchildren;
	    if(XQueryTree(dpy, scr->back, &r, &p, &children, &nchildren)) {
	      int n;
	      for(n=0; n<nchildren; n++)
		if((!XFindContext(dpy, children[n], client_context, (XPointer*)&c2)) &&
		   children[n]==c2->parent)
		  break;
	      if(n<nchildren) {
		Window ws[2];
		ws[0]=children[n];
		ws[1]=event.xcirculaterequest.window;
		XRestackWindows(dpy, ws, 2);
	      }
	    }
	  }
	else
	  raiselowerclient(c, event.xcirculaterequest.place);
	break;
      case MapRequest:
	if(XFindContext(dpy, event.xmaprequest.window, client_context, (XPointer*)&c))
	  XMapWindow(dpy, event.xmaprequest.window);
	else {
	  XWMHints *xwmh;
	  if(c->parent==c->scr->root && (xwmh=XGetWMHints(dpy, c->window))) {
	    if(c->state==WithdrawnState && (xwmh->flags&StateHint)
	       && xwmh->initial_state==IconicState)
	      c->state=IconicState;
	    XFree(xwmh);
	  }
	  switch(c->state) {
	  case WithdrawnState:
	    if(c->parent == c->scr->root)
	      reparent(c);
	  case NormalState:
	    XMapWindow(dpy, c->window);
	    XMapRaised(dpy, c->parent);
	    setclientstate(c, NormalState);
	    break;
	  case IconicState:
	    if(c->parent == c->scr->root)
	      reparent(c);
	    if(!(c->icon))
	      createicon(c);
	    adjusticon(c->icon);
	    XMapWindow(dpy, c->icon->window);
	    XMapWindow(dpy, c->icon->labelwin);
	    break;
	  }
	}
	break;
      case EnterNotify:
	if(menuactive) {
	  scr=menuactive;
	  menubar_enter(event.xcrossing.window);
	} else if(clickwindow && event.xcrossing.window == clickwindow)
	  clickenter();
	else if(c) {
	  if((!c->active) && (c->state==NormalState)) {
	    XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
	    c->active=True;
	    redrawclient(c);
	    if(prefs.autoraise && c->parent!=c->scr->root)
	      XRaiseWindow(dpy, c->parent);
	  }
	  if(event.xcrossing.window==c->window)
	    instcmap(c->colormap);
	}
	break;
      case LeaveNotify:
	if(menuactive) {
	  scr=menuactive;
	  menubar_leave(event.xcrossing.window);
	} else if(clickwindow && event.xcrossing.window == clickwindow)
	  clickleave();
	else if(c) {
	  if(c->active && event.xcrossing.window==c->parent &&
	     event.xcrossing.detail!=NotifyInferior) {
	    if(!menuactive)
	      XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	    c->active=False;
	    instcmap(None);
	    redrawclient(c);
	  } else if(event.xcrossing.window==c->window &&
		    event.xcrossing.detail!=NotifyInferior &&
		    event.xcrossing.mode==NotifyNormal)
	    instcmap(None);
	}
	break;
      case ButtonPress:
	if(!rubberclient && !boundingscr && clickwindow==None &&
	   !dragiconlist && event.xbutton.button==Button1 &&
	   !dragscreen && !menuactive)
	  if(c) {
	    if(event.xbutton.window!=c->depth)
	      if(c==doubleclient && (event.xbutton.time-last_double)<
		 dblClickTime) {
		XRaiseWindow(dpy, c->parent);
	      } else {
		doubleclient=c;
		last_double=event.xbutton.time;
	      }
	    if(event.xbutton.window==c->drag) {
	      forcemoving=(prefs.forcemove==FM_ALWAYS) ||
		(event.xbutton.state & ShiftMask);
	      startdragging(c, &event);
	    } else if(event.xbutton.window==c->resize)
	      startresizing(c, &event);
	    else if(event.xbutton.window==c->window ||
		    event.xbutton.window==c->parent)
	      ;
	    else
	      gadgetclicked(c, event.xbutton.window, &event);
	  } else if(i && event.xbutton.window==i->window) {
	    if(i->selected && (event.xbutton.time-last_icon_click)<dblClickTime) {
	      do_icon_double_click(i->scr);
	    } else {
	      if(!(event.xbutton.state & ShiftMask))
		deselect_all_icons(i->scr);
	      last_icon_click=event.xbutton.time;
	      selecticon(i);
	      starticondragging(i->scr, &event);
	    }
	  } else if(scr&&event.xbutton.window==scr->menubardepth) {
	    clickwindow=scr->menubardepth;
	    mbdclick=mbdscr=scr;
	    redrawmenubar(scr->menubardepth);
	  } else if(scr&&event.xbutton.window==scr->menubar &&
		    scr->back!=scr->root) {
	    startscreendragging(scr, &event);
	  }
	  else if(scr&&scr->back==event.xbutton.window) {
	    startbounding(scr, scr->back, &event);
	  } else ;
	else if(event.xbutton.button==3) {
	  if(rubberclient || boundingscr)
	    abortrubber();
	  else if(scr&&(scr==mbdscr)&&clickwindow==scr->menubardepth) {
	    mbdclick=NULL;
	    clickwindow=None;
	    redrawmenubar(scr->menubardepth);
	  } else if(clickclient)
	    gadgetaborted(clickclient);
	  else if(dragiconlist)
	    aborticondragging();
	  else if(dragscreen)
	    abortscreendragging();
	  else if(scr&&!menuactive) {
	    menu_on();
	    menuactive=scr;
	  }
	}
	break;
      case ButtonRelease:
	if(event.xbutton.button==Button1) {
	  if(rubberclient) {
	    if(dragclient) enddragging();
	    else if(resizeclient) endresizing();
	  } else if(boundingwin)
	    endbounding(&event);
	  else if(clickclient)
	    gadgetunclicked(clickclient, &event);
	  else if(dragiconlist) {
	    endicondragging(&event);
	  } else if(dragscreen)
	    endscreendragging();
	  else if((scr=mbdscr)&& clickwindow==scr->menubardepth) {
	    if(mbdclick) {
	      mbdclick=NULL;
	      redrawmenubar(scr->menubardepth);
	      screentoback();
	    }
	    clickwindow=None;
	  }
	} else if(event.xbutton.button==Button3 && (scr=menuactive)) {
	  menu_off();
	  menuactive=NULL;
	}
	break;
      case MotionNotify:
	do {
	  motionx=event.xmotion.x_root;
	  motiony=event.xmotion.y_root;
	} while(XCheckTypedEvent(dpy, MotionNotify, &event));
	if(dragclient) {
	  scr=dragclient->scr;
	  drawrubber();
	  rubberx=motionx-rubberx0;
	  rubbery=motiony-rubbery0;
	  if(!forcemoving) {
	    if(prefs.forcemove==FM_AUTO &&
	       (rubberx+rubberw-scr->width>(rubberw>>2) ||
		rubbery+rubberh-scr->height>(rubberh>>2) ||
		-rubberx>(rubberw>>2)||
		-rubbery>(rubberh>>2)))
	      forcemoving=1;
	    else {
	      if(rubberx+rubberw>scr->width)
		rubberx=scr->width-rubberw;
	      if(rubbery+rubberh>scr->height)
		rubbery=scr->height-rubberh;
	      if(rubberx<0)
		rubberx=0;
	      if(rubbery<0)
		rubbery=0;
	    }
	  }
	  drawrubber();
	} else if(resizeclient) {
	  int rw=rubberw, rh=rubberh;
	  scr=resizeclient->scr;
	  if(resizeclient->sizehints.width_inc) {
	    rw=motionx-rubberx0-resizeclient->sizehints.base_width-
	      resizeclient->framewidth;
	    rw-=rw%resizeclient->sizehints.width_inc;
	    rw+=resizeclient->sizehints.base_width;
	    if(rw>resizeclient->sizehints.max_width)
	      rw=resizeclient->sizehints.max_width;
	    if(rw<resizeclient->sizehints.min_width)
	      rw=resizeclient->sizehints.min_width;
	    rw+=resizeclient->framewidth;
	  }
	  if(resizeclient->sizehints.height_inc) {
	    rh=motiony-rubbery0-resizeclient->sizehints.base_height-
	      resizeclient->frameheight;
	    rh-=rh%resizeclient->sizehints.height_inc;
	    rh+=resizeclient->sizehints.base_height;
	    if(rh>resizeclient->sizehints.max_height)
	      rh=resizeclient->sizehints.max_height;
	    if(rh<resizeclient->sizehints.min_height)
	      rh=resizeclient->sizehints.min_height;
	    rh+=resizeclient->frameheight;
	  }
	  if(rw!=rubberw || rh!=rubberh) {
	    drawrubber();
	    rubberw=rw;
	    rubberh=rh;
	    drawrubber();
	  }
	} else if(dragiconlist) {
	  int i;
	  for(i=0; i<numdragicons; i++)
	    XMoveWindow(dpy, dragiconlist[i].w,
			dragiconlist[i].x+=motionx-rubberx0,
			dragiconlist[i].y+=motiony-rubbery0);
	  rubberx0=motionx;
	  rubbery0=motiony;
	} else if(dragscreen) {
	  rubbery=motiony-rubbery0;
	  if(rubbery<0)
	    rubbery=0;
	  else if(rubbery>=dragscreen->height)
	    rubbery=dragscreen->height-1;
	  XMoveWindow(dpy, dragscreen->back, -dragscreen->bw,
		      (dragscreen->y=rubbery)-dragscreen->bw);
	} else if(boundingscr) {
	  drawrubber();
	  rubberw=motionx-rubberx0;
	  rubberh=motiony-rubbery0;
	  drawrubber();
	}
	break;
      case KeyPress:
	if(!dispatch_event_to_broker(&event, KeyPressMask, modules))
	  internal_broker(&event);
	break;
      case KeyRelease:
	if(!dispatch_event_to_broker(&event, KeyPressMask, modules))
	  internal_broker(&event);
	break;
      case MappingNotify:
	if(!dispatch_event_to_broker(&event, 0, modules))
	  internal_broker(&event);
	break;
      case PropertyNotify:
	if(event.xproperty.atom != None && c &&
	   event.xproperty.window==c->window)
	  propertychange(c, event.xproperty.atom);
	break;
      default:
	fprintf(stderr, "%s: got unexpected event type %d.\n",
		progname, event.type);
      }
    }
    if(signalled) break;
    rfds = master_fd_set;
    t.tv_sec = t.tv_usec = 0;
    if (select(max_fd, &rfds, NULL, NULL, &t) > 0) {
      handle_module_input(&rfds);
      if(FD_ISSET(x_fd, &rfds))
	XPeekEvent(dpy, &event);
      continue;
    }
    if(signalled) break;
    XFlush(dpy);
    rfds = master_fd_set;
    if(eventlist)
      fill_in_call_out(&t);
    if(select(max_fd, &rfds, NULL, NULL, (eventlist? &t:NULL))<0) {
      if (errno != EINTR) {
	perror("select");
	break;
      }
    } else {
      call_call_out();
      handle_module_input(&rfds);
      if(FD_ISSET(x_fd, &rfds))
	XPeekEvent(dpy, &event);
    }
  }
  if(signalled)
    fprintf(stderr, "%s: exiting on signal\n", progname);
  flushmodules();
  flushclients();
  XFlush(dpy);
  XCloseDisplay(dpy);
  exit(1);
}
