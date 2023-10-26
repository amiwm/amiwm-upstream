#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

#include "alloc.h"
#include "drawinfo.h"
#include "prefs.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

extern Display *dpy;
extern struct DrawInfo dri;
extern Window root;
extern int rootdepth;
extern Visual *rootvisual;
extern Colormap wm_cmap;
extern char *progname;

extern GC menubargc;

unsigned long iconcolor[8];

char *iconcolorname[8];

static char *sysiconcolorname[]={
  "#aaaaaa", "#000000", "#ffffff", "#6688bb",
  "#ee4444", "#55dd55", "#0044dd", "#ee9e00"
};

static char *magicwbcolorname[]={
  "#aaaaaa", "#000000", "#ffffff", "#6688bb",
  "#999999", "#bbbbbb", "#bbaa99", "#ffbbaa"  
};


#define	WBDISK		1
#define	WBDRAWER	2
#define	WBTOOL		3
#define	WBPROJECT	4
#define	WBGARBAGE	5
#define	WBDEVICE	6
#define	WBKICK		7
#define WBAPPICON	8

struct Gadget
{
  struct Gadget *NextGadget;
  INT16 LeftEdge, TopEdge, Width, Height;
  BITS16 Flags, Activation, GadgetType;
/*
  void *GadgetRender, *SelectRender;
  struct IntuiText *GadgetText;
  INT32 MutualExclude;
  void *SpecialInfo;
  CARD16 GadgetID;
  void *UserData;
*/
  BITS16 foo[13];
};

struct Image
{
  INT16 LeftEdge, TopEdge, Width, Height, Depth;
/*
  CARD16 *ImageData;
*/
  BITS16 foo[2];
  CARD8 PlanePick, PlaneOnOff;
  struct Image *NextImage;
};

struct DiskObject {
  CARD16 do_Magic, do_Version;
  struct Gadget do_Gadget;
  CARD8 do_Type, pad[1];
/*
  char *do_DefaultTool, **do_ToolTypes;
  INT32 do_CurrentX, do_CurrentY;
  struct DrawerData *do_DrawerData;
  char *do_ToolWindow;
  INT32 do_StackSize;
*/
  BITS16 foo[14];
};

#define WB_DISKMAGIC	0xe310
#define WB_DISKVERSION	1
#define WB_DISKREVISION	1
#define WB_DISKREVISIONMASK	255

#define NO_ICON_POSITION	(0x80000000)


static INT16 swap16(INT16 x)
{
  return ((x&0xff)<<8)|((x&0xff00)>>8);
}

static Pixmap int_load_do(const char *filename)
{
  struct DiskObject dobj;
  struct Image im;
  Pixmap pm;
  int fd, bitmap_pad, x, y, lame_byteorder=0;
  char *foo_data;

  if((fd=open(filename, O_RDONLY))>=0) {
    if(sizeof(dobj)==read(fd, &dobj, sizeof(dobj)) &&
       ((dobj.do_Magic==WB_DISKMAGIC && dobj.do_Version==WB_DISKVERSION)
	||(((CARD16)swap16((INT16)dobj.do_Magic))==WB_DISKMAGIC &&
	   ((CARD16)swap16((INT16)dobj.do_Version))==WB_DISKVERSION &&
	   (lame_byteorder=1))) &&
       lseek(fd, ((dobj.do_Type==WBDISK||dobj.do_Type==WBDRAWER||
		   dobj.do_Type==WBGARBAGE)?(0x4e)+0x38:0x4e), SEEK_SET)>=0 &&
       sizeof(im)==read(fd, &im, sizeof(im))) {
      if(lame_byteorder) {
	dobj.do_Gadget.Width=swap16(dobj.do_Gadget.Width);
	dobj.do_Gadget.Height=swap16(dobj.do_Gadget.Height);
	im.LeftEdge=swap16(im.LeftEdge);
	im.TopEdge=swap16(im.TopEdge);
	im.Width=swap16(im.Width);
	im.Height=swap16(im.Height);
	im.Depth=swap16(im.Depth);
      }
      {
	int bpr=2*((im.Width+15)>>4), imgsz=bpr*im.Height*im.Depth;
	XImage *ximg;
#ifndef HAVE_ALLOCA
	unsigned char *img=malloc(imgsz);
	if(!img) {
	  close(fd);
	  return None;
	}
#else
	unsigned char *img=alloca(imgsz);
#endif
	if(imgsz!=read(fd, img, imgsz)) {
	  close(fd);
#ifndef HAVE_ALLOCA
	  free(img);
#endif
	  return None;
	}
	close(fd);
	if (rootdepth > 16)
	  bitmap_pad = 32;
	else if (rootdepth > 8)
	  bitmap_pad = 16;
	else
	  bitmap_pad = 8;
	ximg=XCreateImage(dpy, rootvisual, rootdepth, ZPixmap, 0, NULL,
			  im.Width, im.Height, bitmap_pad, 0);
#ifndef HAVE_ALLOCA
	if(!(ximg->data = malloc(ximg->bytes_per_line * im.Height))) {
	  XDestroyImage(ximg);
	  free(img);
	  return None;
	}
#else
	foo_data = alloca(ximg->bytes_per_line * im.Height);
	ximg->data = foo_data;
#endif
	for(y=0; y<im.Height; y++)
	  for(x=0; x<im.Width; x++) {
	    unsigned char b=1, v=im.PlaneOnOff&~(im.PlanePick);
	    INT16 p=0;
	    while(p<im.Depth && b) {
	      if(b&im.PlanePick)
		if(img[(p++*im.Height+y)*bpr+(x>>3)]&(128>>(x&7)))
		  v|=b;
	      b<<=1;
	    }
	    XPutPixel(ximg, x, y, iconcolor[v&7]);
	  }
	pm=XCreatePixmap(dpy, root, dobj.do_Gadget.Width, dobj.do_Gadget.Height,
			 rootdepth);
	XSetForeground(dpy, menubargc, dri.dri_Pens[BACKGROUNDPEN]);
	XFillRectangle(dpy, pm, menubargc, 0, 0,
		       dobj.do_Gadget.Width, dobj.do_Gadget.Height);
	XPutImage(dpy, pm, menubargc, ximg, 0, 0, im.LeftEdge, im.TopEdge,
		  im.Width, im.Height);
#ifndef HAVE_ALLOCA
	free(ximg->data);
	free(img);
#endif
	ximg->data=NULL;
	XDestroyImage(ximg);
	return pm;
      }
    }
    close(fd);
  }
  return None;
}

Pixmap load_do(const char *filename)
{
#ifdef AMIGAOS
  char fn[256];
  strncpy(fn, prefs.icondir, sizeof(fn)-1);
  fn[sizeof(fn)-1]='\0';
  AddPart(fn,filename,sizeof(fn));
#else
  int rl=strlen(filename)+strlen(prefs.icondir)+2;
#ifdef HAVE_ALLOCA
  char *fn=alloca(rl);
#else
  char fn[1024];
#endif
  sprintf(fn, "%s/%s", prefs.icondir, filename);
#endif
  return int_load_do(fn);
}

void init_iconpalette()
{
  extern Status myXAllocNamedColor(Display *, Colormap, char *, XColor *, XColor *);
  XColor scr, xact;
  char *name;
  int i;

  for(i=0; i<8; i++) {
    if(!myXAllocNamedColor(dpy, wm_cmap, name = iconcolorname[i], &scr, &xact)) {
      fprintf(stderr, "%s: cannot allocate color %s\n", progname, name);
      exit(1);
    }
    iconcolor[i]=scr.pixel;
  }
}

void set_mwb_palette()
{
  memcpy(iconcolorname, magicwbcolorname, sizeof(iconcolorname));
}

void set_sys_palette()
{
  memcpy(iconcolorname, sysiconcolorname, sizeof(iconcolorname));
}
