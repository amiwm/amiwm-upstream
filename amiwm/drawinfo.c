#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
#include "drawinfo.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#define TAG_DRI_VERSION 1
#define TAG_DRI_PENS    2
#define TAG_DRI_FONT    3

extern char *progname;
extern Display *dpy;
extern Colormap wm_cmap;

static char black[] = "#000000";
static char white[] = "#ffffff";
static char grey[] = "#aaaaaa";
static char blue[] = "#6688bb";

char *default_colors[NUMDRIPENS] = {
  black, white, black, white, black, blue, black, grey, white,
  black, white, black
};

char *default_screenfont =
"-b&h-lucida-medium-r-normal-sans-12-*-*-*-*-*-iso8859-1";

static void setdriprop(Atom atom, Atom typ, Window win, struct DrawInfo *dri)
{
  CARD32 prop[/* NUMDRIPENS+6 */ 100], *p=prop;
  CARD16 i;
  *p++=TAG_DRI_VERSION;
  *p++=dri->dri_Version;
  *p++=TAG_DRI_FONT;
  *p++=dri->dri_Font->fid;
  *p++=TAG_DRI_PENS;
  *p++=dri->dri_NumPens;
  for(i=0; i<dri->dri_NumPens; i++)
    *p++=dri->dri_Pens[i];
  XChangeProperty(dpy, win, atom, typ, 32, PropModeReplace,
		  (unsigned char *)prop, p-prop);
}

static void getdriprop(Atom atom, Atom typ, Window win, struct DrawInfo *dri)
{
  Atom atyp;
  int afmt;
  unsigned long nitems, extra;
  int i,j,n;
  CARD32 *prop=NULL;
  if(XGetWindowProperty(dpy, win, atom, 0l, NUMDRIPENS+6, False, typ,
			&atyp, &afmt, &nitems, &extra,
			(unsigned char **)&prop) == Success &&
     atyp==typ && afmt==32) {
    for(i=0; i<nitems; )
      switch(prop[i++]) {
      case TAG_DRI_VERSION:
	if(i<nitems)
	  dri->dri_Version=prop[i++];
	break;
      case TAG_DRI_FONT:
	if(i<nitems)
	  dri->dri_Font=XQueryFont(dpy, prop[i++]);
	break;
      case TAG_DRI_PENS:
	if(i<nitems) {
	  n=prop[i++];
	  if(n>nitems-i)
	    n=nitems-i;
	  if(n>0) {
	    dri->dri_NumPens=n;
	    if(dri->dri_Pens=calloc(sizeof(unsigned long), n))
	      for(j=0; j<n; j++)
		dri->dri_Pens[j]=prop[i++];
	  }
	}
	break;
      }
  }
  if(prop)
    XFree(prop);
}

Status myXAllocNamedColor(Display *dpy, Colormap cmap, char *color_name,
			  XColor *screen_def, XColor *exact_def)
{
  Status s;
  int len;

  if((s=XAllocNamedColor(dpy, cmap, color_name, screen_def, exact_def)))
    return s;
  if((s=XLookupColor(dpy, cmap, color_name, screen_def, exact_def)))
    return s;
  if((s=XParseColor(dpy, cmap, color_name, exact_def))) {
    *screen_def = *exact_def;
    s=XAllocColor(dpy, cmap, screen_def);
  }
  return s;
}

unsigned long allocdripen(int n)
{
  XColor screen, exact;
  char *name;

  if(!myXAllocNamedColor(dpy, wm_cmap, name = default_colors[n],
			 &screen, &exact)) {
    fprintf(stderr, "%s: cannot allocate color %s\n", progname, name);
    exit(1);
  }
  return screen.pixel;
}

void init_dri(struct DrawInfo *dri, int screen_num, int override)
{
  int i;
  Atom driatom, dritypatom;

  memset(dri, 0, sizeof(*dri));
  driatom=XInternAtom(dpy, "AMIWM_DRAWINFO", False);
  dritypatom=XInternAtom(dpy, "DRAWINFO", False);
  if(!override)
    getdriprop(driatom, dritypatom, RootWindow(dpy, screen_num), dri);
  if(!dri->dri_Version)
    dri->dri_Version = DRI_VERSION;
  if(!dri->dri_Pens) {
    if(!(dri->dri_Pens = calloc(sizeof(unsigned long), NUMDRIPENS))) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
    for(i=0; i<NUMDRIPENS; i++)
      dri->dri_Pens[i] = allocdripen(i);
    dri->dri_NumPens = NUMDRIPENS;
  }
  if(!dri->dri_Font)
    if(!(dri->dri_Font = XLoadQueryFont(dpy, default_screenfont))) {
      fprintf(stderr, "%s: cannot open font %s\n", progname,
	      default_screenfont);
      exit(1);
    }
  if(override)
    setdriprop(driatom, dritypatom, RootWindow(dpy, screen_num), dri);
}
