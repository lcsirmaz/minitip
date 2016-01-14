/* xassert .c */

#include <stdio.h>
#include <stdlib.h>

void minitip_assert(const char *expr, const char *file, int line)
{
    fprintf(stderr,"assertion \"%s\" failed in %s line %d\n",expr,file,line);
    exit(7); /* sorry, cannot do otherwise */
}