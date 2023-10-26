#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <X11/Xlib.h>

#include "drawinfo.h"
#include "screen.h"
#include "prefs.h"
#include "module.h"
#include "client.h"
#include "icon.h"
#include "version.h"

extern XContext client_context, icon_context;

extern FILE *rcfile;
extern Display *dpy;

extern void add_fd_to_set(int);
extern void remove_fd_from_set(int);

struct module *modules = NULL;

struct mcmd_keygrab *keygrabs = NULL;

static struct mcmd_keygrab *find_keygrab(int keycode, unsigned int modifiers)
{
  struct mcmd_keygrab *kg=keygrabs, *last=NULL;

  while(kg)
    if(kg->keycode == keycode && kg->modifiers == modifiers)
      return kg;
    else kg=kg->next;
  return NULL;
}

int create_keygrab(struct module *m, int keycode, unsigned int modifiers)
{
  static int id=1;
  struct mcmd_keygrab *kg=malloc(sizeof(struct mcmd_keygrab));
  Client *c;

  if(kg) {
    kg->next = keygrabs;
    kg->id=id++;
    kg->owner=m;
    kg->keycode=keycode;
    kg->modifiers=modifiers;
    keygrabs=kg;
    for(c=clients; c; c=c->next)
      if(c->parent && c->parent!=c->scr->root)
	XGrabKey(dpy, keycode, modifiers, c->window, False,
		 GrabModeAsync, GrabModeAsync);
    return kg->id;
  } else return -1;
}

int delete_keygrab(struct module *m, int id)
{
  struct mcmd_keygrab *kg=keygrabs, *last=NULL;
  Client *c;

  while(kg) {
    if(kg->owner==m && (id<0 || kg->id==id)) {
      if(last)
	last->next=kg->next;
      else
	keygrabs=kg->next;
      if(!find_keygrab(kg->keycode, kg->modifiers))
	for(c=clients; c; c=c->next)
	  if(c->parent && c->parent!=c->scr->root)
	    XUngrabKey(dpy, kg->keycode, kg->modifiers, c->window);
    } else last=kg;
    kg=kg->next;
  }
}

static void destroy_module(struct module *m)
{
  delete_keygrab(m, -1);
  if(m->in_fd>=0) { remove_fd_from_set(m->in_fd); close(m->in_fd); }
  if(m->out_fd>=0) { close(m->out_fd); }
  free(m);
}

static void sieve_modules()
{
  struct module *m, **p;
  for(p=&modules; m=*p; )
    if(!m->pid) {
      *p=m->next;
      destroy_module(m);
    } else p=&(m->next);
}

void reap_children(int sig)
{
  pid_t pid;
  int stat;
#ifdef HAVE_WAITPID
  while((pid=waitpid(-1, &stat, WNOHANG))>0)
#else
#ifdef HAVE_WAIT3
  while((pid=wait3(&stat, WNOHANG, NULL))>0)
#else
  if((pid=wait(&stat))>0)
#endif
#endif
#ifdef WIFSTOPPED
    if(!WIFSTOPPED(stat)) {
#else
    {
#endif
      struct module *m;
      for(m=modules; m; m=m->next)
	if(m->pid==pid) {
	  m->pid=0;
	  break;
	}
    }
  if(pid<0 && errno!=ECHILD && errno!=EINTR)
    perror("wait");
  if(sig>0)
    signal(sig, reap_children);
}

void init_modules()
{
  modules = NULL;
#ifdef SIGCHLD
  signal(SIGCHLD, reap_children);
#else
#ifdef SIGCLD
  signal(SIGCLD, reap_children);
#endif
#endif
}

void create_module(Scrn *screen, char *module_name, char *module_arg)
{
  pid_t pid;
  int fds1[2], fds2[2];
  char fd1num[16], fd2num[16], scrnum[16], destpath[1024], *pathelt;
  struct module *m;

#ifdef HAVE_WAITPID
  reap_children(0);
#else
#ifdef HAVE_WAIT3
  reap_children(0);
#endif
#endif
  sieve_modules();
  for(pathelt=strtok(prefs.module_path, ":"); pathelt;
      pathelt=strtok(NULL, ":")) {
    sprintf(destpath, "%s/%s", pathelt, module_name);
    if(access(destpath, X_OK)>=0)
      break;
  }
  if(!pathelt) {
    fprintf(stderr, "%s: no such module\n", module_name);
    return;
  }

  if(pipe(fds1)>=0) {
    if(pipe(fds2)>=0) {
      if((pid=fork())) {
	close(fds1[0]);
	close(fds2[1]);
	if(pid<0)
	  perror("fork");
	else {
	  m=calloc(sizeof(struct module),1);
	  m->pid=pid;
	  m->in_fd=fds2[0];
	  m->out_fd=fds1[1];
	  m->in_buf=malloc(m->in_buf_size=64);
	  m->in_phase=0;
	  m->in_ptr=(char *)&m->mcmd;
	  m->in_left=sizeof(m->mcmd);
	  m->next=modules;
	  modules=m;
	  add_fd_to_set(m->in_fd);
	}
      } else {
	if(rcfile)
	  close(fileno(rcfile));
	for(m=modules; m; m=m->next) {
	  if(m->in_fd>=0) close(m->in_fd);
	  if(m->out_fd>=0) close(m->out_fd);
	}
	close(fds1[1]);
	close(fds2[0]);
	sprintf(fd1num, "%d", fds1[0]);
	sprintf(fd2num, "%d", fds2[1]);
	sprintf(scrnum, "0x%08lx", (screen? (screen->back):None));
	execl(destpath, module_name, fd1num, fd2num, scrnum, module_arg, NULL);
	perror(destpath);
	_exit(1);
      }
    } else {
      perror("pipe");
      close(fds1[0]);
      close(fds1[1]);
    }
  } else perror("pipe");
}

static int m_write(int fd, char *ptr, int len)
{
  char *p=ptr;
  int r, tot=0;
  while(len>0) {
    if((r=write(fd, p, len))<0)
      if(errno==EINTR)
	continue;
      else
	return r;
    if(!r)
      return tot;
    tot+=r;
    p+=r;
    len-=r;
  }
  return tot;  
}

static void reply_module(struct module *m, char *data, int len)
{
  m_write(m->out_fd, (char *)&len, sizeof(len));
  if(len<0) len=~len;
  if(len>0)
    m_write(m->out_fd, data, len);
}

int dispatch_event_to_broker(XEvent *e, unsigned long mask, struct module *m)
{
  Client *c;
  Icon *i;
  e->xany.display=(Display *)0;
  if(!XFindContext(dpy, e->xany.window, client_context, (XPointer *)&c))
    if(e->xany.window==c->window)
      e->xany.display=(Display *)1;
    else
      e->xany.display=(Display *)2;
  else if(!XFindContext(dpy, e->xany.window, icon_context, (XPointer *)&i))
    if(e->xany.window==i->window)
      e->xany.display=(Display *)3;

  while(m) {
    if(m->out_fd>=0 && ((m->broker.mask & mask)||(m->broker.exists && !mask))) {
      struct mcmd_event me;
      me.mask=mask;
      me.event=*e;
      reply_module(m, (char *)&me, ~sizeof(me));
      return 1;
    }
    m=m->next;
  }
  return 0;
}

static int incoming_event(struct module *m, struct mcmd_event *me)
{
  extern void internal_broker(XEvent *);

  if(!dispatch_event_to_broker(&me->event, me->mask, m->next))
    internal_broker(&me->event);
}

static void handle_module_cmd(struct module *m, char *data, int data_len)
{
  extern Scrn *getscreen(Window);
  XID id=m->mcmd.id;
  Client *c;

  switch(m->mcmd.cmd) {
  case MCMD_NOP:
    reply_module(m, NULL, 0);
    break;
  case MCMD_GET_VERSION:
    reply_module(m, "amiwm "VERSION, sizeof("amiwm "VERSION));
    break;
  case MCMD_SEND_EVENT:
    if(data_len>=sizeof(struct mcmd_event)) {
      incoming_event(m, (struct mcmd_event *)data);
      reply_module(m, NULL, 0);
    } else 
      reply_module(m, NULL, -1);
    break;
  case MCMD_SET_BROKER:
    if(data_len>=sizeof(m->broker.mask)) {
      m->broker.mask=*(unsigned long *)data;
      m->broker.exists=1;
      reply_module(m, NULL, 0);
    } else
      reply_module(m, NULL, -1);
    break;
  case MCMD_ROTATE_SCREEN:
    scr=getscreen(id);
    screentoback();
    reply_module(m, NULL, 0);
    break;
  case MCMD_ADD_KEYGRAB:
    if(data_len>=sizeof(int[2])) {
      int res=create_keygrab(m, ((int*)data)[0], ((int*)data)[1]);
      reply_module(m, (char *)&res, sizeof(res));
    } else reply_module(m, NULL, -1);
    break;
  case MCMD_DEL_KEYGRAB:
    if(data_len>=sizeof(int)) {
      delete_keygrab(m, ((int*)data)[0]);
      reply_module(m, NULL, 0);
    } else reply_module(m, NULL, -1);
    break;
  case MCMD_FRONT:
    if(!XFindContext(dpy, id, client_context, (XPointer*)&c)) {
      raiselowerclient(c, PlaceOnTop);
      reply_module(m, NULL, 0);
    } else
      reply_module(m, NULL, -1);
    break;
  case MCMD_BACK:
    if(!XFindContext(dpy, id, client_context, (XPointer*)&c)) {
      raiselowerclient(c, PlaceOnBottom);
      reply_module(m, NULL, 0);
    } else
      reply_module(m, NULL, -1);
    break;
  case MCMD_ICONIFY:
    if(!XFindContext(dpy, id, client_context, (XPointer*)&c)) {
      if(!(c->icon))
	createicon(c);
      XUnmapWindow(dpy, c->parent);
      XUnmapWindow(dpy, c->window);
      adjusticon(c->icon);
      XMapWindow(dpy, c->icon->window);
      XMapWindow(dpy, c->icon->labelwin);
      c->icon->mapped=1;
      setclientstate(c, IconicState);
      reply_module(m, NULL, 0);
    } else
      reply_module(m, NULL, -1);
    break;
  default:
    reply_module(m, NULL, -1);
  }
}

static void module_input_callback(struct module *m)
{
  unsigned char buffer[8192], *p=buffer;
  int r;
  if(!(m->pid)) {
    sieve_modules();
    return;
  }
  r=read(m->in_fd, buffer, sizeof(buffer));
  if(r<0 && errno!=EINTR) {
    perror("module");
    close(m->in_fd);
    close(m->out_fd);
    m->in_fd=m->out_fd=-1;
#ifdef HAVE_WAITPID
    reap_children(0);
#else
#ifdef HAVE_WAIT3
    reap_children(0);
#endif
#endif
    sieve_modules();
    return;
  }
  while(r>0) {
    int t=(r>m->in_left? m->in_left:r);
    memcpy(m->in_ptr, p, t);
    m->in_ptr+=t;
    if(!(m->in_left-=t))
      if(m->in_phase || ((!m->mcmd.len)&&(m->in_ptr=m->in_buf))) {
	*m->in_ptr=0;
	handle_module_cmd(m, m->in_buf, m->mcmd.len);
	m->in_ptr=(char *)&m->mcmd;
	m->in_left=sizeof(m->mcmd);
	m->in_phase=0;
      } else {
	if(m->mcmd.len>=m->in_buf_size) {
	  if((m->in_buf_size<<=1)<=m->mcmd.len)
	    m->in_buf_size=m->mcmd.len+1; {
	  m->in_buf=realloc(m->in_buf, m->in_buf_size);
	  }
	}
	m->in_ptr=m->in_buf;
	m->in_left=m->mcmd.len;
	m->in_phase++;
      }
    p+=t;
    r-=t;
  }
}

void handle_module_input(fd_set *r)
{
  struct module *m;

  for(m=modules; m; m=m->next)
    if(m->in_fd>=0 && FD_ISSET(m->in_fd, r)) {
      module_input_callback(m);
      break;
    }
}

void flushmodules()
{
  struct module *n, *m=modules;
  int i;
  extern char *progname;

  while(m) {
    n=m->next;
    if(m->in_fd>=0) {
      close(m->in_fd);
      remove_fd_from_set(m->in_fd);
      m->in_fd=-1;
    }
    if(m->out_fd>=0) {
      close(m->out_fd);
      m->out_fd=-1;
    }
    if(m->pid>0)
      kill(m->pid, SIGHUP);
    m=n;
  }
  for(i=5; i>0; --i) {
    sieve_modules();
    if(!modules)
      break;
    sleep(1);
  }
  sieve_modules();
  if(modules)
    fprintf(stderr, "%s: giving up waiting for modules to die!\n", progname);
}
