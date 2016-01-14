/* parser.h: minitip parser module

   Type: ORIGINAL / SIMPLE
   Variable names:    identifiers             letters a--z
   Set of variables:  comma separated list    sequence of letters
   entropy:           H(<v>)                  <v>
   conditional        H(<v1>|<v2>)            (<v1>|<v2>)
   mutual             I(<v1>;<v2>)            (<v1>,<v2>)
   cond.mutual        I(<v1>;<v2>|<v3>)       (<v1>,<v2>|<v3>)
   Ingleton           [<v1>;<v2>;<v3>,<v4>]   [<v1>,<v2>,<v3>,<v4>]

   Expression:   [+-]<multiplier>[*]<entropy>
   relation:     <expression> [= | >= | <= ] <expression>
   the LHS, RHS can be zero; leading '+' can be omitted
   
   Conditionals as abbreviation:
    functional dependency:    <v1> : <v2>
    independent:              <v1> . <v2> . ...  .<vn>
                              <v1> || <v2> || ... || <vn>
    Markov:                   <v1> / <v2> / ... / <vn>
                              <v1> -> <v2> -> ... -> <vn>
*/

/*=============================================================*/
/* limits */

/* the maximal number of items in an entropy expression
   or in a conditional expression */
#define minitip_MAX_ITEMS	50
/* maximal number of variables handled. It must be at most 31
   as variables are stored as bits in an integer */
#define minitip_MAX_ID_NO	26
/* maximal length of a full entropy identifier */
#define minitip_MAX_ID_LENGTH	25
/* maximal length of a fully expanded entropy expression */
#define minitip_MAX_EXPR_LENGTH	250
/*==============================================================*/
/* set how the string is to be parsed.
   possible values are: */

typedef enum {
    syntax_full,
    syntax_short
} syntax_style_t;

void set_syntax_style(syntax_style_t style);

/*---------------------------------------------------------------*/
/* syntax error; the error string and the position where the
   error has been found.
   Hard error: syntax error
   Soft error: some resource is exhausted (too long, too many
   variables, etc.) */
struct syntax_error_t {
   char *softerrstr, *harderrstr;
   int softerrpos, harderrpos;
};
extern struct syntax_error_t syntax_error;

/*------------------------------------------------------------------*/
/* the parser gives back the entropy expression through this structure */

typedef enum {  /* type of the constrain/expression */
    ent_eq,	/* equal */
    ent_ge,	/* greater than or equal to zero */
    ent_Markov,	/* Markov chain */
} expr_type_t;

struct entropy_expr_t { /* the expression itself */
    expr_type_t type; /* type */
    int n;	      /* number of elements */
    struct {
      int var;
      double coeff;
    } item [minitip_MAX_EXPR_LENGTH];
};

extern struct entropy_expr_t entropy_expr;
/*------------------------------------------------------------------*/

/* parse the string as an entropy expression. or as a condition.
   keep=1: keep the defined variables;
   keep=0: start new set of random variables (FULL mode only)
   Return value:
   0: OK, entropy_expr is filled
   1: some error, syntax_error is filled */
int parse_entropy(char *str, int keep);
int parse_constraint(char *str, int keep);

