#include "global.h"
#include "stralloc.h"
#include "global.h"
RCSID("$Id: linkfarm.c,v 1.2 2001/07/03 16:09:33 per Exp $");
#include "pike_macros.h"
#include "interpret.h"
#include "program.h"
#include "program_id.h"
#include "object.h"
#include "array.h"
#include "operators.h"
#include "module_support.h"

#include "config.h"

#include "whitefish.h"
#include "resultset.h"
#include "blob.h"
#include "buffer.h"
#include "blobs.h"

/* must be included last */
#include "module_magic.h"
static void exit_linkfarm_struct( );

#define HSIZE 83

static struct program *linkfarm_program;

struct hash
{
  struct pike_string *s;
  struct hash *next;
};

struct linkfarm
{
  int size;
  struct hash *hash[HSIZE];
};

#define THIS ((struct linkfarm *)Pike_fp->current_storage)

static struct hash *new_hash( struct pike_string *id )
{
  struct hash *res =  malloc( sizeof( struct hash ) );
  res->s = id;
  res->next = 0;
  return res;
}

static void free_hash( struct hash *h )
{
  while( h )
  {
    struct hash *n = h->next;
    if( h->s ) free_string( h->s );
    free( h );
    h = n;
  }
}

static int find_hash( struct linkfarm *d, struct pike_string *s )
{
  int r = ((int)((long)s>>3)) % HSIZE;
  struct hash *h = d->hash[ r ];
  while( h )
  {
    if( h->s == s )
      return 0;
    h = h->next;
  }
  d->size++;
  h = new_hash( s );
  h->next = d->hash[ r ];
  d->hash[ r ] = h;
  return 0;
}

static int low_add( struct linkfarm *t,
		    struct pike_string *s )
{
  int ret=0, i;

  switch( s->size_shift )
  {
    case 0:
      {
	char *d = (unsigned char*)s->str;
	for( i = 0; i<s->len; i++ )
	  if( d[i] == '#' )
	  {
	    if( !i ) return 1;
	    s = make_shared_binary_string0( d, i );
	    ret = 1;
	    break;
	  }
      };
      break;
    case 1:
      {
	short *d = (unsigned short*)s->str;
	for( i = 0; i<s->len; i++ )
	  if( d[i] == '#' )
	  {
	    if( !i ) return 1;
	    s = make_shared_binary_string1( d, i );
	    ret = 1;
	    break;
	  }
      };
      break;
    case 2:
      {
	int *d = (int*)s->str;
	for( i = 0; i<s->len; i++ )
	  if( d[i] == '#' )
	  {
	    if( !i ) return 1;
	    s = make_shared_binary_string2( d, i );
	    ret = 1;
	    break;
	  }
      };
      break;
  };
  if( find_hash( t, s ) )
  {
    if( ret ) free_string( s );
    return 1;
  }
  return ret;
}


static void f_linkfarm_add( INT32 args )
{
  struct pike_string *s;
  struct linkfarm *f = THIS;
  if( Pike_sp[-1].type != T_STRING )
    return;

  if( low_add( f, Pike_sp[-1].u.string ) )
    pop_stack();
  else
    Pike_sp--;
}


static void f_linkfarm_memsize( INT32 args )
/*
 *! @decl int memsize()
 *!
 *! Returns the in-memory size of the linkfarm
 */
{
  int size = HSIZE*sizeof(void *);
  int i;
  struct hash *h;
  struct linkfarm *t = THIS;
  
  for( i = 0; i<HSIZE; i++ )
  {
    h = t->hash[i];
    while( h )
    {
      size += h->s->len + sizeof(struct hash);
      h = h->next;
    }
  }
  pop_n_elems(args);
  push_int( size );
}


static void f_linkfarm_read( INT32 args )
/*
 *! @decl array read();
 *!
 *! returns ({ int word_id, @[Blob] b }) or 0
 */
{
  struct linkfarm *t = THIS;
  struct array *a = allocate_array( THIS->size );
  int i, n=0;

  for( i = 0; i<HSIZE; i++ )
  {
    struct hash *h = t->hash[i];
    while( h )
    {
/*       fprintf(stderr, "%d/%d %s\n", n+1, THIS->size, h->s->str ); */
      a->item[n].u.string = h->s;
      h->s = 0;
      a->item[n].type = PIKE_T_STRING;
      h = h->next;
      n++;
    }
  }
  pop_n_elems( args );
  push_array( a );
}


static void init_linkfarm_struct( )
{
  MEMSET( THIS, 0, sizeof( struct linkfarm ) );
}

static void exit_linkfarm_struct( )
{
  int i;

  for( i = 0; i<HSIZE; i++ )
    if( THIS->hash[i] )
      free_hash( THIS->hash[i] );

/*   if( THIS->buffer ) */
/*     wf_buffer_free( THIS->buffer ); */

  init_linkfarm_struct();
}


void init_linkfarm_program()
{
  start_new_program();
  ADD_STORAGE(struct linkfarm);
  add_function("add",f_linkfarm_add,
	       "function(int,array,int,int:void)",0 );
  add_function("memsize", f_linkfarm_memsize, "function(void:int)", 0 );
  add_function("read", f_linkfarm_read, "function(void:array(string))", 0 );
  set_init_callback( init_linkfarm_struct );
  set_exit_callback( exit_linkfarm_struct );
  linkfarm_program = end_program( );
  add_program_constant( "LinkFarm", linkfarm_program, 0 );
}

void exit_linkfarm_program()
{
  free_program( linkfarm_program );
}
  
