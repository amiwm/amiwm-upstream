#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include "libami.h"
#include "alloc.h"
#include "drawinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

Pixmap image_to_pixmap(Display *dpy, Window win, GC gc, unsigned long bg,
		       unsigned long *iconcolor, int iconcolormask,
		       struct Image *im, int width, int height)
{
  int bpr, bitmap_pad, x, y;
  XImage *ximg;
  unsigned char *img;
  Pixmap pm;
  XWindowAttributes attr;

  if(!dpy || !win || !gc || !im || !(img=(unsigned char *)im->ImageData))
    return None;

  if(width<=0) width=im->Width;
  if(height<=0) height=im->Height;

  XGetWindowAttributes(dpy, win, &attr);
  
  bpr=2*((im->Width+15)>>4);
  if (attr.depth > 16)
    bitmap_pad = 32;
  else if (attr.depth > 8)
    bitmap_pad = 16;
  else
    bitmap_pad = 8;
  ximg=XCreateImage(dpy, attr.visual, attr.depth, ZPixmap, 0, NULL,
		    im->Width, im->Height, bitmap_pad, 0);
#ifndef HAVE_ALLOCA
  if(!(ximg->data = malloc(ximg->bytes_per_line * im->Height))) {
    XDestroyImage(ximg);
    return None;
  }
#else
  ximg->data = alloca(ximg->bytes_per_line * im->Height);
#endif
  for(y=0; y<im->Height; y++)
    for(x=0; x<im->Width; x++) {
      unsigned char b=1, v=im->PlaneOnOff&~(im->PlanePick);
      INT16 p=0;
      while(p<im->Depth && b) {
	if(b&im->PlanePick)
	  if(img[(p++*im->Height+y)*bpr+(x>>3)]&(128>>(x&7)))
	    v|=b;
	b<<=1;
      }
      XPutPixel(ximg, x, y, iconcolor[v&iconcolormask]);
    }
  if((pm=XCreatePixmap(dpy, win, width, height, attr.depth))) {
    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, pm, gc, 0, 0, width, height);
    XPutImage(dpy, pm, gc, ximg, 0, 0, im->LeftEdge, im->TopEdge,
	      im->Width, im->Height);
  }
#ifndef HAVE_ALLOCA
  free(ximg->data);
#endif
  ximg->data=NULL;
  XDestroyImage(ximg);
  return pm;
}
