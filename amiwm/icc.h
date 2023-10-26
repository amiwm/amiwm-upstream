#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "client.h"

extern void init_atoms(void);
extern void sendcmessage(Window, Atom, long);
extern void getproto(Client *c);

extern Atom wm_state, wm_change_state, wm_protocols, wm_delete, wm_take_focus, wm_colormaps;

#define Pdelete 1
#define Ptakefocus 2
#define Psizebottom 4
#define Psizeright 8
#define Psizetrans 16

