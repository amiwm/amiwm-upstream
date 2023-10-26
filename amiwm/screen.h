#ifndef SCREEN_H
#define SCREEN_H

typedef struct _Scrn {
  struct _Scrn *behind, *upfront;
  Window root, back;
  Colormap cmap;
  Visual *visual;
  GC gc, icongc, rubbergc, menubargc;
  char *title;
  char *deftitle;
  struct DrawInfo dri;
  int fh,bh,h2,h3,h4,h5,h6,h7,h8;
  int width, height, depth, y, bw;
  Pixmap default_tool_pm, default_tool_pm2, disabled_stipple;
  unsigned int default_tool_pm_w, default_tool_pm_h, lh;
  Window menubar, menubarparent, menubardepth;
  int hotkeyspace, checkmarkspace, subspace, menuleft;
  struct _Icon *icons, *firstselected;
  struct Menu *firstmenu;
} Scrn;

extern Scrn *scr, *front;

#endif
