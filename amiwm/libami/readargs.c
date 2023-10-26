#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define FLG_A 1
#define FLG_K 2
#define FLG_S 4
#define FLG_N 8
#define FLG_M 16
#define FLG_T 32
#define FLG_ARG 256

struct RDArgs {
  int acnt;
  char *argused, *templ;
  struct ra_arg {
    int l, flags;
    const char *t;
    char *arg;
    long num;
  } *a;
  long *array;
};

void freeargs(struct RDArgs *ra)
{
  if(ra) {
    if(ra->argused) free(ra->argused);
    if(ra->templ) free(ra->templ);
    if(ra->a) free(ra->a);
    free(ra);
  }
}

static int findrae(struct RDArgs *ra, const char *a, int l)
{
  int n;
  for(n=0; n<ra->acnt; n++)
    if(ra->a[n].l==l && !strncasecmp(ra->a[n].t, a, l))
      return n;
  return -1;
}

static int parsetmpl(struct RDArgs *ra, int n)
{
  const char *t=ra->a[n].t;
  int l=ra->a[n].l;
  while(l>2 && t[l-2]=='/') {
    switch(t[l-1]) {
    case 's':
    case 'S':
      ra->a[n].flags|=FLG_S|FLG_K;
      break;
    case 't':
    case 'T':
      ra->a[n].flags|=FLG_T|FLG_S|FLG_K;
      break;
    case 'n':
    case 'N':
      ra->a[n].flags|=FLG_N;
      break;
    case 'm':
    case 'M':
      ra->a[n].flags|=FLG_M;
      break;
    case 'a':
    case 'A':
      ra->a[n].flags|=FLG_A;
      break;
    case 'k':
    case 'K':
      ra->a[n].flags|=FLG_K;
      break;
    default:
      errno=EINVAL;
      return 0;
    }
    l-=2;
  }
  ra->a[n].l=l;
  if(!(ra->a[n].flags&FLG_S)) ra->a[n].flags|=FLG_ARG;
  if((ra->a[n].flags&(FLG_S|FLG_M))==(FLG_S|FLG_M)||
     (ra->a[n].flags&(FLG_N|FLG_M))==(FLG_N|FLG_M)||
     (ra->a[n].flags&(FLG_S|FLG_N))==(FLG_S|FLG_N)) {
    errno=EINVAL;
    return 0;
  }
  return 1;
}

static int setraarg(struct RDArgs *ra, int n, char *val)
{
  if((val && !(ra->a[n].flags&FLG_ARG))||(!val && (ra->a[n].flags&FLG_ARG))) {
    errno=EINVAL;
    return 0;
  }
  if(ra->a[n].flags&FLG_T) {
    ra->array[n]=!ra->array[n];
    ra->a[n].arg=(char *)1;
    return 1;
  }
  if(ra->a[n].arg) {
    errno=EINVAL;
    return 0;
  }
  if(val) {
    if(ra->a[n].flags&FLG_N) {
      ra->array[n]=(long)&ra->a[n].num;
      ra->a[n].num=atoi(val);
    } else ra->array[n]=(long)val;
    ra->a[n].arg=val;
  } else {
    ra->array[n]=1;
    ra->a[n].arg=(char *)1;
  }
  return 1;
}

static int addmarg(struct RDArgs *ra, int n, char *val, int maxcnt)
{
  if(!ra->a[n].arg)
    if((ra->a[n].arg=calloc(maxcnt+1, sizeof(char *)))) {
      ra->array[n]=(long)ra->a[n].arg;
      ra->a[n].num=0;
    } else return 0;
  ((char **)ra->a[n].arg)[ra->a[n].num++]=val;
  return 1;
}

struct RDArgs *readargs(const char *template, long *array, struct RDArgs *ra,
			int argc, char **argv)
{
  int i,n,acnt,vcnt=argc-1;
  struct RDArgs *ara=NULL;
  char *t2;

  if(!ra)
    if(!(ra=ara=calloc(1, sizeof(struct RDArgs))))
      return NULL;

  ra->array=array;

  if(argc>1 && !template[0]) {
    errno=EINVAL;
    goto fail;
  }
  if(!(ra->templ=strdup(template)))
    goto fail;

  ra->acnt=1;
  for(strtok(ra->templ, ","); strtok(NULL, ","); ra->acnt++);

  ra->argused=calloc(argc, sizeof(char));
  ra->a=calloc(ra->acnt, sizeof(struct ra_arg));

  for(t2=ra->templ, n=0; n<ra->acnt; n++, t2+=i+1) {
    i=strlen(t2);
    ra->a[n].t=t2; ra->a[n].l=i;
    if(!parsetmpl(ra, n))
      goto fail;
  }

  for(i=1; i<argc; i++) {
    char *eq=strchr(argv[i], '=');
    if(eq && (n=findrae(ra, argv[i], eq-argv[i]))>=0) {
      if(!setraarg(ra, n, eq+1))
	goto fail;
      ra->argused[i]++;
      --vcnt;
    }
  }

  for(i=1; i<argc; i++)
    if(!ra->argused[i])
      if((n=findrae(ra, argv[i], strlen(argv[i])))>=0) {
	if(ra->a[n].flags & FLG_ARG) {
	  if(i+1>=argc || ra->argused[i+1]) {
	    errno=EINVAL;
	    goto fail;
	  }
	  if(!setraarg(ra, n, argv[i+1]))
	    goto fail;
	  ra->argused[i]++;
	  ra->argused[i+1]++;
	  vcnt-=2;
	} else {
	  if(!setraarg(ra, n, NULL))
	    goto fail;
	  ra->argused[i]++;
	  --vcnt;
	}
      }

  for(acnt=0, n=0; n<ra->acnt; n++)
    if((ra->a[n].flags & FLG_A)&&!ra->a[n].arg)
      acnt++;

  for(i=1, n=0; vcnt>0 && n<ra->acnt;)
    if(ra->a[n].arg || (ra->a[n].flags & FLG_K))
      n++;
    else if(ra->argused[i])
      i++;
    else if(ra->a[n].flags & FLG_M) {
      while(vcnt>0 && vcnt>acnt) {
	if(!addmarg(ra, n, argv[i], vcnt))
	  goto fail;
	--vcnt;
	ra->argused[i]++;
	i++;
      }
      if(ra->a[n].flags&FLG_A)
	--acnt;
      n++;
    } else {
      if(!setraarg(ra, n, argv[i]))
	goto fail;
      if(ra->a[n].flags&FLG_A)
	--acnt;
      --vcnt;
      ra->argused[i]++;
      n++; i++;
    }
  
  if(!acnt && !vcnt)
    return ra;
  errno=EINVAL;

fail:
  if(ara)
    freeargs(ara);
  return NULL;
}
