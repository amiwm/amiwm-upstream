#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>

#include "drawinfo.h"
#include "client.h"
#include "icc.h"
#include "prefs.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#define mx(a,b) ((a)>(b)?(a):(b))

extern Display *dpy;
extern Window root;
extern struct DrawInfo dri;
extern int fh,bh,h2,h3,h4,h5,h6,h7,h8;

Window creategadget(Window p, int x, int y, int w, int h)
{
  XSetWindowAttributes attr;
  Window r;

  attr.override_redirect=True;
  attr.background_pixel=dri.dri_Pens[BACKGROUNDPEN];
  r=XCreateWindow(dpy, p, x, y, w, h, 0, CopyFromParent, InputOutput,
		  CopyFromParent, CWOverrideRedirect|CWBackPixel, &attr);
  XSelectInput(dpy, r, ExposureMask|ButtonPressMask|ButtonReleaseMask|
	       EnterWindowMask|LeaveWindowMask);
  return r;
}

void spread_top_gadgets(Client *c)
{
  if(c->zoom) {
    XResizeWindow(dpy, c->drag, c->dragw=mx(4, c->pwidth-23-23-23-19), bh);
    XMoveWindow(dpy, c->icon, c->pwidth-23-23-23, 0);
    XMoveWindow(dpy, c->zoom, c->pwidth-23-23, 0);
  } else {
    XResizeWindow(dpy, c->drag, c->dragw=mx(4, c->pwidth-23-23-19), bh);
    XMoveWindow(dpy, c->icon, c->pwidth-23-23, 0);
  }
  XMoveWindow(dpy, c->depth, c->pwidth-23, 0);
}

void setclientborder(Client *c, int old, int new)
{
  int wc=0;
  int x=c->x, y=c->y, oldpw=c->pwidth, oldph=c->pheight;
  old&=(Psizeright|Psizebottom|Psizetrans);
  new&=(Psizeright|Psizebottom|Psizetrans);
  if(new!=old) {
    if((new & Psizeright)&&!(old & Psizeright))
      { c->pwidth+=14; wc++; }
    else if((old & Psizeright)&&!(new & Psizeright))
      { c->pwidth-=14; wc++; }
    if((new & Psizebottom)&&!(old & Psizebottom))
      c->pheight+=8;
    else if((old & Psizebottom)&&!(new & Psizebottom))
      c->pheight-=8;
    XResizeWindow(dpy, c->parent, c->pwidth, c->pheight);
    if(c->resize)
      XMoveWindow(dpy, c->resize, c->pwidth-18, c->pheight-10);
    if(wc) 
      spread_top_gadgets(c);
  }
  c->proto = (c->proto&~(Psizeright|Psizebottom|Psizetrans)) | new;
  c->framewidth=((new&Psizeright)?22:8);
  c->frameheight=bh+((new&Psizebottom)?10:2);
  if(c->gravity==EastGravity || c->gravity==NorthEastGravity ||
     c->gravity==SouthEastGravity)
    x-=(c->pwidth-oldpw);
  if(c->gravity==SouthGravity || c->gravity==SouthEastGravity ||
     c->gravity==SouthWestGravity)
    y-=(c->pheight-oldph);
  if(x!=c->x || y!=c->y)
    XMoveWindow(dpy, c->parent, c->x=x, c->y=y);
}

void resizeclientwindow(Client *c, int width, int height)
{
  if(width!=c->pwidth || height!=c->pheight) {
    int old_width=c->pwidth;
    XResizeWindow(dpy, c->parent, c->pwidth=width, c->pheight=height);
    if(width!=old_width)
      spread_top_gadgets(c);
    if(c->resize)
      XMoveWindow(dpy, c->resize, c->pwidth-18, c->pheight-10);
    XResizeWindow(dpy, c->window, c->pwidth-c->framewidth, c->pheight-c->frameheight);
    sendconfig(c);
  }
}

static int resizable(XSizeHints *xsh)
{
  int b, r=0;
  if(xsh->width_inc) {
    if(xsh->min_width+xsh->width_inc<=xsh->max_width)
      r++;
  }
  if(xsh->height_inc) {
    if(xsh->min_height+xsh->height_inc<=xsh->max_height)
      r+=2;
  }
  return r;
}

void reparent(Client *c)
{
  XWindowAttributes attr;
  XSetWindowAttributes attr2;
  int cb;

  if(c->parent && c->parent != root)
    return;
  getproto(c);
  XAddToSaveSet(dpy, c->window);
  XGetWindowAttributes(dpy, c->window, &attr);
  c->colormap = attr.colormap;
  c->old_bw = attr.border_width;
  c->framewidth=8;
  c->frameheight=bh+2;
  attr2.override_redirect=True;
  grav_map_win_to_frame(c, attr.x, attr.y, &c->x, &c->y);
  c->parent=XCreateWindow(dpy, root, c->x, c->y,
			  c->pwidth=attr.width+8, c->pheight=attr.height+2+bh,
			  0, CopyFromParent, InputOutput, CopyFromParent,
			  CWOverrideRedirect, &attr2);
  XSetWindowBackground(dpy, c->parent, dri.dri_Pens[BACKGROUNDPEN]);
  XSetWindowBorderWidth(dpy, c->window, 0);
  XSelectInput(dpy, c->parent, SubstructureRedirectMask |
	       SubstructureNotifyMask | ExposureMask |
	       EnterWindowMask | LeaveWindowMask | ButtonPressMask |
	       ButtonReleaseMask);
  XReparentWindow(dpy, c->window, c->parent, 4, bh);
  XSelectInput(dpy, c->window, EnterWindowMask | LeaveWindowMask |
	       ColormapChangeMask | PropertyChangeMask);
  cb=(resizable(&c->sizehints)? prefs.sizeborder:0);
  c->close=creategadget(c->parent, 0, 0, 19, bh);
  c->drag=creategadget(c->parent, 19, 0, 1, 1);
  c->icon=creategadget(c->parent, 0, 0, 23, bh);
  if(cb)
    c->zoom=creategadget(c->parent, 0, 0, 23, bh);
  c->depth=creategadget(c->parent, 0, 0, 23, bh);
  spread_top_gadgets(c);
  setclientborder(c, 0, cb);
  if(cb)
    c->resize=creategadget(c->parent, c->pwidth-18, c->pheight-10, 18, 10);
  XMapSubwindows(dpy, c->parent);
  c->gc = XCreateGC(dpy, c->parent, 0, NULL);
  XSetFont(dpy, c->gc, dri.dri_Font->fid);
  sendconfig(c);
}

void redraw(Client *c, Window w)
{
  if(w==c->parent) {
    int x, y;
    x=c->pwidth-((c->proto&Psizeright)?18:4);
    y=c->pheight-((c->proto&Psizebottom)?10:2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, c->pwidth-1, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, c->pheight-1);
    XDrawLine(dpy, w, c->gc, 4, y, x, y);
    XDrawLine(dpy, w, c->gc, x, bh, x, y);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 1, c->pheight-1, c->pwidth-1, c->pheight-1);
    XDrawLine(dpy, w, c->gc, c->pwidth-1, 1, c->pwidth-1, c->pheight-1);
    XDrawLine(dpy, w, c->gc, 3, bh-1, 3, y);
    XDrawLine(dpy, w, c->gc, 3, bh-1, x, bh-1);
  } else if(w==c->close) {
    if(c->active) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
      XFillRectangle(dpy, w, c->gc, 7, h3, 4, h7-h3);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, c->gc, 7, h3, 4, h7-h3);
    if(c->clicked==w) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
      XDrawLine(dpy, w, c->gc, 0, 0, 18, 0);
      XDrawLine(dpy, w, c->gc, 0, 0, 0, bh-2);
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
      XDrawLine(dpy, w, c->gc, 0, bh-1, 18, bh-1);
      XDrawLine(dpy, w, c->gc, 18, 1, 18, bh-1);
    } else {
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
      XDrawLine(dpy, w, c->gc, 0, 0, 18, 0);
      XDrawLine(dpy, w, c->gc, 0, 0, 0, bh-1);
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
      XDrawLine(dpy, w, c->gc, 1, bh-1, 18, bh-1);
      XDrawLine(dpy, w, c->gc, 18, 1, 18, bh-1);    
    }
  } else if(w==c->drag) {
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->active?FILLTEXTPEN:TEXTPEN]);
    XSetBackground(dpy, c->gc, dri.dri_Pens[c->active?FILLPEN:BACKGROUNDPEN]);
    if(c->title.value)
      XDrawImageString(dpy, w, c->gc, 11, 1+dri.dri_Font->ascent,
		       c->title.value, c->title.nitems);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, c->dragw-1, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, bh-2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 0, bh-1, c->dragw-1, bh-1);
    XDrawLine(dpy, w, c->gc, c->dragw-1, 1, c->dragw-1, bh-1);
  } else if(w==c->icon) {
    if(c->active) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
      XFillRectangle(dpy, w, c->gc, 7, h8-4, 4, 2);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, c->gc, 7, h8-4, 4, 2);
    if(c->clicked==w) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[c->active? FILLPEN:BACKGROUNDPEN]);
      XDrawRectangle(dpy, w, c->gc, 5, h2, 12, h8-h2);
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
      XDrawRectangle(dpy, w, c->gc, 5, h8-(bh>11? 7:6), 9, (bh>11? 7:6));
    } else {
      XSetForeground(dpy, c->gc, dri.dri_Pens[c->active? FILLPEN:BACKGROUNDPEN]);
      XDrawRectangle(dpy, w, c->gc, 5, h8-(bh>11? 7:6), 9, (bh>11? 7:6));
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
      XDrawRectangle(dpy, w, c->gc, 5, h2, 12, h8-h2);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w?SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, 22, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, bh-2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w?SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 0, bh-1, 22, bh-1);
    XDrawLine(dpy, w, c->gc, 22, 0, 22, bh-1);
  } else if(w==c->zoom) {
    if(c->active) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w? SHINEPEN:FILLPEN]);
      XFillRectangle(dpy, w, c->gc, 5, h2, 12, h8-h2);
      XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w? FILLPEN:SHINEPEN]);
      XFillRectangle(dpy, w, c->gc, 6, h2, 5, h5-h2);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, c->gc, 6, h2, 5, h5-h2);
    XDrawRectangle(dpy, w, c->gc, 5, h2, 7, h5-h2);
    XDrawRectangle(dpy, w, c->gc, 5, h2, 12, h8-h2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w? SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, 22, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, bh-2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w? SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 0, bh-1, 22, bh-1);
    XDrawLine(dpy, w, c->gc, 22, 0, 22, bh-1);
  } else if(w==c->depth) {
    if(c->active) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[BACKGROUNDPEN]);
      XFillRectangle(dpy, w, c->gc, 4, h2, 10, h6-h2);
    }
    if(c->clicked!=w) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
      XDrawRectangle(dpy, w, c->gc, 4, h2, 10, h6-h2);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->active?SHINEPEN:BACKGROUNDPEN]);
    XFillRectangle(dpy, w, c->gc, 8, h4, 10, h8-h4);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, c->gc, 8, h4, 10, h8-h4);
    if(c->clicked==w)
      XDrawRectangle(dpy, w, c->gc, 4, h2, 10, h6-h2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w? SHADOWPEN:SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, 22, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, bh-2);
    XSetForeground(dpy, c->gc, dri.dri_Pens[c->clicked==w? SHINEPEN:SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 0, bh-1, 22, bh-1);
    XDrawLine(dpy, w, c->gc, 22, 0, 22, bh-1);
  } else if(w==c->resize) {
    static XPoint points[]={{4,6},{13,2},{14,2},{14,7},{4,7}};
    if(c->active) {
      XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
      XFillPolygon(dpy, w, c->gc, points, sizeof(points)/sizeof(points[0]), Convex, CoordModeOrigin);
    }
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawLines(dpy, w, c->gc, points, sizeof(points)/sizeof(points[0]), CoordModeOrigin);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, c->gc, 0, 0, 16, 0);
    XDrawLine(dpy, w, c->gc, 0, 0, 0, 8);
    XSetForeground(dpy, c->gc, dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, c->gc, 0, 9, 17, 9);
    XDrawLine(dpy, w, c->gc, 17, 0, 17, 9);
  }
}

void redrawclient(Client *c)
{
  unsigned long bgpix=dri.dri_Pens[c->active?FILLPEN:BACKGROUNDPEN];

  if((!c->parent) || c->parent == root)
    return;
  XSetWindowBackground(dpy, c->parent, bgpix);
  XSetWindowBackground(dpy, c->close, bgpix);
  XSetWindowBackground(dpy, c->drag, bgpix);
  XSetWindowBackground(dpy, c->icon, bgpix);
  if(c->zoom)
    XSetWindowBackground(dpy, c->zoom, bgpix);
  XSetWindowBackground(dpy, c->depth, bgpix);
  if(c->resize)
    XSetWindowBackground(dpy, c->resize, bgpix);
  XClearWindow(dpy, c->parent);
  XClearWindow(dpy, c->close);
  XClearWindow(dpy, c->drag);
  XClearWindow(dpy, c->icon);
  if(c->zoom)
    XClearWindow(dpy, c->zoom);
  XClearWindow(dpy, c->depth);
  if(c->resize)
    XClearWindow(dpy, c->resize);
  redraw(c, c->parent);
  redraw(c, c->close);
  redraw(c, c->drag);
  redraw(c, c->icon);
  if(c->zoom)
    redraw(c, c->zoom);
  redraw(c, c->depth);
  if(c->resize)
    redraw(c, c->resize);
}

extern Client *clickclient;
extern Window clickwindow;

void clickenter()
{
  redraw(clickclient, clickclient->clicked=clickwindow);
}

void clickleave()
{
  clickclient->clicked=None;
  redraw(clickclient, clickwindow);
}

void gadgetclicked(Client *c, Window w, XEvent *e)
{
  redraw(c, clickwindow=(clickclient=c)->clicked=w);
}

void gadgetaborted(Client *c)
{
  Window w;
  if(w=c->clicked) {
    c->clicked=None;
    redraw(c, w);
  }
  clickwindow=None;
  clickclient=NULL;
}

void gadgetunclicked(Client *c, XEvent *e)
{
  extern void createicon(Client *);
  extern void adjusticon(Client *);
  Window w;
  if(w=c->clicked) {
    c->clicked=None;
    redraw(c, w);
    if(w==c->close) {
      if((c->proto & Pdelete)&&!(e->xbutton.state&ShiftMask))
	sendcmessage(c->window, wm_protocols, wm_delete);
      else
	XKillClient(dpy, c->window);
    } else if(w==c->depth) {
      Client *c2;
      Window r,p,*children;
      unsigned int nchildren;
      if(XQueryTree(dpy, root, &r, &p, &children, &nchildren)) {
	if(children[nchildren-1]==c->parent ||
	   children[nchildren-1]==c->window) {
	  int n;
	  for(n=0; n<nchildren; n++)
	    if((c2=getclient(children[n])) && children[n]==c2->parent)
	      break;
	  if(n<nchildren) {
	    Window ws[2];
	    ws[0]=children[n];
	    ws[1]=c->parent;
	    XRestackWindows(dpy, ws, 2);
	  }
	} else
	  XRaiseWindow(dpy, c->parent);
	if(children) XFree(children);
      } else XRaiseWindow(dpy, c->parent);
    } else if(w==c->zoom) {
      XWindowAttributes xwa;
      XGetWindowAttributes(dpy, c->parent, &xwa);
      XMoveWindow(dpy, c->parent, c->x=c->zoomx, c->y=c->zoomy);
      resizeclientwindow(c, c->zoomw+c->framewidth, c->zoomh+c->frameheight);
      c->zoomx=xwa.x;
      c->zoomy=xwa.y;
      c->zoomw=xwa.width-c->framewidth;
      c->zoomh=xwa.height-c->frameheight;
/*      XWarpPointer(dpy, None, c->zoom, 0, 0, 0, 0, 23/2, h5);  */
      sendconfig(c);
    } else if(w==c->icon) {
      if(!(c->iconwin))
	createicon(c);
      XUnmapWindow(dpy, c->parent);
      adjusticon(c);
      XMapWindow(dpy, c->iconwin);
      setclientstate(c, IconicState);
    }
  }
  clickwindow=None;
  clickclient=NULL;
}
