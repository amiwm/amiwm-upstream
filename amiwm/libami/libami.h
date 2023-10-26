#include <X11/Xlib.h>

#ifdef AMIGAOS
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#else

#define GLOBAL	extern
#define IMPORT	extern
#define STATIC	static
#define REGISTER register

#ifndef VOID
#define VOID	void
#endif

#ifndef BYTE
#define BYTE Amiga_BYTE
#endif
#ifndef BOOL
#define BOOL Amiga_BOOL
#endif

typedef void    *APTR;
typedef long	LONG;
typedef unsigned long ULONG;
typedef unsigned long LONGBITS;
typedef short	WORD;
typedef unsigned short UWORD;
typedef unsigned short WORDBITS;
#if __STDC__
typedef signed char BYTE;
#else
typedef char BYTE;
#endif
typedef unsigned char UBYTE;
typedef unsigned char BYTEBITS;
typedef unsigned short RPTR;
typedef unsigned char  *STRPTR;
typedef short BOOL;

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif
#ifndef NULL
#define NULL	0L
#endif


struct Node {
  struct  Node *ln_Succ;
  struct  Node *ln_Pred;
  UBYTE   ln_Type;
  BYTE    ln_Pri;
  char    *ln_Name;
};

struct MinNode {
  struct MinNode *mln_Succ;
  struct MinNode *mln_Pred;
};

struct List {
  struct  Node *lh_Head;
  struct  Node *lh_Tail;
  struct  Node *lh_TailPred;
  UBYTE   lh_Type;
  UBYTE   l_pad;
};

struct MinList {
  struct  MinNode *mlh_Head;
  struct  MinNode *mlh_Tail;
  struct  MinNode *mlh_TailPred;
};

extern void AddHead(struct List *, struct Node *);
extern void AddTail(struct List *, struct Node *);
extern void Enqueue(struct List *, struct Node *);
extern struct Node *FindName(struct List *, STRPTR);
extern void Insert(struct List *, struct Node *, struct Node *);
extern struct Node *RemHead(struct List *);
extern void Remove(struct Node *);
extern struct Node *RemTail(struct List *);
extern void NewList(struct List *);

struct CSource {
  UBYTE	*CS_Buffer;
  LONG	CS_Length;
  LONG	CS_CurChr;
};

struct RDArgs {
  struct CSource RDA_Source;
  void *RDA_DAList;
  UBYTE *RDA_Buffer;
  LONG RDA_BufSiz;
  UBYTE *RDA_ExtHelp;
  LONG RDA_Flags;
};

#define RDAB_STDIN	0	/* Use "STDIN" rather than "COMMAND LINE" */
#define RDAF_STDIN	1
#define RDAB_NOALLOC	1	/* If set, do not allocate extra string space.*/
#define RDAF_NOALLOC	2
#define RDAB_NOPROMPT	2	/* Disable reprompting for string input. */
#define RDAF_NOPROMPT	4

#define ITEM_EQUAL	-2	/* "=" Symbol */
#define ITEM_ERROR	-1	/* error */
#define ITEM_NOTHING	0	/* *N, ;, endstreamch */
#define ITEM_UNQUOTED	1	/* unquoted item */
#define ITEM_QUOTED	2	/* quoted item */

#define MAX_TEMPLATE_ITEMS 100
#define MAX_MULTIARGS 128

#define ERROR_NO_FREE_STORE  103
#define ERROR_BAD_TEMPLATE  114
#define ERROR_BAD_NUMBER  115
#define ERROR_REQUIRED_ARG_MISSING  116
#define ERROR_KEY_NEEDS_ARG  117
#define ERROR_TOO_MANY_ARGS  118
#define ERROR_LINE_TOO_LONG  120


extern void FreeArgs(struct RDArgs *);
extern LONG ReadItem(STRPTR, LONG, struct CSource *);
extern LONG FindArg(STRPTR, STRPTR);
extern struct RDArgs * ReadArgs(STRPTR, LONG *, struct RDArgs *);

extern BOOL Fault(LONG, UBYTE *, UBYTE *, LONG);
extern BOOL PrintFault(LONG, UBYTE *);
extern LONG IoErr();
extern LONG SetIoErr(LONG);

#endif

typedef union { LONG num; APTR ptr; } Argtype;

extern void initargs(int, char **);

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
