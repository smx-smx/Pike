/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
#include "global.h"
#include "readline_machine.h"
#include "types.h"
#include "interpret.h"
#include "svalue.h"
#include "stralloc.h"
#include "array.h"
#include "object.h"
#include "macros.h"
#include "threads.h"

#ifndef HAVE_LIBTERMCAP
#undef HAVE_LIBREADLINE
#endif

#if !defined(HAVE_READLINE_H) && !defined(HAVE_READLINE_READLINE_H)
#undef HAVE_LIBREADLINE
#endif

#if !defined(HAVE_HISTORY_H) && !defined(HAVE_READLINE_HISTORY_H) && !defined(HAVE_HISTORY_HISTORY_H)
#undef HAVE_LIBREADLINE
#endif

#ifdef HAVE_LIBREADLINE

#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#else
#ifdef HAVE_READLINE_H
#include <readline.h>
#endif
#endif

#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#else
#ifdef HAVE_HISTORY_HISTORY_H
#include <history/history.h>
#else
#ifdef HAVE_HISTORY_H
#include <history.h>
#endif
#endif
#endif

static void f_readline(INT32 args)
{
  char *r;
  struct pike_string *str;
  if(args < 1)
    error("Too few arguments to readline().\n");

  if(sp[-args].type != T_STRING)
    error("Bad argument 1 to readline()\n");

  str=sp[-args].u.string;
  THREADS_ALLOW();
  r=readline(str->str);
  THREADS_DISALLOW();

  pop_n_elems(args);
  if(r)
  {
    if(*r) add_history(r);
    push_string(make_shared_string(r));
    free(r);
  } else {
    push_int(0);
  }
}

void init_readlinemod_efuns(void)
{
  rl_bind_key('\t', rl_insert);
  add_efun("readline",f_readline,"function(string:string)",OPT_SIDE_EFFECT);
}

#else

#include <stdio.h>

#define BLOCK 16384

static void f_readline(INT32 args)
{
  char line[BLOCK];
  char *r;
  int tmp;

  if(args < 1)
    error("Too few arguments to readline().\n");

  if(sp[-args].type != T_STRING)
    error("Bad argument 1 to readline()\n");

  fwrite(sp[-args].u.string->str,1,sp[-args].u.string->len,stdout);
  fflush(stdout);

  pop_n_elems(args);
  THREADS_ALLOW();
  r=fgets(line,BLOCK,stdin);
  THREADS_DISALLOW();

  if(r)
  {
    INT32 len;
    if(len=strlen(line))
    {
      if(line[len-1]=='\n')
      {
	push_string(make_shared_binary_string(line,len-1));
	return;
      }
    }
  }
  push_int(0);
}

void init_readlinemod_efuns(void)
{
  add_efun("readline",f_readline,"function(string:string)",OPT_SIDE_EFFECT);
}

#endif

void exit_readlinemod(void) {}
void init_readlinemod_programs(void) { }


