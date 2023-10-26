#define MCMD_NOP 1
#define MCMD_GET_VERSION 2
#define MCMD_LIST_SCREENS 3
#define MCMD_SEND_EVENT 4
#define MCMD_SET_BROKER 5
#define MCMD_ROTATE_SCREEN 6
#define MCMD_ADD_KEYGRAB 7
#define MCMD_DEL_KEYGRAB 8
#define MCMD_FRONT 9
#define MCMD_BACK 10
#define MCMD_ICONIFY 11

struct mcmd_header {
  XID id;
  int cmd;
  int len;
};

struct mcmd_event {
  unsigned long mask;
  XEvent event;
};

struct mcmd_keygrab {
  struct mcmd_keygrab *next;
  int id;
  struct module *owner;
  int keycode;
  unsigned int modifiers;
};

extern struct module {
  struct module *next;
  int in_fd, out_fd;
  pid_t pid;
  struct mcmd_header mcmd;
  char *in_ptr;
  int in_left;
  int in_phase;
  char *in_buf;
  int in_buf_size;
  struct {
    int exists;
    unsigned long mask;
  } broker;
} *modules;
