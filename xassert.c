/* xassert .c */

/***********************************************************************
* This code is part of MINITIP (a MINimal Information Theoretic Prover)
*
* Copyright (2016) Laszlo Csirmaz, Central European University, Budapest
*
* This program is free, open-source software. You may redistribute it
* and/or modify unter the terms of the GNU General Public License (GPL).
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
