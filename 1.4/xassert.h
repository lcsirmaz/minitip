/* xassert.h */

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

#define xassert(expr) \
    ((void)((expr)|| (minitip_assert(#expr,__FILE__,__LINE__),1)))

void minitip_assert(const char *expr,const char *file, int line);

/* EOF */

