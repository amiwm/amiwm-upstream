#include "libami.h"
#include "module.h"

int md_rotate_screen(XID id)
{
  return md_command00(id, MCMD_ROTATE_SCREEN);
}

int md_front(XID id)
{
  return md_command00(id, MCMD_FRONT);
}

int md_back(XID id)
{
  return md_command00(id, MCMD_BACK);
}

int md_iconify(XID id)
{
  return md_command00(id, MCMD_ICONIFY);
}


