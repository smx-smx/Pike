#include "global.h"
#include "cyclic.h"

RCSID("$Id: cyclic.c,v 1.5 2000/08/10 08:54:34 grubba Exp $");

#define CYCLIC_HASH_SIZE 4711

CYCLIC *cyclic_hash[CYCLIC_HASH_SIZE];

static void low_unlink_cyclic(CYCLIC *c)
{
  size_t h;
  CYCLIC **p;
  h=(ptrdiff_t)c->id;
  h*=33;
  h|=(ptrdiff_t)c->a;
  h*=33;
  h|=(ptrdiff_t)c->b;
  h*=33;
  h|=(ptrdiff_t)c->th;
  h*=33;
  h%=CYCLIC_HASH_SIZE;

  for(p=cyclic_hash+h;*p;p=&(p[0]->next))
  {
    if(c == *p)
    {
      *p=c->next;
      return;
    }
  }
  fatal("Unlink cyclic on lost cyclic struct.\n");
}

void unlink_cyclic(CYCLIC *c)
{
  UNSET_ONERROR(c->onerr);
  low_unlink_cyclic(c);
}

void *begin_cyclic(CYCLIC *c,
		   void *id,
		   void *th,
		   void *a,
		   void *b)
{
  size_t h;
  void *ret=0;
  CYCLIC *p;

  h=(ptrdiff_t)id;
  h*=33;
  h|=(ptrdiff_t)a;
  h*=33;
  h|=(ptrdiff_t)b;
  h*=33;
  h|=(ptrdiff_t)th;
  h*=33;
  h%=CYCLIC_HASH_SIZE;

  for(p=cyclic_hash[h];p;p=p->next)
  {
    if(a == p->a && b==p->b && id==p->id)
    {
      ret=p->ret;
      break;
    }
  }

  c->ret=(void *)(ptrdiff_t)1;
  c->a=a;
  c->b=b;
  c->id=id;
  c->th=th;
  c->next=cyclic_hash[h];
  cyclic_hash[h]=c;
  SET_ONERROR(c->onerr, low_unlink_cyclic, c);
  return ret;
}
