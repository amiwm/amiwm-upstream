#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "client.h"
#include "drawinfo.h"
#include "prefs.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

extern Display *dpy;
extern Window root, menubar;
extern struct DrawInfo dri;
extern int rootwidth, rootheight, rootdepth, bh;
extern Client *icondragclient;
extern char *progname;

Pixmap default_tool_pm=None;
unsigned int default_tool_pm_w=0, default_tool_pm_h=0;
GC labelgc=None;
int lh=0;
XFontStruct *labelfont;

char *label_font_name="-b&h-lucida-medium-r-normal-sans-10-*-*-*-*-*-iso8859-1";

void createdefaulticons()
{
  Window r;
  int x,y;
  unsigned int b,d;
  extern Pixmap load_do(const char *);

  init_iconpalette();
  default_tool_pm=load_do(prefs.defaulticon);
  if(default_tool_pm == None) {
    fprintf(stderr, "%s: Cannot load default icon \"%s\".\n",
	    progname, prefs.defaulticon);
    exit(1);
  }
  XGetGeometry(dpy, default_tool_pm, &r, &x, &y,
	       &default_tool_pm_w, &default_tool_pm_h, &b, &d);
}

void adjusticon(Client *c)
{
  XWindowAttributes attr;
  Window r,p,*children,w=c->iconwin,lw=c->iconlabelwin;
  unsigned int nchildren;
  int nx, ny;
  Window ws[3];

  XGetWindowAttributes(dpy, w, &attr);
  if((nx=attr.x)+attr.width>rootwidth)
    nx=rootwidth-attr.width;
  if((ny=attr.y)+attr.height>rootheight)
    ny=rootheight-attr.height;
  if(nx<0) nx=0;
  if(ny<bh) ny=bh;
  if(nx!=attr.x || ny!=attr.y)
    XMoveWindow(dpy, w, nx, ny);
  ws[0]=menubar;
  ws[1]=w;
  ws[2]=lw;
  XRestackWindows(dpy, ws, 3);
  XMoveWindow(dpy, lw, nx+(attr.width>>1)-(c->iconlabelwidth>>1),
	      ny+attr.height+1);
  XMapWindow(dpy, lw);
}

void createicon(Client *c)
{
  XSetWindowAttributes attr;
  XWMHints *wmhints;
  int x=20, y=20;
  unsigned int w=40, h=40;
  Window win=None;
  void newicontitle(Client *);

  if((wmhints=XGetWMHints(dpy, c->window))) {
    if((wmhints->flags&IconWindowHint) && wmhints->icon_window) {
      Window r;
      unsigned int b, d;
      win=wmhints->icon_window;
      XGetGeometry(dpy, win, &r, &x, &y, &w, &h, &b, &d);
      x-=4; y-=4; w+=8; h+=8;
    } else if((wmhints->flags&IconPixmapHint) && wmhints->icon_pixmap) {
      Window r;
      int x, y;
      unsigned int b, d;
      c->iconpm=wmhints->icon_pixmap;
      XGetGeometry(dpy, c->iconpm, &r, &x, &y, &w, &h, &b, &d);
      w+=8;
      h+=8;
    } else {
      c->iconpm=default_tool_pm;
      w=default_tool_pm_w+8;
      h=default_tool_pm_h+8;
    }
    if(wmhints->flags&IconPositionHint) {
      x=wmhints->icon_x;
      y=wmhints->icon_y;
    }
  } else {
    c->iconpm=default_tool_pm;
    w=default_tool_pm_w+8;
    h=default_tool_pm_h+8;
  }
  attr.override_redirect=True;
  attr.background_pixel=dri.dri_Pens[BACKGROUNDPEN];
  c->iconwin=XCreateWindow(dpy, root, x, y, w, h, 0, CopyFromParent,
			   InputOutput, CopyFromParent,
			   CWOverrideRedirect|CWBackPixel, &attr);
  XLowerWindow(dpy, c->iconwin);
  c->iconlabelwin=XCreateWindow(dpy, root, 0, 0, 1, 1, 0,
				CopyFromParent, InputOutput, CopyFromParent,
				CWOverrideRedirect|CWBackPixel, &attr);
  XLowerWindow(dpy, c->iconlabelwin);
  if(!labelgc) {
    labelgc=XCreateGC(dpy, c->iconlabelwin, 0, NULL);
    if(!(labelfont = XLoadQueryFont(dpy, label_font_name))) {
      fprintf(stderr, "%s: cannot open font %s\n", progname, label_font_name);
      labelfont = dri.dri_Font;
    }
    XSetFont(dpy, labelgc, labelfont->fid);
    lh = labelfont->ascent+labelfont->descent;
    XSetBackground(dpy, labelgc, dri.dri_Pens[BACKGROUNDPEN]);
    XSetForeground(dpy, labelgc, dri.dri_Pens[TEXTPEN]);
  }
  newicontitle(c);
  if(win) {
    XAddToSaveSet(dpy, win);
    XReparentWindow(dpy, win, c->iconwin, 4, 4);
    XSelectInput(dpy, win, SubstructureRedirectMask);
    XMapWindow(dpy, win);
  }
  XSelectInput(dpy, c->iconwin,ExposureMask|ButtonPressMask|ButtonReleaseMask);
  XSelectInput(dpy, c->iconlabelwin, ExposureMask);
  adjusticon(c);
}

void redrawicon(Client *c, Window w)
{
  XWindowAttributes attr;

  if(w==c->iconwin) {
    XGetWindowAttributes(dpy, w, &attr);
    if(c->iconpm) {
      Window r;
      int x, y;
      unsigned int w, h, bw, d;
      XGetGeometry(dpy, c->iconpm, &r, &x, &y, &w, &h, &bw, &d);
      if(d!=rootdepth) {
	XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
	XSetBackground(dpy, c->gc, dri.dri_Pens[BACKGROUNDPEN]);
	XCopyPlane(dpy, c->iconpm, c->iconwin, c->gc, 0, 0,
		attr.width-8, attr.height-8, 4, 4, 1);
      }
      else
	XCopyArea(dpy, c->iconpm, c->iconwin, c->gc, 0, 0,
		  attr.width-8, attr.height-8, 4, 4);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[c==icondragclient? SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, attr.width-1, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, attr.height-1);
    XSetForeground(dpy, c->gc, dri.dri_Pens[c==icondragclient? SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 1, attr.height-1, attr.width-1, attr.height-1);
    XDrawLine(dpy, w, c->gc, attr.width-1, 1, attr.width-1, attr.height-1);
  } else if(w==c->iconlabelwin) {
    XDrawImageString(dpy, w, labelgc, 0, labelfont->ascent,
		     (c->iconlabel.value? (char *)c->iconlabel.value:"()"),
		     c->iconlabel.nitems);
  }
}

struct iconpos { Client *c; int mapped, x, y, w, h; };

static int cmp_iconpos(const struct iconpos *p1, const struct iconpos *p2)
{
  int r;
  return((r=p2->mapped-p1->mapped)?r:((r=p1->x-p2->x)?r:p1->y-p2->y));
}

static void placeicons(struct iconpos *p, int n, int x, int w)
{
  int i;
  XWindowAttributes attr;

  x+=w>>1;
  for(i=0; i<n; i++) {
    XGetWindowAttributes(dpy, p[i].c->iconwin, &attr);
    XMoveWindow(dpy, p[i].c->iconwin, x-(attr.width>>1), p[i].y);
    XMoveWindow(dpy, p[i].c->iconlabelwin, x-(p[i].c->iconlabelwidth>>1),
		p[i].y+p[i].h+1);
  }
}

void cleanupicons()
{
  extern Client *clients;
  Client *c;
  struct iconpos *icons;
  int nicons=0, maxicons=16;
  XWindowAttributes attr;

  if(icons=calloc(maxicons, sizeof(struct iconpos))) {
    for(c=clients; c; c=c->next)
      if(c->iconwin) {
	if(nicons>=maxicons)
	  if(!(icons=realloc(icons, sizeof(struct iconpos)*(maxicons<<=1))))
	    return;
	XGetWindowAttributes(dpy, c->iconwin, &attr);
	icons[nicons].c=c;
	icons[nicons].mapped=attr.map_state!=IsUnmapped;
	icons[nicons].x=attr.x+(attr.width>>1);
	icons[nicons].y=attr.y;
	icons[nicons].w=((attr.map_state==IsUnmapped ||
			  attr.width>=c->iconlabelwidth)?
			 attr.width:c->iconlabelwidth);
	icons[nicons].h=attr.height;
	nicons++;
      }
    if(nicons) {
      int i0=0, i, x=5, y=bh+4, mw=0;
      qsort(icons, nicons, sizeof(*icons),
	    (int (*)(const void *, const void *))cmp_iconpos);
      for(i=0; i<nicons; i++) {
	if(i>i0 && y+icons[i].h>rootheight-4-lh) {
	  placeicons(icons+i0, i-i0, x, mw);
	  x+=mw+5;
	  y=bh+4;
	  mw=0;
	  i0=0;
	}
	icons[i].y=y;
	if(icons[i].w>mw)
	  mw=icons[i].w;
	y+=icons[i].h+4+lh;
      }
      placeicons(icons+i0, nicons-i0, x, mw);
    }
    free(icons);
  }
}

void newicontitle(Client *c)
{
  XWindowAttributes attr;
  XGetWindowAttributes(dpy, c->iconwin, &attr);
  if(c->iconlabel.value)
    XFree(c->iconlabel.value);
  if(!(XGetWMIconName(dpy, c->window, &c->iconlabel))) {
    c->iconlabel.value=NULL;
    c->iconlabelwidth=XTextWidth(labelfont, "()", c->iconlabel.nitems=2);
  } else
    c->iconlabelwidth=XTextWidth(labelfont, c->iconlabel.value,
				 c->iconlabel.nitems);
  XResizeWindow(dpy, c->iconlabelwin, c->iconlabelwidth, lh);
  XMoveWindow(dpy, c->iconlabelwin,
	      attr.x+(attr.width>>1)-(c->iconlabelwidth>>1),
	      attr.y+attr.height+1);
  redrawicon(c, c->iconlabelwin);
}
