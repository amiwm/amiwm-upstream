#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "drawinfo.h"
#include "screen.h"
#include "icon.h"
#include "client.h"
#include "prefs.h"

extern Display *dpy;
extern Cursor wm_curs;
extern XFontStruct *labelfont;
extern char *progname;
extern XContext screen_context;

extern void createmenubar();

Scrn *front = NULL, *scr = NULL;

void screentoback(void)
{
  if((!scr)||(scr->back==scr->root)) return;
  if(scr==front) {
    XLowerWindow(dpy, scr->back);
    front=scr->behind;
  } else if(scr->behind==front) {
    XRaiseWindow(dpy, scr->back);
    front=scr;
  } else {
    XRaiseWindow(dpy, scr->back);
    scr->upfront->behind=scr->behind;
    scr->behind->upfront=scr->upfront;
    scr->upfront=front->upfront;
    scr->behind=front;
    front->upfront->behind=scr;
    front->upfront=scr;
    front=scr;
  }
  init_dri(&front->dri, front->root, front->cmap, True);
}

void assimilate(Window w, int x, int y)
{
#ifdef ASSIMILATE_WINDOWS
  XSetWindowAttributes xsa;

  XAddToSaveSet(dpy, w);
  XReparentWindow(dpy, w, scr->back, x, (y<scr->y? 0:y-scr->y));
  XSaveContext(dpy, w, screen_context, (XPointer)scr);
  xsa.override_redirect = False;
  XChangeWindowAttributes(dpy, w, CWOverrideRedirect, &xsa);
#endif
}

static void scanwins()
{
  unsigned int i, nwins;
  Client *c;
  Window dw1, dw2, *wins;
  XWindowAttributes *pattr=NULL;
  XPointer dummy;
  Scrn *s=scr;

  XQueryTree(dpy, scr->root, &dw1, &dw2, &wins, &nwins);
  if(nwins && (pattr=calloc(nwins, sizeof(XWindowAttributes)))) {
    for (i = 0; i < nwins; i++)
      XGetWindowAttributes(dpy, wins[i], pattr+i);
    for (i = 0; i < nwins; i++) {
      if (pattr[i].override_redirect) {
	if(scr->back!=scr->root && XFindContext(dpy, wins[i], screen_context, &dummy))
	  assimilate(wins[i], pattr[i].x, pattr[i].y);
	continue;
      }
      c = createclient(wins[i]);
      if (c != 0 && c->window == wins[i]) {
	if (pattr[i].map_state == IsViewable) {
	  c->state=NormalState;
	  getstate(c);
	  reparent(c);
	  if(c->state==IconicState) {
	    createicon(c);
	    adjusticon(c->icon);
	    XMapWindow(dpy, c->icon->window);
	    XMapWindow(dpy, c->icon->labelwin);
	  } else if(c->state==NormalState)
	    XMapRaised(dpy, c->parent);
	  else
	    XRaiseWindow(dpy, c->parent);
	  c->reparenting=1;
	  scr=s;
	}
      }
    }
    free(pattr);
  }
  XFree((void *) wins);
  cleanupicons();
}

Scrn *openscreen(char *deftitle, Window root)
{
  Scrn *s;
  XWindowAttributes attr;
  XSetWindowAttributes swa;
  XGCValues gcv;
  extern char *label_font_name;

  if(!root)
    root = DefaultRootWindow(dpy);

  XGetWindowAttributes(dpy, root, &attr);

  s = (Scrn *)calloc(1, sizeof(Scrn));
  s->root = root;
  s->cmap = attr.colormap;
  s->depth = attr.depth;
  s->visual = attr.visual;

  init_dri(&s->dri, s->root, s->cmap, True);

  swa.background_pixel = s->dri.dri_Pens[BACKGROUNDPEN];
  swa.override_redirect = True;
  swa.colormap = attr.colormap;
  swa.cursor = wm_curs;
  swa.border_pixel = BlackPixel(dpy, DefaultScreen(dpy));

  s->back = XCreateWindow(dpy, root,
			  -prefs.borderwidth, (s->y=0)-prefs.borderwidth,
			  s->width = attr.width, s->height = attr.height,
			  s->bw=prefs.borderwidth, CopyFromParent,
			  InputOutput, CopyFromParent,
			  CWBackPixel|CWOverrideRedirect|CWColormap|CWCursor|
			  CWBorderPixel,
			  &swa);

  XSaveContext(dpy, s->back, screen_context, (XPointer)s);

  gcv.background = s->dri.dri_Pens[BACKGROUNDPEN];
  gcv.font = s->dri.dri_Font->fid;

  s->gc = XCreateGC(dpy, s->back, GCBackground|GCFont, &gcv);
  
  if(!labelfont)
    if(!(labelfont = XLoadQueryFont(dpy, label_font_name))) {
      fprintf(stderr, "%s: cannot open font %s\n", progname, label_font_name);
      labelfont = s->dri.dri_Font;
    }

  gcv.font = labelfont->fid;
  gcv.foreground = s->dri.dri_Pens[TEXTPEN];

  s->icongc = XCreateGC(dpy, s->back, GCForeground|GCBackground|GCFont, &gcv);

  gcv.function = GXinvert;
  gcv.subwindow_mode = IncludeInferiors;

  s->rubbergc = XCreateGC(dpy, s->back, GCFunction|GCSubwindowMode, &gcv);

  s->title = s->deftitle = deftitle;

  s->default_tool_pm = None;
  s->default_tool_pm2 = None;
  s->default_tool_pm_w=0;
  s->default_tool_pm_h=0;
  s->lh = labelfont->ascent+labelfont->descent;

  if(front) {
    s->behind=front;
    s->upfront=front->upfront;
    front->upfront->behind=s;
    front->upfront=s;
  } else {
    s->behind = s->upfront = s;
    front = s;
  }

  scr=s;

  return s;
}

void realizescreens(void)
{
  Scrn *s2;

  scr = front;

  do {
    scr->fh = scr->dri.dri_Font->ascent+scr->dri.dri_Font->descent;
    scr->bh=scr->fh+3;
    scr->h2=(2*scr->bh)/10; scr->h3=(3*scr->bh)/10;
    scr->h4=(4*scr->bh)/10; scr->h5=(5*scr->bh)/10;
    scr->h6=(6*scr->bh)/10; scr->h7=(7*scr->bh)/10;
    scr->h8=(8*scr->bh)/10;
    createmenubar();
    createdefaulticons();

    XSelectInput(dpy, scr->root,
		 SubstructureNotifyMask|SubstructureRedirectMask|
		 KeyPressMask|KeyReleaseMask|
		 ButtonPressMask|ButtonReleaseMask);
    if(scr->back != scr->root)
      XSelectInput(dpy, scr->back,
		   SubstructureNotifyMask|SubstructureRedirectMask|
		   KeyPressMask|KeyReleaseMask|
		   ButtonPressMask|ButtonReleaseMask);

    XStoreName(dpy, scr->back, scr->title);
    XLowerWindow(dpy, scr->back);
    XMapWindow(dpy, scr->back);

    scr=scr->behind;
  } while(scr!=front);
  scanwins();
  init_dri(&scr->dri, scr->root, scr->cmap, True);
}

Scrn *getscreen(Window w)
{
  Scrn *s=front;
  if(w && s)
    do {
      if(s->back == w || s->root == w)
	return s;
      s=s->behind;
    } while(s!=front);
  return front;
}

Scrn *getscreenbyroot(Window w)
{
  Scrn *s=front;
  if(s)
    do {
      if(s->root == w)
	return s;
      s=s->behind;
    } while(s!=front);
  return NULL;
}

/*
Scrn *getscreenbymenubar(Window w)
{
  Scrn *s=front;
  if(s)
    do {
      if(s->menubardepth == w)
	return s;
      s=s->behind;
    } while(s!=front);
  return NULL;
}
*/
