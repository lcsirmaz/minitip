/* parser.h: minitip parser module */

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

/*
   Type: ORIGINAL / SIMPLE
   Variable names:    identifiers             letters a--z + digits
   Set of variables:  comma separated list    sequence of variables
   entropy            H(<v>)                  <v>
   conditional        H(<v1>|<v2>)            (<v1>|<v2>)
   mutual             I(<v1>;<v2>)            (<v1>,<v2>)
   cond.mutual        I(<v1>;<v2>|<v3>)       (<v1>,<v2>|<v3>)
   Ingleton           [<v1>;<v2>;<v3>,<v4>]   [<v1>,<v2>,<v3>,<v4>]
   Macro              D(<v1>;<v2>|<v3>)       D(<v1>,<v2>|<v3>)

   Expression:   [+-]<multiplier>[*]<entropy>
   relation:     <expression> [ = | >= | <= | == ] <expression>
   the LHS, RHS can be zero; leading '+' can be omitted
   
   Conditionals as abbreviation:
    functional dependency:    <v1> : <v2>
    independent:              <v1> . <v2> . ...  .<vn>
                              <v1> || <v2> || ... || <vn>
    Markov:                   <v1> / <v2> / ... / <vn>
                              <v1> -> <v2> -> ... -> <vn>
*/

/***********************************************************************
* minitip style: how the expression is parsed. Ppossible values:
*     syntax_full  (original)
*     syntax_short (simple)
*
* void set_syntax_style(style,sepchar,extvar)
*     set the style to the given one, the character as
*     separator between variables sequences, and allow
*     extended variable syntax in simple style
*
* void set_syntax_measure(how,idx_from,idx_to)
*     set whether extended measures can be used. Indices
*     are for the macro invocations.
*/

typedef enum {
    syntax_full,
    syntax_short
} syntax_style_t;

void set_syntax_style(syntax_style_t style,char sepchar,int extvar);
void set_syntax_measure(int msr,int mfrom,int mto);

/***********************************************************************
* Syntax error
*    when an error is found the syntax_error structure is filled
*    with the error text and the error position.
*    harderr:   syntax error
*    softerr:   some resource is exhausted (too long identifier, 
*               too many terms, etc).
*/
struct syntax_error_t {
   const char *softerrstr, *harderrstr;
   int softerrpos, harderrpos;
   int showexpression;
};
extern struct syntax_error_t syntax_error;

/***********************************************************************
* Parsed entropy line
*    The parsed line is put into the entropy_expr structure. Fields:
*  type -- the type of the line: relation, diff, Markov, or macro def
*  n    -- total number of entropy items
*  item -- the items; for each item
*    item[].var   -- a bitmap of random variables defining a set
*    item[].coeff -- the coefficient of this entropy term
*/

typedef enum {  /* type of the constrain/expression */
    ent_eq,	/* equal */
    ent_ge,	/* greater than or equal to zero */
    ent_diff,	/* unroll: difference of the two sides */
    ent_Markov,	/* Markov chain constraint */
    ent_mdef,	/* macro definition */
} expr_type_t;

struct entropy_expr_t { /* the expression itself */
    expr_type_t type; /* type */
    int n;	      /* number of items */
    struct {
      int var;
      double coeff;
    } item [minitip_MAX_EXPR_LENGTH];
};

extern struct entropy_expr_t entropy_expr;

/***********************************************************************
* int resize_macro_table(int newsize)
*    resize existing macro table
* int macro_total
*    total number of macros defined including the standard macros
*    H(X), H(X|Y), I(X;Y); I(X;Y|Z)
*/
int resize_macro_table(int newsize);
extern int macro_total;

/***********************************************************************
* Parsing routines
*
*  int parse_entropy(char *str, int keep)
*    parse str as an entropy expression. 
*      keep==1 keep the names of random variables from previous
*               parsing
*      keep==0 start a new set of random variables.
*    Returns
*      PARSE_OK  -- OK, entropy_expr is filled
*      PARSE_ERR -- some error, syntax_error is filled
*      PARSE_EQ  -- simplifies to 0=0
*      PARSE_GE  -- simplifies to 0>=0
*
* int parse_diff(char *str)
*    parse str as unroll (==) string. Return value is PARSE_OK or PARSE_ERR,
*
* int parse_constraint(char *str, int keep)
*    parse str as a constraint: this is an expression, or functional
*    dependency, independence, or a Markov chain. Return value is
*    PARSE_OK or PARSE_ERR
*
* int parse_macro_definition(char *str)
*    parse str as a macro definition. If successful, also stores the
*    macro at the next macro slot. Return value is PARSE_OK or PARSE_ERR.
*
* int parse_conv(char *str,int maxvar)
*    parse str as an optional variable list followed by an expression.
*    maxvar is 4 for natural coords, and 0 otherwise.
*    Return value is PARSE_OK or PARSE_ERR
*/
#define PARSE_OK	0
#define PARSE_ERR	1
#define PARSE_EQ	2	// 0=0
#define PARSE_GE	3	// 0>=0

int parse_entropy(const char *str, int keep);
int parse_diff(const char *str);
int parse_constraint(const char *str, int keep);
int parse_macro_definition(const char *str);
int parse_conv(const char *str, int maxvar);

/***********************************************************************
* Delete a macro
*  int parse_delete_macro(char *str)
*    parse str as a header for a macro to be deleted. Returns
*      -1 -- error, syntax_error is filled
*     >=0 -- the index of the macro with the supplied header
*  void delete_macro_with(idx)
*    delete the macro which is at the given slot.
*/
int parse_delete_macro(const char *str);
void delete_macro_with_idx(int idx);

/***********************************************************************
* Printing in raw format
*    These routines print a linear combination of entropies.
*
*  void print_expression(void)
*    print out the expression in entropy_expr
*  void print_in_natural_coords(void)
*    print the expression in natural coords (assuming 4 variables)
*  void print_in_measures(void)
*    print the expression in measures
*  int print_macros_with_name(char name,int from)
*    print all macros with the given character as name above slot from
*    (standard macros are NOT printed) Returns the number of macros
*    printed (which can be zero).
*  void print_macro_with_idx(int idx)
*    print the macro at the given slot.
*  void dump_macro_with_idx(FILE *to, int idx)
*    print macro definition to a file.
*/
void print_expression(void);
void print_in_natural_coords(void);
void print_in_measures(void);
int print_macros_with_name(char name,int from);
void print_macro_with_idx(int idx);
void dump_macro_with_idx(FILE *to, int idx);

/* EOF */

