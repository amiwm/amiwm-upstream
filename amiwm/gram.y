%{
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include "prefs.h"
#include "drawinfo.h"
#include "screen.h"
#include "icc.h"
extern set_sys_palette(void);
extern set_mwb_palette(void);
extern void add_toolitem(char *, char *, int);
extern char *default_colors[NUMDRIPENS];
extern char *default_screenfont, *label_font_name;
extern Display *dpy;
#ifndef HAVE_ALLOCA
#define alloca malloc
#endif
#ifndef HAVE_STRDUP
char *strdup(s) char *s;
{
  char *ptr=(char *)malloc(strlen(s)+1);
  if(ptr) strcpy(ptr, s);
  return ptr;
}
#endif

static int ti_level=0;
%}

%union
{
    int num;
    char *ptr;
};

%token <num> ERRORTOKEN LEFTBRACE RIGHTBRACE
%token <num> YES NO
%token <num> RIGHT BOTTOM BOTH NONE
%token <num> MAGICWB SYSTEM
%token <num> ALWAYS AUTO MANUAL
%token <num> SEPARATOR
%token <num> T_DETAILPEN T_BLOCKPEN T_TEXTPEN T_SHINEPEN T_SHADOWPEN
%token <num> T_FILLPEN T_FILLTEXTPEN T_BACKGROUNDPEN T_HIGHLIGHTTEXTPEN
%token <num> T_BARDETAILPEN T_BARBLOCKPEN T_BARTRIMPEN
%token <num> FASTQUIT SIZEBORDER DEFAULTICON ICONDIR ICONPALETTE SCREENFONT
%token <num> ICONFONT TOOLITEM FORCEMOVE SCREEN MODULE MODULEPATH
%token <num> INTERSCREENGAP AUTORAISE
%token <ptr> STRING
%token <num> NUMBER

%type <num> truth sizeborder dri_pen forcemove_policy
%type <ptr> string

%start amiwmrc

%%

amiwmrc		: stmts
		;

stmts		: stmts stmt
		|
		;

stmt		: error
		| toolitem
		| toolsubmenu toolitems RIGHTBRACE { ti_level=0; }
		| FASTQUIT truth { prefs.fastquit = $2; }
		| SIZEBORDER sizeborder { prefs.sizeborder = $2; }
		| DEFAULTICON string { prefs.defaulticon = $2; }
		| ICONDIR string { prefs.icondir = $2; }
		| ICONPALETTE SYSTEM { set_sys_palette(); }
		| ICONPALETTE MAGICWB { set_mwb_palette(); }
		| dri_pen string { default_colors[$1] = $2; }
		| SCREENFONT string { default_screenfont = $2; }
		| ICONFONT string { label_font_name = $2; }
		| FORCEMOVE forcemove_policy { prefs.forcemove = $2; }
		| SCREEN string { openscreen($2,DefaultRootWindow(dpy)); }
		| MODULEPATH string { prefs.module_path = $2; }
		| MODULE string STRING { create_module((front? front->upfront:NULL), $2, $3); }
		| MODULE string { create_module((front? front->upfront:NULL), $2, NULL); }
		| INTERSCREENGAP NUMBER { prefs.borderwidth=$2; }
		| AUTORAISE truth { prefs.autoraise=$2; }
		;

toolsubmenu	: TOOLITEM string LEFTBRACE { add_toolitem($2, NULL, -1); ti_level=1; }
		;

toolitems	: toolitems toolitem
		|
		;

toolitem	: TOOLITEM string string { add_toolitem($2, $3, ti_level); }
		| TOOLITEM SEPARATOR { add_toolitem(NULL, NULL, ti_level); }
		;

string		: STRING { $$ = strdup($1); }
		;

truth		: YES { $$ = True; }
		| NO { $$ = False; }
		;

sizeborder	: RIGHT { $$ = Psizeright; }
		| BOTTOM { $$ = Psizebottom; }
		| BOTH { $$ = Psizeright|Psizebottom; }
		| NONE { $$ = Psizetrans; }
		| NO { $$ = Psizetrans; }
		;

dri_pen		: T_DETAILPEN { $$ = DETAILPEN; }
		| T_BLOCKPEN { $$ = BLOCKPEN; }
		| T_TEXTPEN { $$ = TEXTPEN; }
		| T_SHINEPEN { $$ = SHINEPEN; }
		| T_SHADOWPEN { $$ = SHADOWPEN; }
		| T_FILLPEN { $$ = FILLPEN; }
		| T_FILLTEXTPEN { $$ = FILLTEXTPEN; }
		| T_BACKGROUNDPEN { $$ = BACKGROUNDPEN; }
		| T_HIGHLIGHTTEXTPEN { $$ = HIGHLIGHTTEXTPEN; }
		| T_BARDETAILPEN { $$ = BARDETAILPEN; }
		| T_BARBLOCKPEN { $$ = BARBLOCKPEN; }
		| T_BARTRIMPEN { $$ = BARTRIMPEN; }
		;

forcemove_policy : ALWAYS { $$ = FM_ALWAYS; }
		 | AUTO { $$ = FM_AUTO; }
		 | MANUAL { $$ = FM_MANUAL; }
		 ;

%%
extern char *progname;
extern int ParseError;
yyerror(s) char *s;
{
    fprintf (stderr, "%s: error in input file:  %s\n", progname, s ? s : "");
    ParseError = 1;
    return 0;
}
