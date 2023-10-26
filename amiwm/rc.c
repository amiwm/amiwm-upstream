#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
#include "prefs.h"
#include "drawinfo.h"
#include "screen.h"
#include "gram.h"
#include "icc.h"

#ifdef AMIGAOS
#include <pragmas/xmu1_pragmas.h>
extern struct Library *Xmu1Base;
#endif

struct prefs_struct prefs;

#ifndef RC_FILENAME
#define RC_FILENAME ".amiwmrc"
#endif

FILE *rcfile;
int ParseError=0;

void read_rc_file()
{
  char *home, *fn;

  memset(prefs, 0, sizeof(prefs));
  prefs.sizeborder=Psizeright;
  prefs.icondir=AMIWM_HOME;
  prefs.module_path=AMIWM_HOME;
  prefs.defaulticon="def_tool.info";
  prefs.borderwidth=1;
  set_sys_palette();
  home=getenv("HOME");
#ifdef AMIGAOS
  {
    char fn[256];
    strncpy(fn, home, sizeof(fn)-1);
    fn[sizeof(fn)-1]='\0';
    AddPart(fn, RC_FILENAME, sizeof(fn));
    if(rcfile=fopen(fn, "r")) {
      yyparse();
      fclose(rcfile);
      rcfile=NULL;
      return;
    }
  }
#else
#ifdef HAVE_ALLOCA
  if(fn=alloca(strlen(home)+strlen(RC_FILENAME)+4)) {
#else
  if(fn=malloc(strlen(home)+strlen(RC_FILENAME)+4)) {
#endif
    sprintf(fn, "%s/"RC_FILENAME, home);
    if(rcfile=fopen(fn, "r")) {
      yyparse();
      fclose(rcfile);
#ifndef HAVE_ALLOCA
      free(fn);
#endif
      return;
    }
#ifndef HAVE_ALLOCA
    free(fn);
#endif
  }
#endif
  if(rcfile=fopen(AMIWM_HOME"/system"RC_FILENAME, "r")) {
    yyparse();
    fclose(rcfile);
  }
}

struct keyword { char *name; int token; } keywords[] = {
  { "always", ALWAYS },
  { "auto", AUTO },
  { "autoraise", AUTORAISE },
  { "backgroundpen", T_BACKGROUNDPEN },
  { "barblockpen", T_BARBLOCKPEN },
  { "bardetailpen", T_BARDETAILPEN },
  { "bartrimpen", T_BARTRIMPEN },
  { "blockpen", T_BLOCKPEN },
  { "both", BOTH },
  { "bottom", BOTTOM },
  { "defaulticon", DEFAULTICON },
  { "detailpen", T_DETAILPEN },
  { "false", NO },
  { "fastquit", FASTQUIT },
  { "fillpen", T_FILLPEN },
  { "filltextpen", T_FILLTEXTPEN },
  { "forcemove", FORCEMOVE },
  { "highlighttextpen", T_HIGHLIGHTTEXTPEN },
  { "icondir", ICONDIR },
  { "iconfont", ICONFONT },
  { "iconpalette", ICONPALETTE },
  { "interscreengap", INTERSCREENGAP },
  { "magicwb", MAGICWB },
  { "manual", MANUAL },
  { "module", MODULE },
  { "modulepath", MODULEPATH },
  { "no", NO },
  { "none", NONE },
  { "off", NO },
  { "on", YES },
  { "right", RIGHT },
  { "screen", SCREEN },
  { "screenfont", SCREENFONT },
  { "separator", SEPARATOR },
  { "shadowpen", T_SHADOWPEN },
  { "shinepen", T_SHINEPEN },
  { "sizeborder", SIZEBORDER },
  { "system", SYSTEM },
  { "textpen", T_TEXTPEN },
  { "toolitem", TOOLITEM },
  { "true", YES },
  { "yes", YES }
};

#define N_KW (sizeof(keywords)/sizeof(keywords[0]))

int parse_keyword(char *str)
{
  int l=0, h=N_KW-1;
  
  XmuCopyISOLatin1Lowered (str, str);
  while(h>=l) {
    int i=(h+l)>>1, c=strcmp(str, keywords[i].name);
    if(!c)
      return keywords[i].token;
    else if(c>=0)
      l=i+1;
    else
      h=i-1;
  }
  return ERRORTOKEN;
}
