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

#include "drawinfo.h"
#include "client.h"
#include "prefs.h"

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

Display *dpy;
int screen_num;
char *progname;
Window root;
int rootdepth, rootwidth, rootheight;
Colormap wm_cmap;
Cursor wm_curs;
Visual *rootvisual;
struct DrawInfo dri;
int fh,bh,h2,h3,h4,h5,h6,h7,h8;
int signalled=0, forcemoving=0;
Client *icondragclient=NULL, *dragclient=NULL, *resizeclient=NULL;
Client *rubberclient=NULL, *clickclient=NULL, *doubleclient=NULL;
Window clickwindow=None;
int rubberx, rubbery, rubberh, rubberw, rubberx0, rubbery0, olddragx, olddragy;
Time last_icon_click, last_double;
Client *last_icon_clicked=NULL;
static int initting=0, ignore_badwindow=0, menuactive=0;
int dblClickTime=1500;

unsigned int meta_mask, switch_mask;

static char **main_argv;

extern void reparent(Client *);
extern void redraw(Client *, Window);
extern void redrawclient(Client *);
extern void redrawmenubar(Window);
extern void gadgetclicked(Client *c, Window w, XEvent *e);
extern void gadgetunclicked(Client *c, XEvent *e);
extern void gadgetaborted(Client *c);
extern void clickenter(void);
extern void clickleave(void);
extern void adjusticon(Client *c);
extern void cleanupicons();
extern void propterychange(Client *c, Atom a);
extern void setclientstate(Client *, int);
extern void menu_on(void);
extern void menu_off(void);
extern void menubar_enter(Window);
extern void menubar_leave(Window);
extern void *getitembyhotkey(char);
extern void menuaction(void *);

#ifndef AMIGAOS
void restart_amiwm()
{
  flushclients();
  XFlush(dpy);
  XCloseDisplay(dpy);
  execvp(main_argv[0], main_argv);
}
#endif

void lookup_keysyms()
{
  int i,j,k,maxsym,mincode,maxcode;
  XModifierKeymap *map=XGetModifierMapping(dpy);
  KeySym *kp, *kmap;
  meta_mask=switch_mask=0;
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
  
  XmuPrintDefaultErrorMessage(d, e, stderr);
  
  if (initting) {
    fprintf(stderr, "%s: failure during initialisation; aborting\n",
	    progname);
    exit(1);
  }
  return 0;
}

void drawrubber()
{
  XGCValues values;
  GC gc=rubberclient->gc;

  values.function=GXinvert;
  values.subwindow_mode=IncludeInferiors;
  XChangeGC(dpy, gc, GCFunction|GCSubwindowMode, &values);
  XDrawRectangle(dpy, root, gc, rubberx, rubbery, rubberw-1, rubberh-1);
  values.function=GXcopy;
  values.subwindow_mode=ClipByChildren;
  XChangeGC(dpy, gc, GCFunction|GCSubwindowMode, &values);
}

void endrubber()
{
  if(rubberclient) {
    drawrubber();
    rubberclient=NULL;
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
  }
}

void startdragging(Client *c, XEvent *e)
{
  dragclient=c;
  XGrabPointer(dpy, c->drag, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
  XGrabServer(dpy);
  initrubber(e->xbutton.x_root, e->xbutton.y_root, c);
  rubberx0-=rubberx;
  rubbery0-=rubbery;
  if(!forcemoving) {
    if(rubberx+rubberw>rootwidth)
      rubberx=rootwidth-rubberw;
    if(rubbery+rubberh>rootheight)
      rubbery=rootheight-rubberh;
    if(rubberx<0)
      rubberx=0;
    if(rubbery<0)
      rubbery=0;
  }
  drawrubber();
}

void starticondragging(Client *c, XEvent *e)
{
  XWindowAttributes xwa;
  icondragclient=c;
  XUnmapWindow(dpy, c->iconlabelwin);
  XRaiseWindow(dpy, c->iconwin);
  XGrabPointer(dpy, c->iconwin, False, Button1MotionMask|ButtonPressMask|
	       ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None,
	       CurrentTime);
/*  initrubber2(e->xbutton.x_root, e->xbutton.y_root, c); */
  redrawicon(c, c->iconwin);
  XGetWindowAttributes(dpy, c->iconwin, &xwa);
  olddragx=rubberx=xwa.x;
  olddragy=rubbery=xwa.y;
  rubberx0=e->xbutton.x_root;
  rubbery0=e->xbutton.y_root;
}

void enddragging()
{
  if(dragclient) {
    Client *c=dragclient;
    endrubber();
    XMoveWindow(dpy, c->parent, c->x=rubberx, c->y=rubbery);
    dragclient=NULL;
    XUngrabServer(dpy);
    XUngrabPointer(dpy, CurrentTime);
    sendconfig(c);
  }
}

void endicondragging()
{
  Client *c;
  if(c=icondragclient) {
    icondragclient=NULL;
    adjusticon(c);
    redrawicon(c, c->iconwin);
    XUngrabPointer(dpy, CurrentTime);
  }
}

void aborticondragging()
{
  if(icondragclient) {
    XMoveWindow(dpy, icondragclient->iconwin, olddragx, olddragy);
    endicondragging();
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

void scanwins()
{
  unsigned int i, nwins;
  Client *c;
  Window dw1, dw2, *wins;
  XWindowAttributes *pattr=NULL;
  
  XQueryTree(dpy, root, &dw1, &dw2, &wins, &nwins);
  if(nwins && (pattr=calloc(nwins, sizeof(XWindowAttributes)))) {
    for (i = 0; i < nwins; i++)
      XGetWindowAttributes(dpy, wins[i], pattr+i);
    for (i = 0; i < nwins; i++) {
      if (pattr[i].override_redirect)
	continue;
      c = createclient(wins[i]);
      if (c != 0 && c->window == wins[i]) {
	if (pattr[i].map_state == IsViewable) {
	  c->state=NormalState;
	  getstate(c);
	  reparent(c);
	  if(c->state==IconicState) {
	    createicon(c);
	    adjusticon(c);
	    XMapWindow(dpy, c->iconwin);
	  } else if(c->state==NormalState)
	    XMapRaised(dpy, c->parent);
	  else
	    XRaiseWindow(dpy, c->parent);
	  c->reparenting=1;
	}
      }
    }
    free(pattr);
  }
  XFree((void *) wins);
  cleanupicons();
}

RETSIGTYPE sighandler(int sig)
{
  signalled=1;
  signal(sig, SIG_IGN);
}

static void instcmap(Colormap c)
{
  XInstallColormap(dpy, (c == None) ? wm_cmap : c);
}

int main(int argc, char *argv[])
{
  XWindowAttributes attr;
  extern void createmenubar();
  extern void createdefaulticons();

  main_argv=argv;
  progname=argv[0];
  if(!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "%s: cannot connect to X server %s\n", progname,
	    XDisplayName(NULL));
    exit(1);
  }
  screen_num = DefaultScreen(dpy);
  root = RootWindow(dpy, screen_num);
  XGetWindowAttributes(dpy, root, &attr);
  rootdepth = attr.depth;
  wm_cmap = attr.colormap;
  rootvisual = attr.visual;
  rootwidth = attr.width;
  rootheight = attr.height;

  read_rc_file();

  init_dri(&dri, DefaultScreen(dpy), True);
  fh = dri.dri_Font->ascent+dri.dri_Font->descent;
  bh=fh+3;
  h2=(2*bh)/10; h3=(3*bh)/10; h4=(4*bh)/10; h5=(5*bh)/10;
  h6=(6*bh)/10; h7=(7*bh)/10; h8=(8*bh)/10;
  XDefineCursor(dpy, root, wm_curs=XCreateFontCursor(dpy, XC_top_left_arrow));
  createmenubar();
  createdefaulticons();

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

  XSelectInput(dpy, root, SubstructureNotifyMask|SubstructureRedirectMask|
	       KeyPressMask|ButtonPressMask|ButtonReleaseMask);

#ifndef AMIGAOS
  if((fcntl(ConnectionNumber(dpy), F_SETFD, 1)) == -1)
    fprintf(stderr, "%s: child cannot disinherit TCP fd\n");
#endif

  lookup_keysyms();

  initting = 0;

  scanwins();

  for(;;) {
    int fd;
    fd_set rfds;
    struct timeval t;
    XEvent event;

    while((!signalled) && QLength(dpy)>0) {
      Client *c;
      int motionx, motiony;
      
      XNextEvent(dpy, &event);
      switch(event.type) {
      case Expose:
	if(!event.xexpose.count) {
	  if(rubberclient) drawrubber();
	  if((c=getclient(event.xexpose.window)))
	    redraw(c, event.xexpose.window);
	  else if((c=getclientbyicon(event.xexpose.window)))
	    redrawicon(c, event.xexpose.window);
	  else
	    redrawmenubar(event.xexpose.window);
	  if(rubberclient) drawrubber();
	}
	break;
      case CreateNotify:
	if(!event.xcreatewindow.override_redirect)
	  createclient(event.xcreatewindow.window);
	break;
      case DestroyNotify:
	if(c=getclient(event.xdestroywindow.window)) {
	  rmclient(c);
	  ignore_badwindow = 1;
	  XSync(dpy, False);
	  ignore_badwindow = 0;
	}
	break;
      case UnmapNotify:
	if((c=getclient(event.xunmap.window)) &&
	   (event.xunmap.window==c->window)) {
	  if(c->active) {
	    c->active=False;
	    if(!menuactive)
	      XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	  }
	  if((!c->reparenting) && c->parent != root) {
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
      case ClientMessage:
	break;
      case MappingNotify:
	if(event.xmapping.request==MappingKeyboard ||
	   event.xmapping.request==MappingModifier)
	  XRefreshKeyboardMapping(&event.xmapping);
	lookup_keysyms();
	break;
      case ColormapNotify:
	if(event.xcolormap.new && (c=getclient(event.xcolormap.window)))
	  if(c->colormap!=event.xcolormap.colormap) {
	    c->colormap=event.xcolormap.colormap;
	    if(c->active)
	      instcmap(c->colormap);
	  }
	break;
      case ConfigureRequest:
	if((c=getclient(event.xconfigurerequest.window))&&
	   event.xconfigurerequest.window==c->window &&
	   c->parent!=root) {
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
	} else
	  XConfigureWindow(dpy, event.xconfigurerequest.window,
			   event.xconfigurerequest.value_mask,
			   (XWindowChanges *)&event.xconfigurerequest.x);
	break;
      case MapRequest:
	if((c=getclient(event.xmaprequest.window)) &&
	   (event.xmaprequest.window == c->window)) {
	  XWMHints *xwmh;
	  if(c->parent==root && (xwmh=XGetWMHints(dpy, c->window))) {
	    if(c->state==WithdrawnState && (xwmh->flags&StateHint)
	       && xwmh->initial_state==IconicState)
	      c->state=IconicState;
	    XFree(xwmh);
	  }
	  switch(c->state) {
	  case WithdrawnState:
	    if(c->parent == root)
	      reparent(c);
	  case NormalState:
	    XMapWindow(dpy, c->window);
	    XMapRaised(dpy, c->parent);
	    setclientstate(c, NormalState);
	    break;
	  case IconicState:
	    if(c->parent == root)
	      reparent(c);
	    if(!(c->iconwin))
	      createicon(c);
	    adjusticon(c);
	    XMapWindow(dpy, c->iconwin);
	    break;
	  }
	}
	break;
      case EnterNotify:
	if(menuactive)
	  menubar_enter(event.xcrossing.window);
	else if(clickwindow && event.xcrossing.window == clickwindow)
	  clickenter();
	else if((c=getclient(event.xcrossing.window))) {
	  if((!c->active) && (c->state==NormalState)) {
	    XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
	    c->active=True;
	    redrawclient(c);
	  }
	  if(event.xcrossing.window==c->window)
	    instcmap(c->colormap);
	}
	break;
      case LeaveNotify:
	if(clickwindow && event.xcrossing.window == clickwindow)
	  clickleave();
	else if((c=getclient(event.xcrossing.window))) {
	  if(c->active && !(event.xcrossing.detail==NotifyInferior ||
			    (event.xcrossing.detail==NotifyNonlinear &&
			     event.xcrossing.window!=c->parent))) {
	    if(!menuactive)
	      XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	    c->active=False;
	    redrawclient(c);
	  }
	  if(event.xcrossing.window==c->window)
	    instcmap(None);
	} else if(menuactive) menubar_leave(event.xcrossing.window);
	break;
      case ButtonPress:
	if(!rubberclient && !clickclient &&
	   !icondragclient && event.xbutton.button==Button1 && !menuactive)
	  if((c=getclient(event.xbutton.window))) {
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
	  } else if((c=getclientbyicon(event.xbutton.window)) &&
		    event.xbutton.window==c->iconwin) {
	    if(c==last_icon_clicked &&
	       (event.xbutton.time-last_icon_click)<dblClickTime) {
	      if(c->iconlabelwin)
		XUnmapWindow(dpy, c->iconlabelwin);
	      if(c->iconwin)
		XUnmapWindow(dpy, c->iconwin);
	      XMapWindow(dpy, c->window);
	      if(c->parent!=root)
		XMapRaised(dpy, c->parent);
	      setclientstate(c, NormalState);
	      last_icon_clicked=NULL;
	    } else {
	      last_icon_click=event.xbutton.time;
	      last_icon_clicked=c;
	      starticondragging(c, &event);
	    }
	  }
	  else ;
	else if(event.xbutton.button==3) {
	  if(rubberclient)
	    abortrubber();
	  else if(clickclient)
	    gadgetaborted(clickclient);
	  else if(icondragclient)
	    aborticondragging();
	  else if(!menuactive) {
	    menu_on();
	    menuactive++;
	  }
	}
	break;
      case ButtonRelease:
	if(event.xbutton.button==Button1) {
	  if(rubberclient) {
	    if(dragclient) enddragging();
	    else if(resizeclient) endresizing();
	  }
	  else if(clickclient)
	    gadgetunclicked(clickclient, &event);
	  else if(icondragclient)
	    endicondragging();
	} else if(event.xbutton.button==Button3 && menuactive) {
	  menu_off();
	  menuactive=0;
	}
	break;
      case MotionNotify:
	do {
	  motionx=event.xmotion.x_root;
	  motiony=event.xmotion.y_root;
	} while(XCheckTypedEvent(dpy, MotionNotify, &event));
	if(dragclient) {
	  drawrubber();
	  rubberx=motionx-rubberx0;
	  rubbery=motiony-rubbery0;
	  if(!forcemoving) {
	    if(prefs.forcemove==FM_AUTO &&
	       (rubberx+rubberw-rootwidth>(rubberw>>2) ||
		rubbery+rubberh-rootheight>(rubberh>>2) ||
		-rubberx>(rubberw>>2)||
		-rubbery>(rubberh>>2)))
	      forcemoving=1;
	    else {
	      if(rubberx+rubberw>rootwidth)
		rubberx=rootwidth-rubberw;
	      if(rubbery+rubberh>rootheight)
		rubbery=rootheight-rubberh;
	      if(rubberx<0)
		rubberx=0;
	      if(rubbery<0)
		rubbery=0;
	    }
	  }
	  drawrubber();
	} else if(resizeclient) {
	  int rw=rubberw, rh=rubberh;
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
	} else if(icondragclient) {
	  rubberx+=motionx-rubberx0;
	  rubbery+=motiony-rubbery0;
	  rubberx0=motionx;
	  rubbery0=motiony;
	  XMoveWindow(dpy, icondragclient->iconwin, rubberx, rubbery);
	}
	break;
      case KeyPress:
	if(event.xkey.state & meta_mask) {
	  KeySym ks=XLookupKeysym(&event.xkey,
				  ((event.xkey.state & ShiftMask)?1:0)+
				  ((event.xkey.state & switch_mask)?2:0));
	  void *item;
	  if(item=getitembyhotkey(ks))
	    menuaction(item);
	}
	break;
      case PropertyNotify:
	if(event.xproperty.atom != None &&
	   (c=getclient(event.xproperty.window)) &&
	   event.xproperty.window==c->window)
	  propertychange(c, event.xproperty.atom);
	break;
      default:
	fprintf(stderr, "%s: got unexpected event type %d.\n",
		progname, event.type);
      }
    }
    if(signalled) break;
    fd = ConnectionNumber(dpy);
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    t.tv_sec = t.tv_usec = 0;
    if (select(fd+1, &rfds, NULL, NULL, &t) == 1) {
      XPeekEvent(dpy, &event);
      continue;
    }
    if(signalled) break;
    XFlush(dpy);
    FD_SET(fd, &rfds);
    if(select(fd+1, &rfds, NULL, NULL, NULL) == 1) {
      XPeekEvent(dpy, &event);
      continue;
    }
    if (errno != EINTR || !signalled)
      perror("select");
    break;
  }
  if(signalled)
    fprintf(stderr, "%s: exiting on signal\n", progname);
  flushclients();
  XFlush(dpy);
  XCloseDisplay(dpy);
  exit(1);
}
