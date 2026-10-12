/* xassert .c */

/***********************************************************************
* This code is part of MINITIP (a MINimal Information Theoretic Prover)
*
* Copyright (2016-2024) Laszlo Csirmaz, github.com/lcsirmaz/minitip
*
* This program is free, open-source software. You may redistribute it
* and/or modify under the terms of the GNU General Public License (GPL).
*
* There is ABSOLUTELY NO WARRANTY, use at your own risk.
*************************************************************************/

#include <stdio.h>
#include <stdlib.h>

void minitip_assert(const char *expr, const char *file, int line)
{
    fprintf(stderr,"assertion \"%s\" failed in %s line %d\n",expr,file,line);
    exit(7); /* sorry, cannot do otherwise */
}

/* EOF */
