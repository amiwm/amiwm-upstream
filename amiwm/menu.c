#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "alloc.h"
#include "drawinfo.h"
#include "prefs.h"
#include "client.h"
#include "version.h"

#ifdef AMIGAOS
#include <pragmas/xlib_pragmas.h>
extern struct Library *XLibBase;
#endif

#ifdef AMIGAOS
#define BIN_PREFIX AMIWM_HOME
#else
#define BIN_PREFIX AMIWM_HOME"/"
#endif

#define CHECKIT 1
#define CHECKED 2
#define DISABLED 4

extern int rootwidth, bh;
/*static*/ GC menubargc=None;
Pixmap disabled_stipple;

extern Display *dpy;
extern Window root;
extern struct DrawInfo dri;
extern int bh,h2,h4,h6,h8;
extern Cursor wm_curs;

Window menubar, menubardepth, menubarparent;

static int menuleft, hotkeyspace, checkmarkspace;

static struct ToolItem {
  struct ToolItem *next;
  const char *name, *cmd;
} *firsttoolitem=NULL, *lasttoolitem=NULL;

static struct Item {
  struct Item *next;
  Window win;
  const char *text;
  int textlen;
  char hotkey, flags;
  struct Menu *menu, *sub;
} *activeitem=NULL, *activesubitem=NULL;

static struct Menu {
  struct Menu *next;
  struct Item *item;
  Window win, parent;
  const char *title;
  int titlelen;
  int width, height;
  char flags;
  struct Item *firstitem, *lastitem;
} *firstmenu=NULL, *activemenu=NULL, *activesubmenu=NULL;

#ifdef AMIGAOS
void spawn(const char *cmd)
{
  char *line=malloc(strlen(cmd)+12);
  if(line) {
    sprintf(line, "RUN <>NIL: %s", cmd);
    system(line);
    free(line);
  }
}
#else
void spawn(const char *cmd)
{
#ifdef HAVE_ALLOCA
  char *line=alloca(strlen(cmd)+4);
#else
  char *line=malloc(strlen(cmd)+4);
  if(line) {
#endif
  sprintf(line, "%s &", cmd);
  system(line);
#ifndef HAVE_ALLOCA
    free(line);
  }
#endif
}
#endif

void add_toolitem(const char *n, const char *c)
{
  struct ToolItem *ti;
  if((ti=malloc(sizeof(struct ToolItem)))) {
    ti->name=n;
    ti->cmd=c;
    ti->next=NULL;
    if(lasttoolitem)
      lasttoolitem->next=ti;
    else
      firsttoolitem=ti;
    lasttoolitem=ti;
  }
}

static void menu_layout(struct Menu *menu)
{
  XSetWindowAttributes attr;
  XWindowAttributes attr2;
  int w, x, y;
  struct Item *item;
  if(menu->win) {
    XGetWindowAttributes(dpy, menu->win, &attr2);
    x=attr2.x; y=bh-2;
  } else {
    XGetWindowAttributes(dpy, menu->item->win, &attr2);
    x=attr2.x+attr2.width-hotkeyspace+9; y=attr2.y-2;
    XGetWindowAttributes(dpy, menu->item->menu->parent, &attr2);
    x+=attr2.x; y+=attr2.y;
  }
  menu->width=menu->height=0;
  for(item=menu->firstitem; item; item=item->next) {
    if(item->text)
      menu->height+=dri.dri_Font->ascent+dri.dri_Font->descent+1;
    else
      menu->height+=6;
    w=XTextWidth(dri.dri_Font, item->text, item->textlen)+2;
    if(item->hotkey)
      w+=hotkeyspace;
    if(item->flags&CHECKIT)
      w+=checkmarkspace;
    if(w>menu->width)
      menu->width=w;
  }
  menu->width+=6;
  menu->height+=2;
  attr.override_redirect=True;
  attr.background_pixel=dri.dri_Pens[BARBLOCKPEN];
  attr.border_pixel=dri.dri_Pens[BARDETAILPEN];
  menu->parent=XCreateWindow(dpy, root, x, y,
			     menu->width, menu->height, 1,
			     CopyFromParent, InputOutput, CopyFromParent,
			     CWOverrideRedirect|CWBackPixel|CWBorderPixel,
			     &attr);
  XSelectInput(dpy, menu->parent, ExposureMask);
  w=1;
  for(item=menu->firstitem; item; item=item->next) {
    int h=(item->text? dri.dri_Font->ascent+dri.dri_Font->descent+1:6);
    item->win=XCreateWindow(dpy, menu->parent, 3, w, menu->width-6, h, 0,
			    CopyFromParent, InputOutput, CopyFromParent,
			    CWOverrideRedirect|CWBackPixel, &attr);
    w+=h;
    XSelectInput(dpy, item->win, ExposureMask|ButtonReleaseMask|
		 EnterWindowMask|LeaveWindowMask);
  }
  XMapSubwindows(dpy, menu->parent);
}

static struct Menu *add_menu(const char *name, char flags)
{
  struct Menu *menu=calloc(1, sizeof(struct Menu));
  XSetWindowAttributes attr;
  int w;

  if(menu) {
    attr.override_redirect=True;
    attr.background_pixel=dri.dri_Pens[BARBLOCKPEN];
    w=XTextWidth(dri.dri_Font, name, menu->titlelen=strlen(name))+8;
    menu->win=XCreateWindow(dpy, menubarparent, menuleft, 0, w, bh-1, 0,
			    CopyFromParent, InputOutput, CopyFromParent,
			    CWOverrideRedirect|CWBackPixel, &attr);
    XMapWindow(dpy, menu->win);
    XSelectInput(dpy, menu->win, ExposureMask|EnterWindowMask|LeaveWindowMask);
    menuleft+=w+6;
    menu->flags=flags;
    menu->title=name;
    menu->item=NULL;
    menu->firstitem=menu->lastitem=NULL;
    menu->next=firstmenu;
    firstmenu=menu;
  }
  return menu;
}

static struct Item *add_item(struct Menu *m, const char *name, char key,
			     char flags)
{
  struct Item *item=calloc(1, sizeof(struct Item));

  if(item) {
    if(name) {
      item->text=name;
      item->textlen=strlen(name);
    } else item->text=NULL;
    item->hotkey=key;
    item->flags=flags;
    item->menu=m;
    item->sub=NULL;
    item->next=NULL;
    if(m->lastitem)
      m->lastitem->next=item;
    else
      m->firstitem=item;
    m->lastitem=item;
  }
  return item;  
}

static struct Menu *sub_menu(struct Item *i, char flags)
{
  struct Menu *menu=calloc(1, sizeof(struct Menu));

  if(menu) {
    menu->flags=flags;
    menu->title=NULL;
    menu->firstitem=menu->lastitem=NULL;
    menu->next=NULL;
    menu->item=i;
    i->sub=menu;
  }
  return menu;
}

void redraw_menu(struct Menu *m, Window w)
{
  int active=(m==activemenu && !(m->flags & DISABLED));
  XSetForeground(dpy, menubargc, dri.dri_Pens[active?BARBLOCKPEN:
					      BARDETAILPEN]);
  XSetBackground(dpy, menubargc, dri.dri_Pens[active?BARDETAILPEN:
					      BARBLOCKPEN]);
  XDrawImageString(dpy, w, menubargc, 4, 1+dri.dri_Font->ascent,
		   m->title, m->titlelen);
}

void redraw_item(struct Item *i, Window w)
{
  struct Menu *m=i->menu;
  int s=dri.dri_Font->ascent>>1;
  if((i==activeitem || i==activesubitem) && !(i->flags&DISABLED)) {
    XSetForeground(dpy, menubargc, dri.dri_Pens[BARBLOCKPEN]);
    XSetBackground(dpy, menubargc, dri.dri_Pens[BARDETAILPEN]);
  } else {
    XSetForeground(dpy, menubargc, dri.dri_Pens[BARDETAILPEN]);
    XSetBackground(dpy, menubargc, dri.dri_Pens[BARBLOCKPEN]);
  }
  if(i->text)
    XDrawImageString(dpy, w, menubargc, (i->flags&CHECKIT)?1+checkmarkspace:1,
		     dri.dri_Font->ascent+1, i->text, i->textlen);
  else
    XFillRectangle(dpy, w, menubargc, 2, 2, m->width-10, 2);
  if(i->sub) {
    int x=m->width-6-hotkeyspace-1+8;
    XDrawImageString(dpy, w, menubargc, x+dri.dri_Font->ascent+1,
		     1+dri.dri_Font->ascent, "»", 1);
  } else if(i->hotkey) {
    int x=m->width-6-hotkeyspace-1+8;
    XDrawLine(dpy, w, menubargc, x, 1+s, x+s, 1);
    XDrawLine(dpy, w, menubargc, x+s, 1, x+s+s, 1+s);
    XDrawLine(dpy, w, menubargc, x+s+s, 1+s, x+s, 1+s+s);
    XDrawLine(dpy, w, menubargc, x+s, 1+s+s, x, 1+s);
    XDrawImageString(dpy, w, menubargc, x+dri.dri_Font->ascent+1,
		     1+dri.dri_Font->ascent, &i->hotkey, 1);
  }
  if(i->flags&CHECKED) {
    XDrawLine(dpy, w, menubargc, 0, s, s, dri.dri_Font->ascent);
    XDrawLine(dpy, w, menubargc, s, dri.dri_Font->ascent, s+s, 0);
  }
  if(i->flags&DISABLED) {
    XSetStipple(dpy, menubargc, disabled_stipple);
    XSetFillStyle(dpy, menubargc, FillStippled);
    XSetForeground(dpy, menubargc, dri.dri_Pens[BARBLOCKPEN]);
    XFillRectangle(dpy, w, menubargc, 0, 0,
		   m->width-6, dri.dri_Font->ascent+dri.dri_Font->descent+1);
    XSetFillStyle(dpy, menubargc, FillSolid);
  }
}

void createmenubar()
{
  XSetWindowAttributes attr;
  struct Menu *m, *sm1, *sm2, *sm3;
  struct ToolItem *ti;

  attr.override_redirect=True;
  attr.background_pixel=dri.dri_Pens[BARBLOCKPEN];
  menubar=XCreateWindow(dpy, root, 0, 0, rootwidth, bh, 0, CopyFromParent,
			InputOutput, CopyFromParent,
			CWOverrideRedirect|CWBackPixel,
			&attr);
  menubarparent=XCreateWindow(dpy, menubar, 0, 0, rootwidth, bh-1, 0,
			      CopyFromParent, InputOutput, CopyFromParent,
			      CWOverrideRedirect|CWBackPixel, &attr);
  attr.background_pixel=dri.dri_Pens[BACKGROUNDPEN];
  menubardepth=XCreateWindow(dpy, menubar, rootwidth-23, 0, 23, bh, 0,
			     CopyFromParent, InputOutput, CopyFromParent,
			     CWOverrideRedirect|CWBackPixel, &attr);
  if(!disabled_stipple) {
    GC gc;
    disabled_stipple=XCreatePixmap(dpy, root, 6, 2, 1);
    gc=XCreateGC(dpy, disabled_stipple, 0, NULL);
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, disabled_stipple, gc, 0, 0, 6, 2);
    XSetForeground(dpy, gc, 1);
    XDrawPoint(dpy, disabled_stipple, gc, 0, 0);
    XDrawPoint(dpy, disabled_stipple, gc, 3, 1);
    XFreeGC(dpy, gc);
  }
  if(!menubargc)
    menubargc=XCreateGC(dpy, menubar, 0, NULL); 
  XSetFont(dpy, menubargc, dri.dri_Font->fid);
  XSetBackground(dpy, menubargc, dri.dri_Pens[BARBLOCKPEN]);
  XSelectInput(dpy, menubar, ExposureMask);
  XSelectInput(dpy, menubardepth, ExposureMask);
  XMapWindow(dpy, menubardepth);
  XMapWindow(dpy, menubar);
  hotkeyspace=8+1+dri.dri_Font->max_bounds.width+dri.dri_Font->ascent;
  checkmarkspace=4+dri.dri_Font->ascent;
  menuleft=4;
  m=add_menu("Workbench", 0);
  add_item(m,"Backdrop",'B',CHECKIT|CHECKED|DISABLED);
  add_item(m,"Execute Command...",'E',0);
  add_item(m,"Redraw All",0,DISABLED);
  add_item(m,"Update All",0,DISABLED);
  add_item(m,"Last Message",0,DISABLED);
  add_item(m,"About...",'?',0);
  add_item(m,"Quit...",'Q',0);
  menu_layout(m);
  m=add_menu("Window", 0);
  add_item(m,"New Drawer",'N',DISABLED);
  add_item(m,"Open Parent",0,DISABLED);
  add_item(m,"Close",'K',DISABLED);
  add_item(m,"Update",0,DISABLED);
  add_item(m,"Select Contents",'A',DISABLED);
  add_item(m,"Clean Up",'.',0);
  sm1=sub_menu(add_item(m,"Snapshot",0,DISABLED),0);
  add_item(sm1, "Window",0,DISABLED);
  add_item(sm1, "All",0,DISABLED);
  sm2=sub_menu(add_item(m,"Show",0,DISABLED),0);
  add_item(sm2, "Only Icons",0,CHECKIT|CHECKED|DISABLED);
  add_item(sm2, "All Files",'V',CHECKIT|DISABLED);
  sm3=sub_menu(add_item(m,"View By",0,DISABLED),0);
  add_item(sm3, "Icon",0,CHECKIT|CHECKED|DISABLED);
  add_item(sm3, "Name",0,CHECKIT|DISABLED);
  add_item(sm3, "Date",0,CHECKIT|DISABLED);
  add_item(sm3, "Size",0,CHECKIT|DISABLED);
  menu_layout(m);
  menu_layout(sm1);
  menu_layout(sm2);
  menu_layout(sm3);
  m=add_menu("Icons", DISABLED);
  add_item(m,"Open",'O',DISABLED);
  add_item(m,"Copy",'C',DISABLED);
  add_item(m,"Rename...",'R',DISABLED);
  add_item(m,"Information...",'I',DISABLED);
  add_item(m,"Snapshot",'S',DISABLED);
  add_item(m,"UnSnapshot",'U',DISABLED);
  add_item(m,"Leave Out",'L',DISABLED);
  add_item(m,"Put Away",'P',DISABLED);
  add_item(m,NULL,0,DISABLED);
  add_item(m,"Delete...",'D',DISABLED);
  add_item(m,"Format Disk...",0,DISABLED);
  add_item(m,"Empty Trash",0,DISABLED);
  menu_layout(m);
  m=add_menu("Tools", 0);
#ifdef AMIGAOS
  add_item(m,"ResetWB",0,DISABLED);
#else
  add_item(m,"ResetWB",0,0);
#endif
  for(ti=firsttoolitem; ti; ti=ti->next)
    add_item(m, ti->name,0,0);
  menu_layout(m);
}

void redrawmenubar(Window w)
{
  struct Menu *m;
  struct Item *item;

  if(!w)
    return;
  if(w==menubar) {
    XSetForeground(dpy, menubargc, dri.dri_Pens[BARDETAILPEN]);
    XSetBackground(dpy, menubargc, dri.dri_Pens[BARBLOCKPEN]);
    XDrawImageString(dpy, w, menubargc, 4, 1+dri.dri_Font->ascent,
		     "Workbench Screen", 16);
    XSetForeground(dpy, menubargc, dri.dri_Pens[BARTRIMPEN]);  
    XDrawLine(dpy, w, menubargc, 0, bh-1, rootwidth-1, bh-1);
  } else if(w==menubardepth) {
    XSetForeground(dpy, menubargc, dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, menubargc, 4, h2, 10, h6-h2);
    XSetForeground(dpy, menubargc, dri.dri_Pens[SHINEPEN]);
    XFillRectangle(dpy, w, menubargc, 8, h4, 10, h8-h4);
    XSetForeground(dpy, menubargc, dri.dri_Pens[SHADOWPEN]);
    XDrawRectangle(dpy, w, menubargc, 8, h4, 10, h8-h4);
    XSetForeground(dpy, menubargc, dri.dri_Pens[SHINEPEN]);
    XDrawLine(dpy, w, menubargc, 0, 0, 22, 0);
    XDrawLine(dpy, w, menubargc, 0, 0, 0, bh-2);
    XSetForeground(dpy, menubargc, dri.dri_Pens[SHADOWPEN]);
    XDrawLine(dpy, w, menubargc, 0, bh-1, 22, bh-1);
    XDrawLine(dpy, w, menubargc, 22, 0, 22, bh-1);
  } else {
    for(m=firstmenu; m; m=m->next)
      if(m->win==w)
	redraw_menu(m, w);
    if(activemenu) {
      for(item=activemenu->firstitem; item; item=item->next)
	if(item->win==w)
	  redraw_item(item, w);
      if(w==activemenu->parent) {
	XSetForeground(dpy, menubargc, dri.dri_Pens[BARDETAILPEN]);
	XDrawLine(dpy, w, menubargc, 0, 0, 0, activemenu->height-1);
	XDrawLine(dpy, w, menubargc, activemenu->width-1, 0,
		  activemenu->width-1, activemenu->height-1);
      }
    }
    if(activesubmenu) {
      for(item=activesubmenu->firstitem; item; item=item->next)
	if(item->win==w)
	  redraw_item(item, w);
      if(w==activesubmenu->parent) {
	XSetForeground(dpy, menubargc, dri.dri_Pens[BARDETAILPEN]);
	XDrawLine(dpy, w, menubargc, 0, 0, 0, activesubmenu->height-1);
	XDrawLine(dpy, w, menubargc, activesubmenu->width-1, 0,
		  activesubmenu->width-1, activesubmenu->height-1);
      }      
    }
  }
}

static void leave_item(struct Item *i, Window w)
{
  if(i==activesubitem)
    activesubitem=NULL;
  if(i==activeitem)
    if(activesubmenu && i->sub==activesubmenu)
      return;
    else
      activeitem=NULL;
  XSetWindowBackground(dpy, i->win, dri.dri_Pens[BARBLOCKPEN]);
  XClearWindow(dpy, i->win);
  redraw_item(i, i->win);    
}

static void enter_item(struct Item *i, Window w)
{
  if(activesubitem)
    leave_item(activesubitem, activesubitem->win);
  if(activeitem && !(activeitem->sub && i->menu==activeitem->sub))
    leave_item(activeitem, activeitem->win);
  if(activesubmenu!=i->sub && i->menu!=activesubmenu) {
    if(activesubmenu)
      XUnmapWindow(dpy, activesubmenu->parent);
    if(i->sub)
      XMapRaised(dpy, i->sub->parent);
    activesubmenu=i->sub;
  }
  if(!(i->flags&DISABLED)) {
    if(activeitem)
      activesubitem=i;
    else
      activeitem=i;
    XSetWindowBackground(dpy, i->win, dri.dri_Pens[BARDETAILPEN]);
    XClearWindow(dpy, i->win);
    redraw_item(i, i->win);    
  }
}

static void enter_menu(struct Menu *m, Window w)
{
  if(m!=activemenu) {
    struct Menu *oa=activemenu;
    if(activesubitem)
      leave_item(activeitem, activesubitem->win);
    if(activesubmenu) {
      XUnmapWindow(dpy, activesubmenu->parent);
      activesubmenu=NULL;
    }
    if(activeitem)
      leave_item(activeitem, activeitem->win);
    if(!(m->flags & DISABLED))
      XSetWindowBackground(dpy, w, dri.dri_Pens[BARDETAILPEN]);
    XClearWindow(dpy, w);
    redraw_menu(activemenu=m, w);
    if(m->parent)
      XMapRaised(dpy, m->parent);
    if(oa) {
      if(oa->parent)
	XUnmapWindow(dpy, oa->parent);
      XSetWindowBackground(dpy, oa->win, dri.dri_Pens[BARBLOCKPEN]);
      XClearWindow(dpy, oa->win);
      redraw_menu(oa, oa->win);
    }
  }
}

void menubar_enter(Window w)
{
  struct Menu *m;
  struct Item *i;

  for(m=firstmenu; m; m=m->next)
    if(m->win==w) {
      enter_menu(m, w);
      return;
    }
  if(m=activemenu)
    for(i=m->firstitem; i; i=i->next)
      if(w==i->win) {
	enter_item(i, w);
	return;
      }
  if(m=activesubmenu)
    for(i=m->firstitem; i; i=i->next)
      if(w==i->win) {
	enter_item(i, w);
	return;
      }
}

void menubar_leave(Window w)
{
  if(activeitem && activeitem->win==w)
    leave_item(activeitem, w);
}

void menu_on()
{
  Window r, c;
  int rx, ry, x, y;
  unsigned int m;

  if(menubarparent) {
    XMapRaised(dpy, menubarparent);
    XRaiseWindow(dpy, menubar);
    XGrabPointer(dpy, root, True, ButtonPressMask|ButtonReleaseMask|
		EnterWindowMask|LeaveWindowMask, GrabModeAsync, GrabModeAsync,
		None, wm_curs, CurrentTime);
    XSetInputFocus(dpy, menubar, RevertToParent, CurrentTime);
    if(XQueryPointer(dpy, menubarparent, &r, &c, &rx, &ry, &x, &y, &m))
      menubar_enter(c);
  }
}

void menuaction(struct Item *i, struct Item *si)
{
  extern void restart_amiwm(void);
  struct Menu *m;
  struct Item *mi;
  struct ToolItem *ti;
  int menu=0, item=0, sub=0;

  for(m=i->menu->next; m; m=m->next) menu++;
  for(mi=i->menu->firstitem; mi&&mi!=i; mi=mi->next) item++;
  if(i->sub)
    for(mi=i->sub->firstitem; mi&&mi!=si; mi=mi->next) sub++;
  else
    --sub;
  switch(menu) {
  case 0: /* Workbench */
    switch(item) {
    case 1:
      spawn(BIN_PREFIX"executecmd");
      break;
    case 5:
#ifdef AMIGAOS
      spawn(BIN_PREFIX"requestchoice >NIL: amiwm \""
	     "version "VERSION"*Nby Marcus Comstedt*N"
	     "<marcus@lysator.liu.se>"
	     "\" Ok");
#else
      spawn(BIN_PREFIX"requestchoice >/dev/null amiwm '"
	     "version "VERSION"\nby Marcus Comstedt\n"
	     "<marcus@lysator.liu.se>"
	     "' Ok");
#endif
      break;      
    case 6:
#ifndef AMIGAOS
      if(prefs.fastquit) {
#endif
	flushclients();
	XFlush(dpy);
	XCloseDisplay(dpy);
	exit(0);
#ifndef AMIGAOS
      } else {
#ifdef HAVE_ALLOCA
	char *buf=alloca(256);
#else
	char buf[256];
#endif
	sprintf(buf, "( if [ `"BIN_PREFIX"requestchoice Workbench '"
		"Do you really want\nto quit workbench?' Ok Cancel` = 1 ]; "
		"then kill %d; fi; )", getpid());
	spawn(buf);
      }
#endif
      break;
    }
    break;
  case 1: /* Window */
    switch(item) {
    case 5:
      cleanupicons();
      break;
    }
    break;
  case 2: /* Icons */
    break;
  case 3: /* Tools */
#ifndef AMIGAOS
    if(item==0)
      restart_amiwm();
#endif
    if(item>0) {
      for(ti=firsttoolitem; ti && --item; ti=ti->next);
      if(ti) spawn(ti->cmd);
    }
    break;
  }
}

void menu_off()
{
  struct Menu *oa;
  struct Item *oi, *osi;

  if(menubarparent) {
    Window r,p,*children;
    unsigned int nchildren;
    XUngrabPointer(dpy, CurrentTime);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XUnmapWindow(dpy, menubarparent);
    if(XQueryTree(dpy, root, &r, &p, &children, &nchildren)) {
      int n;
      Client *c2;
      for(n=0; n<nchildren; n++)
	if((c2=getclient(children[n])) && children[n]==c2->parent)
	  break;
      if(n<nchildren) {
	Window ws[2];
	ws[0]=children[n];
	ws[1]=menubar;
	XRestackWindows(dpy, ws, 2);
      }
      if(children) XFree(children);
    }
  }
  if(osi=activesubitem)
    leave_item(osi, osi->win);
  if(oi=activeitem)
    leave_item(oi, oi->win);
  if(oa=activesubmenu) {
    activesubmenu=NULL;
    if(oa->parent)
      XUnmapWindow(dpy, oa->parent);
  }
  if(oa=activemenu) {
    activemenu=NULL;
    if(oa->parent)
      XUnmapWindow(dpy, oa->parent);
    XSetWindowBackground(dpy, oa->win, dri.dri_Pens[BARBLOCKPEN]);
    XClearWindow(dpy, oa->win);
    redraw_menu(oa, oa->win);
  }
  if(oi)
    menuaction(oi, osi);
}

struct Item *getitembyhotkey(char key)
{
  struct Menu *m;
  struct Item *i;

  if(key) {
    if(key>='a' && key<='z')
      key-=0x20;
    for(m=firstmenu; m; m=m->next)
      for(i=m->firstitem; i; i=i->next)
	if(i->hotkey==key)
	  return i;
  }
  return NULL;
}
