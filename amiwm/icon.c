#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "drawinfo.h"
#include "screen.h"
#include "icon.h"
#include "client.h"
#include "prefs.h"
#include "icc.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

extern Display *dpy;
extern char *progname;
extern XContext icon_context;

XFontStruct *labelfont;

char *label_font_name="-b&h-lucida-medium-r-normal-sans-10-*-*-*-*-*-iso8859-1";

void redrawicon(Icon *i, Window w)
{
  Pixmap pm;

  scr=i->scr;
  if(w==i->window) {
    pm=i->iconpm;
    if(i->selected && i->secondpm)
      pm=i->secondpm;
    if(pm) {
      Window r;
      int x, y;
      unsigned int w, h, bw, d;
      XGetGeometry(dpy, pm, &r, &x, &y, &w, &h, &bw, &d);
      if(d!=scr->depth) {
	XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[SHADOWPEN]);
	XSetBackground(dpy, scr->gc, scr->dri.dri_Pens[BACKGROUNDPEN]);
	XCopyPlane(dpy, pm, i->window, scr->gc, 0, 0,
		i->width-8, i->height-8, 4, 4, 1);
      }
      else
	XCopyArea(dpy, pm, i->window, scr->gc, 0, 0,
		  i->width-8, i->height-8, 4, 4);
    }
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[i->selected? SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, scr->gc, 0, 0, i->width-1, 0);
    XDrawLine(dpy, w, scr->gc, 0, 0, 0, i->height-1);
    XSetForeground(dpy, scr->gc, scr->dri.dri_Pens[i->selected? SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, scr->gc, 1, i->height-1, i->width-1, i->height-1);
    XDrawLine(dpy, w, scr->gc, i->width-1, 1, i->width-1, i->height-1);
  } else if(w==i->labelwin) {
    XDrawImageString(dpy, w, scr->icongc, 0, labelfont->ascent,
		     (i->label.value? (char *)i->label.value:"()"),
		     i->label.nitems);
  }
}

void selecticon(Icon *i)
{
  if(!(i->selected)) {
    i->nextselected = i->scr->firstselected;
    i->scr->firstselected = i;
    i->selected = 1;
    redrawicon(i, i->window);
  }
}

void deselecticon(Icon *i)
{
  Icon *i2;
  if(i==i->scr->firstselected)
    i->scr->firstselected = i->nextselected;
  else for(i2=i->scr->firstselected; i2; i2=i2->nextselected)
    if(i2->nextselected==i) {
      i2->nextselected = i->nextselected;
      break;
    }
  i->nextselected = NULL;
  i->selected=0;
  redrawicon(i, i->window);
}

void deselect_all_icons(Scrn *scr)
{
  while(scr->firstselected)
    deselecticon(scr->firstselected);
}

void select_all_icons(Scrn *scr)
{
  Icon *i;
  XWindowAttributes attr;
  for(i=scr->icons; i; i=i->next)
    if(i->window && i->mapped)
      selecticon(i);
}

void reparenticon(Icon *i, Scrn *s, int x, int y)
{
  Icon **ip;
  int os=i->selected;
  if(s==i->scr) {
    if(x!=i->x || y!=i->y)
      XMoveWindow(dpy, i->window, i->x=x, i->y=y);
    return;
  }
  if(os)
    deselecticon(i);
  for(ip=&i->scr->icons; *ip; ip=&(*ip)->next)
    if(*ip==i) {
      *ip=i->next;
      break;
    }
  XReparentWindow(dpy, i->window, s->back, i->x=x, i->y=y);
  if(i->labelwin) {
    XReparentWindow(dpy, i->labelwin, s->back,
		    x+(i->width>>1)-(i->labelwidth>>1),
		    y+i->height+1);
  }
  i->scr=s;
  i->next=s->icons;
  s->icons=i;
  if(i->client) {
    i->client->scr=s;
    if(i->client->parent != i->client->scr->root)
      XReparentWindow(dpy, i->client->parent, s->back,
		      i->client->x, i->client->y);
    setstringprop(i->client->window, amiwm_screen, s->deftitle);
    sendconfig(i->client);
  }
  if(os)
    selecticon(i);
}

void createdefaulticons()
{
  Window r;
  int x,y;
  unsigned int b,d;
  extern void load_do(const char *, Pixmap *, Pixmap *);

  init_iconpalette();
  load_do(prefs.defaulticon, &scr->default_tool_pm, &scr->default_tool_pm2);
  if(scr->default_tool_pm == None) {
    fprintf(stderr, "%s: Cannot load default icon \"%s\".\n",
	    progname, prefs.defaulticon);
    exit(1);
  }
  XGetGeometry(dpy, scr->default_tool_pm, &r, &x, &y,
	       &scr->default_tool_pm_w, &scr->default_tool_pm_h, &b, &d);
}

void adjusticon(Icon *i)
{
  Window r,p,*children,w=i->window,lw=i->labelwin;
  unsigned int nchildren;
  int nx, ny;
  Window ws[3];
  Icon *i2;

  scr=i->scr;
  nx=i->x; ny=i->y;
  for(i2=scr->icons; i2; i2=i2->next)
    if(i2!=i && i2->mapped && nx<i2->x+i2->width && nx+i->width>i2->x &&
       ny<i2->y+i2->height+scr->lh+2 && ny+i->height+scr->lh+2>i2->y) {
      ny=i2->y+i2->height+scr->lh+4;
      if(ny+i->height+scr->lh+1>scr->height) {
	nx=i2->x+i2->width+5;
	if(nx+i->width>scr->width) {
	  ny=scr->height;
	  break;
	} else
	  ny=scr->bh+4;
      }
      i2=scr->icons;
      continue;
    }

  if(nx+i->width>scr->width)
    nx=scr->width-i->width;
  if(ny+i->height>scr->height)
    ny=scr->height-i->height;
  if(nx<0) nx=0;
  if(ny<scr->bh) ny=scr->bh;
  if(nx!=i->x || ny!=i->y)
    XMoveWindow(dpy, w, i->x=nx, i->y=ny);
  ws[0]=scr->menubar;
  ws[1]=w;
  ws[2]=lw;
  XRestackWindows(dpy, ws, 3);
  XMoveWindow(dpy, lw, nx+(i->width>>1)-(i->labelwidth>>1),
	      ny+i->height+1);
}

void destroyiconicon(Icon *i)
{
  if(i->innerwin) {
    XUnmapWindow(dpy, i->innerwin);
    XReparentWindow(dpy, i->innerwin, i->scr->root, i->x+4, i->y+4);
    XRemoveFromSaveSet(dpy, i->innerwin);
    i->innerwin=None;
  }
  i->iconpm = i->secondpm = None;
}

void createiconicon(Icon *i, XWMHints *wmhints)
{
  XSetWindowAttributes attr;
  int x=20, y=20;
  unsigned int w=40, h=40;
  Window win=None;
  void newicontitle(Client *);

  scr=i->scr;
  if(wmhints) {
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
      i->iconpm=wmhints->icon_pixmap;
      XGetGeometry(dpy, i->iconpm, &r, &x, &y, &w, &h, &b, &d);
      w+=8;
      h+=8;
    } else {
      i->iconpm=scr->default_tool_pm;
      i->secondpm=scr->default_tool_pm2;
      w=scr->default_tool_pm_w+8;
      h=scr->default_tool_pm_h+8;
    }
    if(wmhints->flags&IconPositionHint) {
      x=wmhints->icon_x;
      y=wmhints->icon_y;
    } else if(i->window) {
      Window r;
      int w, h, bw, d;
      XGetGeometry(dpy, i->window, &r, &x, &y, &w, &h, &bw, &d);
    }
  } else {
    i->iconpm=scr->default_tool_pm;
    w=scr->default_tool_pm_w+8;
    h=scr->default_tool_pm_h+8;
  }
  if(!(i->window)) {  
    attr.override_redirect=True;
    attr.background_pixel=scr->dri.dri_Pens[BACKGROUNDPEN];
    i->window=XCreateWindow(dpy, scr->back, i->x=x, i->y=y,
			    i->width=w, i->height=h, 0, CopyFromParent,
			    InputOutput, CopyFromParent,
			    CWOverrideRedirect|CWBackPixel, &attr);
    i->mapped=0;
    XSaveContext(dpy, i->window, icon_context, (XPointer)i);
    XLowerWindow(dpy, i->window);
    XSelectInput(dpy, i->window,ExposureMask|ButtonPressMask|ButtonReleaseMask);
  } else {
    XMoveResizeWindow(dpy, i->window, i->x=x, i->y=y, i->width=w, i->height=h);
    redrawicon(i, i->window);
  }
  if(!(i->labelwin)) {
    i->labelwin=XCreateWindow(dpy, scr->back, 0, 0, 1, 1, 0,
			      CopyFromParent, InputOutput, CopyFromParent,
			      CWOverrideRedirect|CWBackPixel, &attr);
    XSaveContext(dpy, i->labelwin, icon_context, (XPointer)i);
    XLowerWindow(dpy, i->labelwin);
    XSelectInput(dpy, i->labelwin, ExposureMask);
  }
  if(i->client)
    newicontitle(i->client);
  if(win=i->innerwin) {
    XAddToSaveSet(dpy, win);
    XReparentWindow(dpy, win, i->window, 4, 4);
    XSelectInput(dpy, win, SubstructureRedirectMask);
    XMapWindow(dpy, win);
  }
  adjusticon(i);
}

void createicon(Client *c)
{
  XWMHints *wmhints;
  Icon *i;

  c->icon=i=(Icon *)calloc(1, sizeof(Icon));
  i->scr=scr=c->scr;
  i->client=c;
  i->next=scr->icons;
  scr->icons=i;

  wmhints=XGetWMHints(dpy, c->window);
  createiconicon(i, wmhints);
  if(wmhints) XFree(wmhints);
}

void rmicon(Icon *i)
{
  Icon *ii;
  Window r,p,*ch;
  unsigned int nc;
  
  if (i->selected)
    deselecticon(i);

  if (i == i->scr->icons)
    i->scr->icons = i->next;
  else
    if(ii = i->scr->icons)
      for (; ii->next; ii = ii->next)
	if (ii->next == i) {
	  ii->next = i->next;
	  break;
	}

  destroyiconicon(i);
  XDestroyWindow(dpy, i->window);
  XDeleteContext(dpy, i->window, icon_context);
  if(i->labelwin) {
    XDestroyWindow(dpy, i->labelwin);
    XDeleteContext(dpy, i->labelwin, icon_context);
  }
  if(i->label.value)
    XFree(i->label.value);
  free(i);
}

static int cmp_iconpos(const Icon **p1, const Icon **p2)
{
  int r;
  return((r=(*p2)->mapped-(*p1)->mapped)?r:((r=(*p1)->x-(*p2)->x)?r:
					    (*p1)->y-(*p2)->y));
}

static void placeicons(Icon **p, int n, int x, int w)
{
  int i;

  x+=w>>1;
  for(i=0; i<n; i++) {
    Icon *i=*p++;
    XMoveWindow(dpy, i->window, i->x=x-(i->width>>1), i->y);
    XMoveWindow(dpy, i->labelwin, x-(i->labelwidth>>1),
		i->y+i->height+1);
  }
}

void cleanupicons()
{
  Icon *i, **icons;
  int nicons=0, maxicons=16;

  if(icons=calloc(maxicons, sizeof(Icon*))) {
    for(i=scr->icons; i; i=i->next)
      if(i->window) {
	if(nicons>=maxicons)
	  if(!(icons=realloc(icons, sizeof(Icon*)*(maxicons<<=1))))
	    return;
	icons[nicons]=i;
	i->x+=i->width>>1;
	nicons++;
      }
    if(nicons) {
      int i0=0, i, x=5, y=scr->bh+4, w, mw=0;
      qsort(icons, nicons, sizeof(*icons),
	    (int (*)(const void *, const void *))cmp_iconpos);
      for(i=0; i<nicons; i++) {
	if(i>i0 && y+icons[i]->height>scr->height-4-scr->lh) {
	  placeicons(icons+i0, i-i0, x, mw);
	  x+=mw+5;
	  y=scr->bh+4;
	  mw=0;
	  i0=i;
	}
	icons[i]->y=y;
	w=icons[i]->width;
	if(icons[i]->mapped && icons[i]->labelwidth>w)
	  w=icons[i]->labelwidth;
	if(w>mw)
	  mw=w;
	y+=icons[i]->height+4+scr->lh;
      }
      placeicons(icons+i0, nicons-i0, x, mw);
    }
    free(icons);
  }
}

void newicontitle(Client *c)
{
  Icon *i=c->icon;
  if(i->label.value)
    XFree(i->label.value);
  if(!(XGetWMIconName(dpy, c->window, &i->label))) {
    i->label.value=NULL;
    i->labelwidth=XTextWidth(labelfont, "()", i->label.nitems=2);
  } else
    i->labelwidth=XTextWidth(labelfont, i->label.value,
			     i->label.nitems);
  XResizeWindow(dpy, i->labelwin, i->labelwidth, c->scr->lh);
  XMoveWindow(dpy, i->labelwin,
	      i->x+(i->width>>1)-(i->labelwidth>>1),
	      i->y+i->height+1);
  redrawicon(i, i->labelwin);
}
