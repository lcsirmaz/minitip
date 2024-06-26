/* minitip.h (bounds and default values) */

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

#ifndef MINITIP_H
#define MINITIP_H

/* stringify macro */
#define stringify(x)	#x
#define mkstringof(x)	stringify(x)

/* Bounds and default values */

/* interactive prompt */
#define minitip_PROMPT		"minitip: "
/* default history file, can be override as a parameter */
#define DEFAULT_HISTORY_FILE	".minitip"
/* default rc file; parse and execute before the first prompt */
#define DEFAULT_RC_FILE		".minitiprc"
/* initial style: syntax_short or syntax_full */
#define minitip_INITIAL_STYLE	syntax_short
/* initial separator character; should be ';' for syntax_full */
#define minitip_INITIAL_SEPCHAR	','
/* allowed separator characters in syntax_short */
#define minitip_SEPARATOR_CHARS	",./:;~!@#%^"

/* maximal number of constraints handled */
#define minitip_INITIAL_CONSTR	50
/* maximal number of macros which can be stored */
#define minitip_INITIAL_MACRONO	50
/* maximal number of variables handled. It must be at most
   31 as variables are stored as bits in an integer */
#define minitip_MAX_ID_NO	27
/* maximal length of a full entropy identifies */
#define minitip_MAX_ID_LENGTH	25
/* maximal length of a fully expanded entropy expression */
#define minitip_MAX_EXPR_LENGTH	550
/* maximal depth of information measure, 2^depth < MAX_EXPR_LENGTH */
#define minitip_MAX_MEASURE_DEPTH 6
/* maximal length of a line in a batch file */
#define minitip_MAX_LINE_LENGTH	1024
/* maximal depth of batch file embedding */
#define minitip_MAX_BATCH_DEPTH	5

/* return values for offline usage */
#define EXIT_TRUE	0
#define EXIT_INFO	0
#define EXIT_FALSE	1
#define EXIT_SYNTAX	2
#define EXIT_ERROR	3

#endif

/* EOF */


