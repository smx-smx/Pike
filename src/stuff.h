/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: stuff.h,v 1.10 2000/09/20 02:04:26 hubbe Exp $
 */
#ifndef STUFF_H
#define STUFF_H

#include "global.h"

/* Prototypes begin here */
int my_log2(size_t x);
int count_bits(unsigned INT32 x);
int is_more_than_one_bit(unsigned INT32 x);
double my_strtod(char *nptr, char **endptr);
PMOD_EXPORT unsigned INT32 my_sqrt(unsigned INT32 n);
/* Prototypes end here */

extern INT32 hashprimes[32];

#endif
