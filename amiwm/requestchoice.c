#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawinfo.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#define BUT_BUTSPACE (2*(2+5))
#define BUT_INTSPACE 12
#define BUT_EXTSPACE 4
#define BUT_VSPACE   6
#define TXT_HSPACE   48
#define TXT_TOPSPACE 4
#define TXT_MIDSPACE 3
#define TXT_BOTSPACE 4

struct choice {
  struct choice *next;
  const char *text;
  int l, w;
  Window win;
} *firstchoice=NULL, *lastchoice=NULL;

struct line {
  struct line *next;
  const char *text;
  int l, w, h;
} *firstline=NULL, *lastline=NULL;

Display *dpy;
Window root, mainwin, textwin;
Colormap wm_cmap;
char *progname;
GC gc;
Pixmap stipple;

int totw=0, maxw=0, toth=0, nchoices=0;
int depressed=0;
struct choice *selected=NULL;

struct DrawInfo dri;

void selection(int n)
{
  printf("%d\n", n);
  XDestroyWindow(dpy, mainwin);
  XCloseDisplay(dpy);
  exit(0);
}

void *myalloc(size_t s)
{
  void *p=calloc(s,1);
  if(p)
    return p;
  fprintf(stderr, "%s: out of memory!\n", progname);
  exit(1);
}

void addchoice(const char *txt)
{
  struct choice *c=myalloc(sizeof(struct choice));
  if(lastchoice)
    lastchoice->next=c;
  else
    firstchoice=c;
  lastchoice=c;
  c->l=strlen(c->text=txt);
  totw+=(c->w=XTextWidth(dri.dri_Font, c->text, c->l))+BUT_BUTSPACE;
  nchoices++;
}

void addline(const char *txt)
{
  struct line *l=myalloc(sizeof(struct line));
  if(lastline)
    lastline->next=l;
  else
    firstline=l;
  lastline=l;
  l->l=strlen(l->text=txt);
  l->w=XTextWidth(dri.dri_Font, l->text, l->l);
  toth+=l->h=dri.dri_Font->ascent+dri.dri_Font->descent;
  if(l->w>maxw)
    maxw=l->w;
}

void refresh_text()
{
  int w=totw-BUT_EXTSPACE-BUT_EXTSPACE;
  int h=toth-TXT_TOPSPACE-TXT_MIDSPACE-TXT_BOTSPACE-BUT_VSPACE-
    (dri.dri_Font->ascent+dri.dri_Font->descent);
  int x=(totw-maxw+TXT_HSPACE)>>1;
  int y=((dri.dri_Font->ascent+dri.dri_Font->descent)>>1)+dri.dri_Font->ascent;
  struct line *l;
  XSetForeground(dpy, gc, dri.dri_Pens[SHADOWPEN]);
  XDrawLine(dpy, textwin, gc, 0, 0, w-2, 0);
  XDrawLine(dpy, textwin, gc, 0, 0, 0, h-2);
  XSetForeground(dpy, gc, dri.dri_Pens[SHINEPEN]);
  XDrawLine(dpy, textwin, gc, 0, h-1, w-1, h-1);
  XDrawLine(dpy, textwin, gc, w-1, 0, w-1, h-1);  
  XSetForeground(dpy, gc, dri.dri_Pens[TEXTPEN]);
  for(l=firstline; l; l=l->next) {
    XDrawString(dpy, textwin, gc, x, y, l->text, l->l);
    y+=dri.dri_Font->ascent+dri.dri_Font->descent;
  }
}

void refresh_choice(struct choice *c)
{
  int w=c->w+BUT_BUTSPACE;
  int h=dri.dri_Font->ascent+dri.dri_Font->descent+BUT_VSPACE;
  XSetForeground(dpy, gc, dri.dri_Pens[TEXTPEN]);
  XDrawString(dpy, c->win, gc, BUT_BUTSPACE/2,
	      dri.dri_Font->ascent+BUT_VSPACE/2, c->text, c->l);
  XSetForeground(dpy, gc, dri.dri_Pens[(c==selected && depressed)?
				     SHADOWPEN:SHINEPEN]);
  XDrawLine(dpy, c->win, gc, 0, 0, w-2, 0);
  XDrawLine(dpy, c->win, gc, 0, 0, 0, h-2);
  XSetForeground(dpy, gc, dri.dri_Pens[(c==selected && depressed)?
				     SHINEPEN:SHADOWPEN]);
  XDrawLine(dpy, c->win, gc, 1, h-1, w-1, h-1);
  XDrawLine(dpy, c->win, gc, w-1, 1, w-1, h-1);  
  XSetForeground(dpy, gc, dri.dri_Pens[BACKGROUNDPEN]);
  XDrawPoint(dpy, c->win, gc, w-1, 0);
  XDrawPoint(dpy, c->win, gc, 0, h-1);
}

void split(char *str, const char *delim, void (*func)(const char *))
{
  char *p;
  if(p=strtok(str, delim))
    do {
      (*func)(p);
    } while(p=strtok(NULL, delim));
}

struct choice *getchoice(Window w)
{
  struct choice *c;
  for(c=firstchoice; c; c=c->next)
    if(w == c->win)
      return c;
  return NULL;
}

void toggle(struct choice *c)
{
  XSetWindowBackground(dpy, c->win, dri.dri_Pens[(depressed&&c==selected)?
					       FILLPEN:BACKGROUNDPEN]);
  XClearWindow(dpy, c->win);
  refresh_choice(c);
}

void abortchoice()
{
  if(depressed) {
    depressed=0;
    toggle(selected);
  }
  selected=NULL;
}

void endchoice()
{
  struct choice *c=selected, *c2=firstchoice;
  int n;

  abortchoice();
  if(c==lastchoice)
    selection(0);
  for(n=1; c2; n++, c2=c2->next)
    if(c2==c)
      selection(n);
  selection(0);
}

int main(int argc, char *argv[])
{
  XWindowAttributes attr;
  static XSizeHints size_hints;
  static XTextProperty txtprop1, txtprop2;
  int x, y, extra=0, n=0;
  struct choice *c;

  progname=argv[0];
  if(argc<4) {
    fprintf(stderr, "Usage:\n%s TITLE/A,BODY/A,GADGETS/M\n", progname);
    exit(1);
  }
  if(!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "%s: cannot connect to X server %s\n", progname,
	    XDisplayName(NULL));
    exit(1);
  }
  root = RootWindow(dpy, DefaultScreen(dpy));
  XGetWindowAttributes(dpy, root, &attr);
  wm_cmap = attr.colormap;
  init_dri(&dri, DefaultScreen(dpy), False);

  split(argv[2], "\n", addline);
  for(x=3; x<argc; x++)
    split(argv[x], "|\n", addchoice);

  totw+=BUT_EXTSPACE+BUT_EXTSPACE+BUT_INTSPACE*(nchoices-1);
  toth+=2*(dri.dri_Font->ascent+dri.dri_Font->descent)+TXT_TOPSPACE+
    TXT_MIDSPACE+TXT_BOTSPACE+BUT_VSPACE;
  maxw+=TXT_HSPACE+BUT_EXTSPACE+BUT_EXTSPACE;

  if(maxw>totw) {
    extra=maxw-totw;
    totw=maxw;
  }

  mainwin=XCreateSimpleWindow(dpy, root, 0, 0, totw, toth, 1,
			      dri.dri_Pens[SHADOWPEN],
			      dri.dri_Pens[BACKGROUNDPEN]);
  gc=XCreateGC(dpy, mainwin, 0, NULL);
  XSetBackground(dpy, gc, dri.dri_Pens[BACKGROUNDPEN]);
  XSetFont(dpy, gc, dri.dri_Font->fid);
  stipple=XCreatePixmap(dpy, mainwin, 2, 2, attr.depth);
  XSetForeground(dpy, gc, dri.dri_Pens[BACKGROUNDPEN]);
  XFillRectangle(dpy, stipple, gc, 0, 0, 2, 2);
  XSetForeground(dpy, gc, dri.dri_Pens[SHINEPEN]);
  XDrawPoint(dpy, stipple, gc, 0, 1);
  XDrawPoint(dpy, stipple, gc, 1, 0);
  XSetWindowBackgroundPixmap(dpy, mainwin, stipple);
  textwin=XCreateSimpleWindow(dpy, mainwin, BUT_EXTSPACE, TXT_TOPSPACE, totw-
			      BUT_EXTSPACE-BUT_EXTSPACE, toth-TXT_TOPSPACE-
			      TXT_MIDSPACE-TXT_BOTSPACE-BUT_VSPACE-
			      (dri.dri_Font->ascent+dri.dri_Font->descent),
			      0, dri.dri_Pens[SHADOWPEN],
			      dri.dri_Pens[BACKGROUNDPEN]);
  XSelectInput(dpy, textwin, ExposureMask);
  x=BUT_EXTSPACE;
  y=toth-TXT_BOTSPACE-(dri.dri_Font->ascent+dri.dri_Font->descent)-BUT_VSPACE;
  for(c=firstchoice; c; c=c->next) {
    c->win=XCreateSimpleWindow(dpy, mainwin,
			       x+(nchoices==1? (extra>>1):
				  n++*extra/(nchoices-1)),
			       y, c->w+BUT_BUTSPACE,
			       dri.dri_Font->ascent+dri.dri_Font->descent+
			       BUT_VSPACE, 0,
			       dri.dri_Pens[SHADOWPEN],
			       dri.dri_Pens[BACKGROUNDPEN]);
    XSelectInput(dpy, c->win, ExposureMask|ButtonPressMask|ButtonReleaseMask|
		 EnterWindowMask|LeaveWindowMask);
    x+=c->w+BUT_BUTSPACE+BUT_INTSPACE;
  }
  size_hints.flags = PResizeInc;
  txtprop1.value=(unsigned char *)argv[1];
  txtprop2.value=(unsigned char *)"RequestChoice";
  txtprop2.encoding=txtprop1.encoding=XA_STRING;
  txtprop2.format=txtprop1.format=8;
  txtprop1.nitems=strlen((char *)txtprop1.value);
  txtprop2.nitems=strlen((char *)txtprop2.value);
  XSetWMProperties(dpy, mainwin, &txtprop1, &txtprop2, argv, argc,
		   &size_hints, NULL, NULL);
  XMapSubwindows(dpy, mainwin);
  XMapRaised(dpy, mainwin);
  for(;;) {
    XEvent event;
    XNextEvent(dpy, &event);
    switch(event.type) {
    case Expose:
      if(!event.xexpose.count)
	if(event.xexpose.window == textwin)
	  refresh_text();
	else if(c=getchoice(event.xexpose.window))
	  refresh_choice(c);
      break;
    case LeaveNotify:
      if(depressed && event.xcrossing.window==selected->win) {
	depressed=0;
	toggle(selected);
      }
      break;
    case EnterNotify:
      if((!depressed) && selected && event.xcrossing.window==selected->win) {
	depressed=1;
	toggle(selected);
      }
      break;
    case ButtonPress:
      if(event.xbutton.button==Button1 &&
	 (c=getchoice(event.xbutton.window))) {
	abortchoice();
	depressed=1;
	toggle(selected=c);
      }
      break;
    case ButtonRelease:
      if(event.xbutton.button==Button1 && selected)
	if(depressed)
	  endchoice();
	else
	  abortchoice();
      break;
    }
  }
}
