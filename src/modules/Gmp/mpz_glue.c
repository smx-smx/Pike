/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
#include "global.h"
RCSID("$Id: mpz_glue.c,v 1.58 1999/10/26 17:52:52 noring Exp $");
#include "gmp_machine.h"

#if defined(HAVE_GMP2_GMP_H) && defined(HAVE_LIBGMP2)
#define USE_GMP2
#else /* !HAVE_GMP2_GMP_H || !HAVE_LIBGMP2 */
#if defined(HAVE_GMP_H) && defined(HAVE_LIBGMP)
#define USE_GMP
#endif /* HAVE_GMP_H && HAVE_LIBGMP */
#endif /* HAVE_GMP2_GMP_H && HAVE_LIBGMP2 */

#if defined(USE_GMP) || defined(USE_GMP2)

#include "interpret.h"
#include "svalue.h"
#include "stralloc.h"
#include "array.h"
#include "pike_macros.h"
#include "program.h"
#include "stralloc.h"
#include "object.h"
#include "pike_types.h"
#include "error.h"
#include "builtin_functions.h"
#include "opcodes.h"
#include "module_support.h"
#include "bignum.h"
#include "operators.h"

#include "my_gmp.h"

#include <limits.h>

#ifdef _MSC_VER
/* No random()... provide one for gmp
 * This should possibly be a configure test
 * /Hubbe
 */
long random(void)
{
  return my_rand();
}
#endif

#undef THIS
#define THIS ((MP_INT *)(fp->current_storage))
#define OBTOMPZ(o) ((MP_INT *)(o->storage))
#define THIS_PROGRAM (fp->context.prog)

static struct program *mpzmod_program;
#ifdef AUTO_BIGNUM
static struct program *bignum_program;
#endif

static void get_mpz_from_digits(MP_INT *tmp,
				struct pike_string *digits,
				int base)
{
  if(!base || ((base >= 2) && (base <= 36)))
  {
    int offset = 0;
    
    /* We need to fix the case with binary 0b101... and -0b101... numbers. */
    if(base == 0 && digits->len > 2)
    {
      if(INDEX_CHARP(digits->str, 0, digits->size_shift) == '-')
	offset += 1;
      if((INDEX_CHARP(digits->str, offset, digits->size_shift) == '0') &&
	 ((INDEX_CHARP(digits->str, offset+1, digits->size_shift) == 'b') ||
	  (INDEX_CHARP(digits->str, offset+1, digits->size_shift) == 'B')))
      {
	offset += 2;
	base = 2;
      }
      else
	offset = 0;
    }

    if (mpz_set_str(tmp, digits->str + offset, base))
      error("invalid digits, cannot convert to mpz");

    if(offset == 3)
    {
      /* This means a negative binary number. */
      mpz_neg(tmp, tmp);
    }
  }
  else if(base == 256)
  {
    int i;
    mpz_t digit;
    
    mpz_init(digit);
    mpz_set_ui(tmp, 0);
    for (i = 0; i < digits->len; i++)
    {
      mpz_set_ui(digit, EXTRACT_UCHAR(digits->str + i));
      mpz_mul_2exp(digit, digit, (digits->len - i - 1) * 8);
      mpz_ior(tmp, tmp, digit);
    }
    mpz_clear(digit);
  }
  else
  {
    error("invalid base.\n");
  }
}

static void get_new_mpz(MP_INT *tmp, struct svalue *s)
{
  switch(s->type)
  {
  case T_INT:
    mpz_set_si(tmp, (signed long int) s->u.integer);
    break;
    
  case T_FLOAT:
    mpz_set_d(tmp, (double) s->u.float_number);
    break;

  case T_OBJECT:
    if(s->u.object->prog != mpzmod_program
#ifdef AUTO_BIGNUM
       && s->u.object->prog != bignum_program
#endif
      )
      error("Wrong type of object, cannot convert to mpz.\n");

    mpz_set(tmp, OBTOMPZ(s->u.object));
    break;
#if 0    
  case T_STRING:
    mpz_set_str(tmp, s->u.string->str, 0);
    break;

  case T_ARRAY:   /* Experimental */
    if ( (s->u.array->size != 2)
	 || (ITEM(s->u.array)[0].type != T_STRING)
	 || (ITEM(s->u.array)[1].type != T_INT))
      error("cannot convert array to mpz.\n");
    get_mpz_from_digits(tmp, ITEM(s->u.array)[0].u.string,
			ITEM(s->u.array)[1]);
    break;
#endif
  default:
    error("cannot convert argument to mpz.\n");
  }
}

static void mpzmod_create(INT32 args)
{
  switch(args)
  {
  case 1:
    if(sp[-args].type == T_STRING)
      get_mpz_from_digits(THIS, sp[-args].u.string, 0);
    else
      get_new_mpz(THIS, sp-args);
    break;

  case 2: /* Args are string of digits and integer base */
    if(sp[-args].type != T_STRING)
      error("bad argument 1 for Mpz->create()");

    if (sp[1-args].type != T_INT)
      error("wrong type for base in Mpz->create()");

    get_mpz_from_digits(THIS, sp[-args].u.string, sp[1-args].u.integer);
    break;

  default:
    error("Too many arguments to Mpz->create()\n");

  case 0:
    break;	/* Needed by AIX cc */
  }
  pop_n_elems(args);
}

static void mpzmod_get_int(INT32 args)
{
  pop_n_elems(args);
  push_int(mpz_get_si(THIS));
}

static void mpzmod_get_float(INT32 args)
{
  pop_n_elems(args);
  push_float((float)mpz_get_d(THIS));
}

static struct pike_string *low_get_digits(MP_INT *mpz, int base)
{
  struct pike_string *s = 0;   /* Make gcc happy. */
  INT32 len;
  
  if ( (base >= 2) && (base <= 36))
  {
    len = mpz_sizeinbase(mpz, base) + 2;
    s = begin_shared_string(len);
    mpz_get_str(s->str, base, mpz);
    /* Find NULL character */
    len-=4;
    if (len < 0) len = 0;
    while(s->str[len]) len++;
    s->len=len;
    s=end_shared_string(s);
  }
  else if (base == 256)
  {
    unsigned INT32 i;
#if 0
    mpz_t tmp;
#endif

    if (mpz_sgn(mpz) < 0)
      error("only non-negative numbers can be converted to base 256.\n");
#if 0
    len = (mpz_sizeinbase(mpz, 2) + 7) / 8;
    s = begin_shared_string(len);
    mpz_init_set(tmp, mpz);
    i = len;
    while(i--)
    {
      s->str[i] = mpz_get_ui(tmp) & 0xff;
      mpz_fdiv_q_2exp(tmp, tmp, 8);
    }
    mpz_clear(tmp);
#endif

    /* lets optimize this /Mirar & Per */

    /* len = mpz->_mp_size*sizeof(mp_limb_t); */
    /* This function should not return any leading zeros. /Nisse */
    len = (mpz_sizeinbase(mpz, 2) + 7) / 8;
    s = begin_shared_string(len);

    if (!mpz->_mp_size)
    {
      /* Zero is a special case. There are no limbs at all, but
       * the size is still 1 bit, and one digit should be produced. */
      if (len != 1)
	fatal("mpz->low_get_digits: strange mpz state!\n");
      s->str[0] = 0;
    } else {
      mp_limb_t *src = mpz->_mp_d;
      unsigned char *dst = (unsigned char *)s->str+s->len;

      while (len > 0)
      {
	mp_limb_t x=*(src++);
	for (i=0; i<sizeof(mp_limb_t); i++)
	{
	  *(--dst)=x&0xff,x>>=8;
	  if (!--len)
	    break;
	  
	}
      }
    }
    s = end_shared_string(s);
  }
  else
  {
    error("invalid base.\n");
    return 0; /* Make GCC happy */
  }

  return s;
}

static void mpzmod_get_string(INT32 args)
{
  pop_n_elems(args);
  push_string(low_get_digits(THIS, 10));
}

static void mpzmod_digits(INT32 args)
{
  INT32 base;
  struct pike_string *s;
  
  if (!args)
  {
    base = 10;
  }
  else
  {
    if (sp[-args].type != T_INT)
      error("Bad argument 1 for Mpz->digits().\n");
    base = sp[-args].u.integer;
  }

  s = low_get_digits(THIS, base);
  pop_n_elems(args);

  push_string(s);
}

static void mpzmod__sprintf(INT32 args)
{
  INT_TYPE precision, base = 0, mask_shift = 0;
  struct pike_string *s = 0;
  
  if(args < 1 || sp[-args].type != T_INT)
    error("Bad argument 1 for Mpz->_sprintf().\n");
  if(args < 2 || sp[1-args].type != T_MAPPING)
    error("Bad argument 2 for Mpz->_sprintf().\n");

  push_svalue(&sp[1-args]);
  push_constant_text("precision");
  f_index(2);
  if(sp[-1].type != T_INT)
    error("Precision argument to Mpz->_sprintf() is not an integer.\n");
  precision = (--sp)->u.integer;
  
  switch(sp[-args].u.integer)
  {
  case 'O':
  case 'u': /* Note: 'u' is not really supported. */
  case 'd':
    s = low_get_digits(THIS, 10);
    break;

  case 'x':
  case 'X':
    base += 8;
    mask_shift += 1;
    /* Fall-through. */
  case 'o':
    base += 6;
    mask_shift += 2;
    /* Fall-through. */
  case 'b':
    base += 2;
    mask_shift += 1;

    if(precision > 0)
    {
      mpz_t mask;

      mpz_init_set_ui(mask, 1);
      mpz_mul_2exp(mask, mask, precision * mask_shift);
      mpz_sub_ui(mask, mask, 1);
      mpz_and(mask, mask, THIS);
      s = low_get_digits(mask, base);
      mpz_clear(mask);
    }
    else
      s = low_get_digits(THIS, base);
    break;
  }

  pop_n_elems(args);

  if(s)
    push_string(s);
  else
    push_int(0);   /* Push false? */
}

static void mpzmod__is_type(INT32 args)
{
  INT32 r = 0;
  
  if(args < 1 || sp[-args].type != T_STRING)
    error("Bad argument 1 for Mpz->_is_type().\n");

  pop_n_elems(args-1);
  push_constant_text("int");
  f_eq(2);
}

static void mpzmod_size(INT32 args)
{
  int base;
  if (!args)
  {
    /* Default is number of bits */
    base = 2;
  }
  else
  {
    if (sp[-args].type != T_INT)
      error("bad argument 1 for Mpz->size()\n");
    base = sp[-args].u.integer;
    if ((base != 256) && ((base < 2) || (base > 36)))
      error("invalid base\n");
  }
  pop_n_elems(args);

  if (base == 256)
    push_int((mpz_sizeinbase(THIS, 2) + 7) / 8);
  else
    push_int(mpz_sizeinbase(THIS, base));
}

static void mpzmod_cast(INT32 args)
{
  struct pike_string *s;

  if(args < 1)
    error("mpz->cast() called without arguments.\n");
  if(sp[-args].type != T_STRING)
    error("Bad argument 1 to mpz->cast().\n");

  s = sp[-args].u.string;
  add_ref(s);

  pop_n_elems(args);

  switch(s->str[0])
  {
  case 'i':
    if(!strncmp(s->str, "int", 3))
    {
#ifdef AUTO_BIGNUM_XXXX
      /* FIXME 1: Do we need to free the string?  Looking
	 at the case with 'o' makes this confusing...

	 FIXME 2: The run-time cast checking does not work
	 properly with bignums. Take a look at row 209 and
	 226 in opcodes.c for more examples on this. */
      push_object(this_object());
#else
      free_string(s);
      mpzmod_get_int(0);
#endif /* AUTO_BIGNUM */
      return;
    }
    break;

  case 's':
    if(!strcmp(s->str, "string"))
    {
      free_string(s);
      mpzmod_get_string(0);
      return;
    }
    break;

  case 'f':
    if(!strcmp(s->str, "float"))
    {
      free_string(s);
      mpzmod_get_float(0);
      return;
    }
    break;

  case 'o':
    if(!strcmp(s->str, "object"))
    {
      push_object(this_object());
    }
    break;

  case 'm':
    if(!strcmp(s->str, "mixed"))
    {
      push_object(this_object());
    }
    break;
    
  }

  push_string(s);	/* To get it freed when error() pops the stack. */

  error("mpz->cast() to \"%s\" is other type than string, int or float.\n",
	s->str);
}

#ifdef DEBUG_MALLOC
#define get_mpz(X,Y) \
 (debug_get_mpz((X),(Y)),( (X)->type==T_OBJECT? debug_malloc_touch((X)->u.object) :0 ),debug_get_mpz((X),(Y)))
#else
#define get_mpz debug_get_mpz 
#endif

/* Converts an svalue, located on the stack, to an mpz object */
static MP_INT *debug_get_mpz(struct svalue *s, int throw_error)
{
#define MPZ_ERROR(x) if (throw_error) error(x)
  struct object *o;
  switch(s->type)
  {
  default:
    MPZ_ERROR("Wrong type of object, cannot convert to mpz.\n");
    return 0;

  case T_INT:
  case T_FLOAT:
#if 0
  case T_STRING:
  case T_ARRAY:
#endif
    o=clone_object(mpzmod_program,0);
    get_new_mpz(OBTOMPZ(o), s);
    free_svalue(s);
    s->u.object=o;
    s->type=T_OBJECT;
    return (MP_INT *)o->storage;
    
  case T_OBJECT:
    if(s->u.object->prog != mpzmod_program
#ifdef AUTO_BIGNUM
       && s->u.object->prog != bignum_program
#endif
      )
      {
	MPZ_ERROR("Wrong type of object, cannot convert to mpz.\n");
	return 0;
      }
    return (MP_INT *)s->u.object->storage;
  }
#undef ERROR
}

/* Non-reentrant */
#if 0
/* These two functions are here so we can allocate temporary
 * objects without having to worry about them leaking in
 * case of errors..
 */
static struct object *temporary;
MP_INT *get_tmp(void)
{
  if(!temporary)
    temporary=clone_object(mpzmod_program,0);

  return (MP_INT *)temporary->storage;
}

static void return_temporary(INT32 args)
{
  pop_n_elems(args);
  push_object(temporary);
  temporary=0;
}
#endif

#ifdef AUTO_BIGNUM
static void reduce(struct object *o)
{
  INT_TYPE i;
  
  i = mpz_get_si(OBTOMPZ(o));
  if(mpz_cmp_si(OBTOMPZ(o), i) == 0)
  {
    free_object(o);
    push_int(i);
  }else{
    push_object(o);
  }
}
#define PUSH_REDUCED(o) do { struct object *reducetmp__=(o);	\
   if(THIS_PROGRAM == bignum_program)				\
     reduce(reducetmp__);					\
   else								\
     push_object(reducetmp__);					\
}while(0)

#else
#define PUSH_REDUCED(o) push_object(o)
#endif /* AUTO_BIGNUM */

#define BINFUN(name, fun)				\
static void name(INT32 args)				\
{							\
  INT32 e;						\
  struct object *res;					\
  for(e=0; e<args; e++)					\
    get_mpz(sp+e-args, 1);				\
  res = fast_clone_object(THIS_PROGRAM, 0);		\
  mpz_set(OBTOMPZ(res), THIS);				\
  for(e=0;e<args;e++)					\
    fun(OBTOMPZ(res), OBTOMPZ(res),			\
	OBTOMPZ(sp[e-args].u.object));			\
  pop_n_elems(args);					\
  PUSH_REDUCED(res);					\
}

BINFUN(mpzmod_add,mpz_add)
BINFUN(mpzmod_mul,mpz_mul)
BINFUN(mpzmod_gcd,mpz_gcd)


static void mpzmod_add_eq(INT32 args)
{
  INT32 e;
  struct object *res;
  for(e=0; e<args; e++)
    get_mpz(sp+e-args, 1);
  for(e=0;e<args;e++)
    mpz_add(THIS, THIS, OBTOMPZ(sp[e-args].u.object));
  pop_n_elems(args);
  /* FIXME: How to reduce this?? */
  ref_push_object(fp->current_object);
}

static void mpzmod_sub(INT32 args)
{
  INT32 e;
  struct object *res;

  if (args)
    for (e = 0; e<args; e++)
      get_mpz(sp + e - args, 1);
  
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);

  if(args)
  {
    for(e=0;e<args;e++)
      mpz_sub(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));
  }else{
    mpz_neg(OBTOMPZ(res), OBTOMPZ(res));
  }
  pop_n_elems(args);
  debug_malloc_touch(res);
  PUSH_REDUCED(res);
}

static void mpzmod_rsub(INT32 args)
{
  INT32 e;
  struct object *res;
  MP_INT *a;
  
  if(args!=1)
    error("Gmp.mpz->``- called with more or less than one argument.\n");
  
  a=get_mpz(sp-1,1);
  
  res = fast_clone_object(THIS_PROGRAM, 0);

  mpz_sub(OBTOMPZ(res), a, THIS);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_div(INT32 args)
{
  INT32 e;
  struct object *res;
  
  for(e=0;e<args;e++)
  {
    if(sp[e-args].type != T_INT || sp[e-args].u.integer<=0)
      if (!mpz_sgn(get_mpz(sp+e-args, 1)))
	error("Division by zero.\n");	
  }
  
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);
  for(e=0;e<args;e++)	
  {
    if(sp[e-args].type == T_INT)
      mpz_fdiv_q_ui(OBTOMPZ(res), OBTOMPZ(res), sp[e-args].u.integer);
   else
      mpz_fdiv_q(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));
  }

  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rdiv(INT32 args)
{
  MP_INT *a;
  struct object *res;
  if(!mpz_sgn(THIS))
    error("Division by zero.\n");

  if(args!=1)
    error("Gmp.mpz->``/() called with more than one argument.\n");

  a=get_mpz(sp-1,1);
  
  res=fast_clone_object(THIS_PROGRAM,0);
  mpz_fdiv_q(OBTOMPZ(res), a, THIS);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_mod(INT32 args)
{
  INT32 e;
  struct object *res;
  
  for(e=0;e<args;e++)
    if (!mpz_sgn(get_mpz(sp+e-args, 1)))
      error("Division by zero.\n");	
  
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);
  for(e=0;e<args;e++)	
    mpz_fdiv_r(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));

  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rmod(INT32 args)
{
  MP_INT *a;
  struct object *res;
  if(!mpz_sgn(THIS))
    error("Modulo by zero.\n");

  if(args!=1)
    error("Gmp.mpz->``%%() called with more than one argument.\n");

  a=get_mpz(sp-1,1);
  
  res=fast_clone_object(THIS_PROGRAM,0);
  mpz_fdiv_r(OBTOMPZ(res), a, THIS);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}


static void mpzmod_gcdext(INT32 args)
{
  struct object *g, *s, *t;
  MP_INT *a;

  if (args != 1)
    error("Gmp.mpz->gcdext: Wrong number of arguments.\n");

  a = get_mpz(sp-1, 1);
  
  g = fast_clone_object(THIS_PROGRAM, 0);
  s = fast_clone_object(THIS_PROGRAM, 0);
  t = fast_clone_object(THIS_PROGRAM, 0);

  mpz_gcdext(OBTOMPZ(g), OBTOMPZ(s), OBTOMPZ(t), THIS, a);
  pop_n_elems(args);
  PUSH_REDUCED(g); PUSH_REDUCED(s); PUSH_REDUCED(t);
  f_aggregate(3);
}

static void mpzmod_gcdext2(INT32 args)
{
  struct object *g, *s;
  MP_INT *a;

  if (args != 1)
    error("Gmp.mpz->gcdext: Wrong number of arguments.\n");

  a = get_mpz(sp-args, 1);
  
  g = fast_clone_object(THIS_PROGRAM, 0);
  s = fast_clone_object(THIS_PROGRAM, 0);

  mpz_gcdext(OBTOMPZ(g), OBTOMPZ(s), NULL, THIS, a);
  pop_n_elems(args);
  PUSH_REDUCED(g); PUSH_REDUCED(s); 
  f_aggregate(2);
}

static void mpzmod_invert(INT32 args)
{
  MP_INT *modulo;
  struct object *res;

  if (args != 1)
    error("Gmp.mpz->invert: wrong number of arguments.\n");
  modulo = get_mpz(sp-args, 1);
  if (!mpz_sgn(modulo))
    error("divide by zero");
  res = fast_clone_object(THIS_PROGRAM, 0);
  if (mpz_invert(OBTOMPZ(res), THIS, modulo) == 0)
  {
    free_object(res);
    error("Gmp.mpz->invert: not invertible");
  }
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

BINFUN(mpzmod_and,mpz_and)
BINFUN(mpzmod_or,mpz_ior)

static void mpzmod_compl(INT32 args)
{
  struct object *o;
  pop_n_elems(args);
  o=fast_clone_object(THIS_PROGRAM,0);
  mpz_com(OBTOMPZ(o), THIS);
  PUSH_REDUCED(o);
}


#define CMPFUN(name,cmp)				\
static void name(INT32 args)				\
{							\
  INT32 i;						\
  if(!args) error("Comparison with one argument?\n");	\
  i=mpz_cmp(THIS, get_mpz(sp-args, 1)) cmp 0;		\
  pop_n_elems(args);					\
  push_int(i);						\
}

#define CMPEQU(name,cmp,default)			\
static void name(INT32 args)				\
{							\
  INT32 i;						\
  MP_INT *arg;						\
  if(!args) error("Comparison with one argument?\n");	\
  if (!(arg = get_mpz(sp-args, 0)))			\
    i = default;					\
  else							\
    i=mpz_cmp(THIS, arg) cmp 0;				\
  pop_n_elems(args);					\
  push_int(i);						\
}

CMPFUN(mpzmod_gt, >)
CMPFUN(mpzmod_lt, <)
CMPFUN(mpzmod_ge, >=)
CMPFUN(mpzmod_le, <=)
CMPEQU(mpzmod_eq, ==, 0)
CMPEQU(mpzmod_nq, !=, 1)

static void mpzmod_probably_prime_p(INT32 args)
{
  int count;
  if (args)
  {
    get_all_args("Gmp.mpz->probably_prime_p", args, "%i", &count);
    count = sp[-1].u.integer;
    if (count <= 0)
      error("Gmp.mpz->probably_prime_p: count argument must be positive.\n");
  } else
    count = 25;
  pop_n_elems(args);
  push_int(mpz_probab_prime_p(THIS, count));
}

static void mpzmod_small_factor(INT32 args)
{
  int limit;

  if (args)
    {
      get_all_args("Gmp.mpz->small_factor", args, "%i", &limit);
      if (limit < 1)
	error("Gmp.mpz->small_factor: limit argument must be at least 1.\n");
    }
  else
    limit = INT_MAX;
  pop_n_elems(args);
  push_int(mpz_small_factor(THIS, limit));
}

static void mpzmod_next_prime(INT32 args)
{
  INT32 count = 25;
  INT32 limit = INT_MAX;
  struct object *o;

  switch(args)
  {
  case 0:
    break;
  case 1:
    get_all_args("Gmp.mpz->next_prime", args, "%i", &count);
    break;
  default:
    get_all_args("Gmp.mpz->next_prime", args, "%i%i", &count, &limit);
    break;
  }
  pop_n_elems(args);
  
  o = fast_clone_object(THIS_PROGRAM, 0);
  mpz_next_prime(OBTOMPZ(o), THIS, count, limit);
  
  PUSH_REDUCED(o);
}

static void mpzmod_sgn(INT32 args)
{
  pop_n_elems(args);
  push_int(mpz_sgn(THIS));
}


static void mpzmod_sqrt(INT32 args)
{
  struct object *o;
  pop_n_elems(args);
  if(mpz_sgn(THIS)<0)
    error("mpz->sqrt() on negative number.\n");

  o=fast_clone_object(THIS_PROGRAM,0);
  mpz_sqrt(OBTOMPZ(o), THIS);
  PUSH_REDUCED(o);
}

static void mpzmod_sqrtrem(INT32 args)
{
  struct object *root, *rem;
  
  pop_n_elems(args);
  if(mpz_sgn(THIS)<0)
    error("mpz->sqrtrem() on negative number.\n");

  root = fast_clone_object(THIS_PROGRAM,0);
  rem = fast_clone_object(THIS_PROGRAM,0);
  mpz_sqrtrem(OBTOMPZ(root), OBTOMPZ(rem), THIS);
  PUSH_REDUCED(root); PUSH_REDUCED(rem);
  f_aggregate(2);
}

static void mpzmod_lsh(INT32 args)
{
  struct object *res;
  if (args != 1)
    error("Wrong number of arguments to Gmp.mpz->`<<.\n");
  ref_push_string(int_type_string);
  stack_swap();
  f_cast();
  if(sp[-1].u.integer < 0)
    error("mpz->lsh on negative number.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_mul_2exp(OBTOMPZ(res), THIS, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rsh(INT32 args)
{
  struct object *res;
  if (args != 1)
    error("Wrong number of arguments to Gmp.mpz->`>>.\n");
  ref_push_string(int_type_string);
  stack_swap();
  f_cast();
  if (sp[-1].u.integer < 0)
    error("Gmp.mpz->rsh: Shift count must be positive.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_fdiv_q_2exp(OBTOMPZ(res), THIS, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rlsh(INT32 args)
{
  struct object *res;
  INT32 i;
  if (args != 1)
    error("Wrong number of arguments to Gmp.mpz->``<<.\n");
  get_mpz(sp-1,1);
  i=mpz_get_si(THIS);
  if(i < 0)
    error("mpz->``<< on negative number.\n");

  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_mul_2exp(OBTOMPZ(res), OBTOMPZ(sp[-1].u.object), i);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rrsh(INT32 args)
{
  struct object *res;
  INT32 i;
  if (args != 1)
    error("Wrong number of arguments to Gmp.mpz->``>>.\n");
  get_mpz(sp-1,1);
  i=mpz_get_si(THIS);
  if(i < 0)
    error("mpz->``>> on negative number.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_fdiv_q_2exp(OBTOMPZ(res), OBTOMPZ(sp[-1].u.object), i);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_powm(INT32 args)
{
  struct object *res;
  MP_INT *n;
  
  if(args != 2)
    error("Wrong number of arguments to Gmp.mpz->powm()\n");

  n = get_mpz(sp - 1, 1);
  if (!mpz_sgn(n))
    error("Gmp.mpz->powm: Divide by zero\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_powm(OBTOMPZ(res), THIS, get_mpz(sp - 2, 1), n);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_pow(INT32 args)
{
  struct object *res;
  
  if (args != 1)
    error("Gmp.mpz->pow: Wrong number of arguments.\n");
  if (sp[-1].type != T_INT)
    error("Gmp.mpz->pow: Non int exponent.\n");
  if (sp[-1].u.integer < 0)
    error("Gmp.mpz->pow: Negative exponent.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_pow_ui(OBTOMPZ(res), THIS, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_not(INT32 args)
{
  pop_n_elems(args);
  push_int(!mpz_sgn(THIS));
}

static void mpzmod_popcount(INT32 args)
{
  pop_n_elems(args);
  switch (mpz_sgn(THIS))
  {
  case 0:
    push_int(0);
    break;
  case -1:
    /* How would one define popcount for negative numbers? */
    error("Gmp.mpz->popcount: Undefined for negative numbers.\n");
    /* Not reached */
  case 1:
    push_int(mpn_popcount(THIS->_mp_d, THIS->_mp_size));
    break;
  default:
    fatal("Gmp.mpz->popcount: Unexpected sign!\n");
  }
}

static void gmp_pow(INT32 args)
{
  struct object *res;
  if (args != 2)
    error("Gmp.pow: Wrong number of arguments");
  if ( (sp[-2].type != T_INT) || (sp[-2].u.integer < 0)
       || (sp[-1].type != T_INT) || (sp[-1].u.integer < 0))
    error("Gmp.pow: Negative arguments");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_ui_pow_ui(OBTOMPZ(res), sp[-2].u.integer, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void gmp_fac(INT32 args)
{
  struct object *res;
  if (args != 1)
    error("Gmp.fac: Wrong number of arguments.\n");
  if (sp[-1].type != T_INT)
    error("Gmp.fac: Non int argument.\n");
  if (sp[-1].u.integer < 0)
    error("Gmp.mpz->pow: Negative exponent.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_fac_ui(OBTOMPZ(res), sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_random(INT32 args)
{
  struct object *res;
  pop_n_elems(args);
  if(mpz_sgn(THIS) <= 0)
    error("random on negative number.\n");

  res=fast_clone_object(THIS_PROGRAM,0);
  /* We add two to assure reasonably uniform randomness */
  mpz_random(OBTOMPZ(res), mpz_size(THIS) + 2);
  mpz_fdiv_r(OBTOMPZ(res), OBTOMPZ(res), THIS); /* modulo */
  PUSH_REDUCED(res);
}
  
static void init_mpz_glue(struct object *o)
{
  mpz_init(THIS);
}

static void exit_mpz_glue(struct object *o)
{
  mpz_clear(THIS);
}
#endif

void pike_module_exit(void)
{
#if defined(USE_GMP) || defined(USE_GMP2)
  free_program(mpzmod_program);
  mpzmod_program=0;
#ifdef AUTO_BIGNUM
  free_program(bignum_program);
  bignum_program=0;
#endif
#endif
}


#define MPZ_ARG_TYPE "int|float|object"
#define MPZ_SHIFT_TYPE "function(int|float|object:object)"
#define MPZ_BINOP_TYPE ("function(" MPZ_ARG_TYPE "...:object)")
#define MPZ_CMPOP_TYPE ("function(" MPZ_ARG_TYPE ":int)")

#define MPZ_DEFS()							\
  ADD_STORAGE(MP_INT);							\
  									\
  /* function(void|string|int|float|object:void)"			\
  "|function(string,int:void) */					\
  ADD_FUNCTION("create", mpzmod_create,					\
	       tOr(tFunc(tOr5(tVoid,tStr,tInt,tFlt,			\
			      tObj),tVoid),				\
		   tFunc(tStr tInt,tVoid)), 0);				\
									\
  add_function("`+",mpzmod_add,MPZ_BINOP_TYPE,0);			\
  add_function("`+=",mpzmod_add_eq,MPZ_BINOP_TYPE,0);			\
  add_function("``+",mpzmod_add,MPZ_BINOP_TYPE,0);			\
  add_function("`-",mpzmod_sub,MPZ_BINOP_TYPE,0);			\
  add_function("``-",mpzmod_rsub,MPZ_BINOP_TYPE,0);			\
  add_function("`*",mpzmod_mul,MPZ_BINOP_TYPE,0);			\
  add_function("``*",mpzmod_mul,MPZ_BINOP_TYPE,0);			\
  add_function("`/",mpzmod_div,MPZ_BINOP_TYPE,0);			\
  add_function("``/",mpzmod_rdiv,MPZ_BINOP_TYPE,0);			\
  add_function("`%",mpzmod_mod,MPZ_BINOP_TYPE,0);			\
  add_function("``%",mpzmod_rmod,MPZ_BINOP_TYPE,0);			\
  add_function("`&",mpzmod_and,MPZ_BINOP_TYPE,0);			\
  add_function("``&",mpzmod_and,MPZ_BINOP_TYPE,0);			\
  add_function("`|",mpzmod_or,MPZ_BINOP_TYPE,0);			\
  add_function("``|",mpzmod_or,MPZ_BINOP_TYPE,0);			\
  /* function(:object) */						\
  ADD_FUNCTION("`~",mpzmod_compl,tFunc(tNone,tObj),0);			\
									\
  add_function("`<<",mpzmod_lsh,MPZ_SHIFT_TYPE,0);			\
  add_function("`>>",mpzmod_rsh,MPZ_SHIFT_TYPE,0);			\
  add_function("``<<",mpzmod_rlsh,MPZ_SHIFT_TYPE,0);			\
  add_function("``>>",mpzmod_rrsh,MPZ_SHIFT_TYPE,0);			\
									\
  add_function("`>", mpzmod_gt,MPZ_CMPOP_TYPE,0);			\
  add_function("`<", mpzmod_lt,MPZ_CMPOP_TYPE,0);			\
  add_function("`>=",mpzmod_ge,MPZ_CMPOP_TYPE,0);			\
  add_function("`<=",mpzmod_le,MPZ_CMPOP_TYPE,0);			\
									\
  add_function("`==",mpzmod_eq,MPZ_CMPOP_TYPE,0);			\
  add_function("`!=",mpzmod_nq,MPZ_CMPOP_TYPE,0);			\
									\
  /* function(:int) */							\
  ADD_FUNCTION("`!",mpzmod_not,tFunc(tNone,tInt),0);			\
									\
  /* function(:int) */							\
  ADD_FUNCTION("__hash",mpzmod_get_int,tFunc(tNone,tInt),0);		\
  /* function(string:mixed) */						\
  ADD_FUNCTION("cast",mpzmod_cast,tFunc(tStr,tMix),0);			\
									\
  /* function(int:string) */						\
  ADD_FUNCTION("_is_type", mpzmod__is_type, tFunc(tStr,tInt), 0);	\
  									\
  /* function(void|int:string) */					\
  ADD_FUNCTION("digits", mpzmod_digits,tFunc(tOr(tVoid,tInt),tStr), 0);	\
  /* function(int:string) */						\
  ADD_FUNCTION("_sprintf", mpzmod__sprintf, tFunc(tInt,tStr), 0);	\
  /* function(void|int:int) */						\
  ADD_FUNCTION("size", mpzmod_size,tFunc(tOr(tVoid,tInt),tInt), 0);	\
									\
  /* function(:int) */							\
  ADD_FUNCTION("cast_to_int",mpzmod_get_int,tFunc(tNone,tInt),0);	\
  /* function(:string) */						\
  ADD_FUNCTION("cast_to_string",mpzmod_get_string,tFunc(tNone,tStr),0);	\
  /* function(:float) */						\
  ADD_FUNCTION("cast_to_float",mpzmod_get_float,tFunc(tNone,tFlt),0);	\
									\
  /* function(:int) */							\
  ADD_FUNCTION("probably_prime_p",mpzmod_probably_prime_p,		\
	       tFunc(tNone,tInt),0);					\
  /* function(int|void:int) */						\
  ADD_FUNCTION("small_factor", mpzmod_small_factor,			\
	       tFunc(tOr(tInt,tVoid),tInt), 0);				\
  /* function(int|void,int|void:object) */				\
  ADD_FUNCTION("next_prime", mpzmod_next_prime,				\
	       tFunc(tOr(tInt,tVoid) tOr(tInt,tVoid),tObj), 0);		\
  									\
  add_function("gcd",mpzmod_gcd, MPZ_BINOP_TYPE, 0);			\
  add_function("gcdext", mpzmod_gcdext,					\
  "function(" MPZ_ARG_TYPE ":array(object))", 0);			\
  add_function("gcdext2", mpzmod_gcdext2,				\
  "function(" MPZ_ARG_TYPE ":array(object))", 0);			\
  add_function("invert", mpzmod_invert,					\
  "function(" MPZ_ARG_TYPE ":object)", 0);				\
									\
  /* function(:object) */						\
  ADD_FUNCTION("sqrt", mpzmod_sqrt,tFunc(tNone,tObj),0);		\
  /* function(:array(object)) */					\
  ADD_FUNCTION("sqrtrem", mpzmod_sqrtrem,tFunc(tNone,tArr(tObj)), 0);	\
  add_function("powm",mpzmod_powm,					\
  "function(" MPZ_ARG_TYPE "," MPZ_ARG_TYPE ":object)", 0);		\
  /* function(int:object) */						\
  ADD_FUNCTION("pow", mpzmod_pow,tFunc(tInt,tObj), 0);			\
									\
  /* function(void:int) */						\
  ADD_FUNCTION("popcount", mpzmod_popcount,tFunc(tVoid,tInt), 0);	\
									\
  ADD_FUNCTION("_random",mpzmod_random,tFunc(tNone,tObj),0);		\
  									\
  set_init_callback(init_mpz_glue);					\
  set_exit_callback(exit_mpz_glue);


void pike_module_init(void)
{
#if defined(USE_GMP) || defined(USE_GMP2)
  start_new_program();

  MPZ_DEFS();

#if 0
  /* These are not implemented yet */
  /* function(:int) */
  ADD_FUNCTION("squarep", mpzmod_squarep,tFunc(tNone,tInt), 0);
  add_function("divmod", mpzmod_divmod, "function(" MPZ_ARG_TYPE ":array(object))", 0);
  add_function("divm", mpzmod_divm, "function(" MPZ_ARG_TYPE ","
	       MPZ_ARG_TYPE ":object)", 0);
#endif

  add_program_constant("mpz", mpzmod_program=end_program(), 0);

  /* function(int, int:object) */
  ADD_FUNCTION("pow", gmp_pow,tFunc(tInt tInt,tObj), 0);
  /* function(int:object) */
  ADD_FUNCTION("fac", gmp_fac,tFunc(tInt,tObj), 0);

#ifdef AUTO_BIGNUM
  {
    int id;
    extern gmp_library_loaded;

    /* This program autoconverts to integers, Gmp.mpz does not!!
     * magic? no, just an if statement :)              /Hubbe
     */
    start_new_program();

    /* I first tried to just do an inherit here, but it becomes too hard
     * to tell the programs apart when I do that..          /Hubbe
     */
    MPZ_DEFS();

    id=add_program_constant("bignum", bignum_program=end_program(), 0);
    
    /* Alert bignum.c that we have been loaded /Hubbe */
    gmp_library_loaded=1;

#if 0
    /* magic /Hubbe
     * This seems to break more than it fixes though... /Hubbe
     */
    free_string(ID_FROM_INT(new_program, id)->type);
    ID_FROM_INT(new_program, id)->type=CONSTTYPE(tOr(tFunc(tOr5(tVoid,tStr,tInt,tFlt,tObj),tInt),tFunc(tStr tInt,tInt)));
#endif
  }
#endif

#endif
}

