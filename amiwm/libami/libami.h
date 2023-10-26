#include <X11/Xlib.h>

extern char *amiwm_version;

extern int md_fd;
extern Window md_root;

#define WINDOW_EVENT(e) ((e)->xany.display==(Display *)1)
#define FRAME_EVENT(e) ((e)->xany.display==(Display *)2)
#define ICON_EVENT(e) ((e)->xany.display==(Display *)3)

#define IN_ROOT_MASK 1
#define IN_WINDOW_MASK 2
#define IN_FRAME_MASK 4
#define IN_ICON_MASK 8
#define IN_ANYTHING_MASK (~0)

/* module.c */
extern void md_exit(int);
extern int md_handle_input(void);
extern void md_process_queued(void);
extern void md_main_loop(void);
extern int md_command(XID, int, void *, int, char **);
extern int md_command0(XID, int, void *, int);
extern int md_command00(XID, int);
extern Display *md_display(void);
extern char *md_init(int, char *[]);

/* broker.c */
extern int cx_broker(unsigned long, void (*)(XEvent *, unsigned long));
extern int cx_send_event(unsigned long, XEvent *);

/* mdscreen.c */
extern int md_rotate_screen(Window);
extern int md_front(Window);
extern int md_back(Window);
extern int md_iconify(Window);

/* eventdispatcher.c */
extern void cx_event_broker(int, unsigned long, int (*)(XEvent*));

/* kbdsupport.c */
extern int md_grabkey(int, unsigned int);
extern int md_ungrabkey(int);

/* hotkey.c */
extern void cx_hotkey(KeySym, unsigned int, int, int,
		      void (*)(XEvent*,void*), void*);
