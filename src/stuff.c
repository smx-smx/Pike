/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: stuff.c,v 1.13 2000/09/20 02:04:26 hubbe Exp $
 */
#include "global.h"
#include "stuff.h"
#include "stralloc.h"

/* Not all of these are primes, but they should be adequate */
PMOD_EXPORT INT32 hashprimes[32] =
{
  31,        /* ~ 2^0  = 1 */
  31,        /* ~ 2^1  = 2 */
  31,        /* ~ 2^2  = 4 */
  31,        /* ~ 2^3  = 8 */
  31,        /* ~ 2^4  = 16 */
  31,        /* ~ 2^5  = 32 */
  61,        /* ~ 2^6  = 64 */
  127,       /* ~ 2^7  = 128 */
  251,       /* ~ 2^8  = 256 */
  541,       /* ~ 2^9  = 512 */
  1151,      /* ~ 2^10 = 1024 */
  2111,      /* ~ 2^11 = 2048 */
  4327,      /* ~ 2^12 = 4096 */
  8803,      /* ~ 2^13 = 8192 */
  17903,     /* ~ 2^14 = 16384 */
  32321,     /* ~ 2^15 = 32768 */
  65599,     /* ~ 2^16 = 65536 */
  133153,    /* ~ 2^17 = 131072 */
  270001,    /* ~ 2^18 = 264144 */
  547453,    /* ~ 2^19 = 524288 */
  1109891,   /* ~ 2^20 = 1048576 */
  2000143,   /* ~ 2^21 = 2097152 */
  4561877,   /* ~ 2^22 = 4194304 */
  9248339,   /* ~ 2^23 = 8388608 */
  16777215,  /* ~ 2^24 = 16777216 */
  33554431,  /* ~ 2^25 = 33554432 */
  67108863,  /* ~ 2^26 = 67108864 */
  134217727, /* ~ 2^27 = 134217728 */
  268435455, /* ~ 2^28 = 268435456 */
  536870911, /* ~ 2^29 = 536870912 */
  1073741823,/* ~ 2^30 = 1073741824 */
  2147483647,/* ~ 2^31 = 2147483648 */
};

/* same thing as (int)floor(log((double)x) / log(2.0)) */
/* Except a bit quicker :) (hopefully) */

PMOD_EXPORT int my_log2(size_t x)
{
  static signed char bit[256] =
  {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  };
  register size_t tmp;
#if SIZEOF_CHAR_P > 4
  if((tmp=(x>>32)))
  {
    if((x=(tmp>>16))) {
      if((tmp=(x>>8))) return bit[tmp]+56;
      return bit[x]+48;
    }
    if((x=(tmp>>8))) return bit[x]+40;
    return bit[tmp]+32;
  }
#endif /* SIZEOF_CHAP_P > 4 */
  if((tmp=(x>>16)))
  {
    if((x=(tmp>>8))) return bit[x]+24;
    return bit[tmp]+16;
  }
  if((tmp=(x>>8))) return bit[tmp]+8;
  return bit[x];
}


/* Return the number of bits in a 32-bit integer */
PMOD_EXPORT int count_bits(unsigned INT32 x)
{
#define B(X) X+0,X+1,X+1,X+2,\
             X+1,X+2,X+2,X+3,\
             X+1,X+2,X+2,X+3,\
             X+2,X+3,X+3,X+4
  static char bits[256] =
  {
    B(0), B(1), B(1), B(2),
    B(1), B(2), B(2), B(3),
    B(1), B(2), B(2), B(3),
    B(2), B(3), B(3), B(4)
  };

  return (bits[x & 255] +
	  bits[(x>>8) & 255] +
	  bits[(x>>16) & 255] +
	  bits[(x>>24) & 255]);
}

/* Return true for integers with more than one bit set */
PMOD_EXPORT int is_more_than_one_bit(unsigned INT32 x)
{
  return !!(x & (x-1));
}

PMOD_EXPORT double my_strtod(char *nptr, char **endptr)
{
  double tmp=STRTOD(nptr,endptr);
  if(*endptr>nptr)
  {
    if(endptr[0][-1]=='.')
      endptr[0]--;
  }
  return tmp;
}

PMOD_EXPORT unsigned INT32 my_sqrt(unsigned INT32 n)
{
  unsigned INT32 b, s, y=0;
  unsigned INT16 x=0;
  
  for(b=1<<(sizeof(INT32)*8-2); b; b>>=2)
  {
    x<<=1; s=b+y; y>>=1;
    if(n>=s)
    {
      x|=1; y|=b; n-=s;
    }
  }
  return x;
}
