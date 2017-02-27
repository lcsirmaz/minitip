/* mklp.h */

/***********************************************************************
* This code is part of MINITIP (a MINimal Information Theoretic Prover)
*
* Copyright (2016) Laszlo Csirmaz, Central European University, Budapest
*
* This program is free, open-source software. You may redistribute it
* and/or modify under the terms of the GNU General Public License (GPL).
*
* There is ABSOLUTELY NO WARRANTY, use at your own risk.
*************************************************************************/

/* return values for call_lp */
#define EXPR_TRUE	((char*)0x2)
#define EXPR_FALSE	((char*)0x3)
#define EQ_GE_ONLY	((char*)0x4)
#define EQ_LE_ONLY	((char*)0x5)

/* call the lp routine. Initially the expression to be checked
   is in the struct entropy_expr (with all variables set properly).
   Calling next_expr(i) puts the i-th constraint there, or returns
   1 if there are no more constraints.
   Return value: EXPR_TRUE, EXPR_FALSE, or an error string for
   other errors (such as out of memory, LP problem, etc) */
char *call_lp(int next_expr(int),int iterlimit,int timelimit);


/* EOF */

