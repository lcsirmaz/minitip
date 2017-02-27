/* parser.c: minitip parser module */

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "minitip.h"
#include "parser.h"

/***********************************************************************
* This is a simple look-ahead parser with a backtrack possibility.
* The string to be parsed is in X_str; the actual position we are
* looking at is at X_pos; and the actual character is also stored in
* X_chr. 
*
* When an error is encountered, the actual position and the error message
* is stored in the syntax_error structure. 
*  Hard errors are the syntax errors, and soft errors are violations of
*  bounds (such as variable length, number of items in an expression, etc)
*/

/***********************************************************************
* Variables storing the string to be parsed
*
*   char *X_str   the string to be parsed
*   int   X_pos   the actual position
*   char  X_chr   the character at the X_pos position
*
*/
static const char *X_str; static char X_chr; static int X_pos;

/***********************************************************************
* Error structure, error routines and messages
*
*   syntax_error_t  syntax_error
*    contains the position and the message of the first encountered 
*    error. Hard errors are syntax errors, while soft errors are
*    limit violations (too long identifier, etc).
*    Error messages are constant strings (better case), or a static
*    storage for a special error message.
*
* void softerr(char *err)
*     adds the soft error at the actual position
* void harderr(char *err)
*     adds a hard error at the actual position
* void must(int condition, char *errmessage)
*     if the condition does not hold, issue a hard error. This procedure
*     is defined as a macro and even if it is defined inline, the errmessage
*     argument is evaluated even when the condition is true. The #define
*     form evaluates errmessage only when the condition is false.
*/
struct syntax_error_t syntax_error;
static void softerr(char *err){
    if(!syntax_error.softerrstr){
      syntax_error.softerrstr=err; syntax_error.softerrpos=X_pos; }
}
static void harderr(char *err){
    if(!syntax_error.harderrstr){
      syntax_error.harderrstr=err; syntax_error.harderrpos=X_pos; }
}
static void harderr_show(char *err){
    if(!syntax_error.harderrstr){
      syntax_error.harderrstr=err; syntax_error.harderrpos=X_pos;
      syntax_error.showexpression=1;
    }
}
#define must(cond,err)	\
    { if(!(cond)) harderr(err); }
#define must_show(cond,err)	\
    { if(!(cond)) harderr_show(err); }
inline static void adjust_error_position(int d)
{   syntax_error.softerrpos+=d; syntax_error.harderrpos+=d; }

/***********************************************************************
* soft errors
*/
#define e_TOO_MANY_ID	"too many different random variables (max " mkstringof(minitip_MAX_ID_NO) ")"
#define e_TOO_LONG_ID	"too long identifier (max length=" mkstringof(minitip_MAX_ID_LENGTH) ")"
#define e_TOO_LONG_EXPR	"the expanded expression is too long (max " mkstringof(minitip_MAX_EXPR_LENGTH) ")"
#define e_TOO_MANY_ARGS "too many arguments (max " mkstringof(minitip_MAX_ID_NO) ")"
#define e_TOO_MANY_MACRO "cannot add more macros, the maximum has been reached"
#define e_NO_MEMORY	"out of memory while storing this macro"
/***********************************************************************
* hard errors
*/
#define e_VAR_EXPECTED  "variable is expected after a comma ','"
#define e_GREATER	"> symbol should be followed by ="
#define e_LESS		"< symbol should be followed by ="
#define e_INGLETONVAR	"in 'Ingleton' expression variable list is expected here"
#define e_INGLETONSEP	"in 'Ingleton' expression separator is expected here"
#define e_INGLETONCLOSE	"in 'Ingleton' expression closing ] is missing here"
#define e_CONDEXPR	"( should be followed by a variable list"
#define e_IEXPR2	"variable list is missing after | symbol"
#define e_CLOSING	"closing parenthesis ')' is expected here"
#define e_COMMA_OR_BAR	"either a list separator or '|' is expected here"
#define e_VARLIST	"variable list is expected here"

#define e_PLUSORMINUS	"either '+' or '-' is expected here"
#define e_DOUBLE_REL	"only one relation is allowed in an expression"
#define e_DIFF_USEEQ	"use '==' to separate the expressions"
#define e_WRONGITEM	"unrecognized character"
#define e_NOHOMOGEN	"constant before or after the relation sign must be zero"
#define e_WRONGAST	"multiplication symbol '*' at a wrong place"
#define e_EXTRA_TEXT	"extra characters at the end"
#define e_EMPTY		"no expression is given"
#define e_NORELATION	"there must be an '=', '<=' or '>=' somewhere"
#define e_NORHS		"no expression after '=', '<=' or '>=' "
#define e_DBLEEQ_REL	"use '=', '<=' or '>=' to separate the two sides"
#define e_EXTRANUM	"no constants allowed"
#define e_NOMACRO	"no macro with this name is defined"
#define e_NOMACROARG	"no macro with this name and pattern is defined"
#define e_ID_IN_MACRO	"only macro arguments can be used as variables"
#define e_NO_REL_MACRO	"no relation is allowed in a macro definition"
#define e_MDEF_NAME	"macro definition starts with the macro name followed by '('"
#define e_MDEF_NOPAR	"missing argument: a single variable is expected here"
#define e_MDEF_NOSTD	"standard entropy functions cannot be redefined"
#define e_MDEF_DEFINED	"this type of a macro is defined; delete it first"
#define e_MDEF_SAMEPAR	"all arguments must be different"
#define e_MDEF_PARSEP   "a ')', a list separator, or '|' is expected here"
#define e_MDEF_NOEQ	"macro text should start with an '=' symbol"
#define e_MDEF_SIMP0	"the macro text simplifies to 0, not stored"
#define e_MDEF_UNUSED   "this argument is not used in the final macro text"
#define e_MDEL_NONE	"no match was found. Use 'macro list' to list all macros"
#define e_FUNC_EQUAL	"the first variable set is always a function of other"
#define e_ALLZERO	"all coefficients are zero"
#define e_MARKOV	"a Markov chain must contain at least three tags"
#define e_INTERNAL	"internal error, should not occur..."

#define e_SIMPLIFIES_EQ	"the expression simplifies to '0=0', thus it is always TRUE"
#define e_SIMPLIFIES_GE	"the expression simplifies to '0<=0', thus it is always TRUE"
#define e_POSCOMBINATION "the expression is TRUE as a positive combination of entropy values"
#define e_SINGLE_TERM	"the expression simplifies to a single term, no check is performed"

/***********************************************************************
* dynamic error message; the actual message depends on the argument
*   The first line prevents overwriting the static buffer when issued
*   multiple times, so the first message is kept.
*/
static char* e_FUNCTIONOF(int i){
static char buf1[80],buf2[10]; char *txt;
    if(syntax_error.harderrstr) return "-";
    switch(i){
        case 0: txt="first"; break;
        case 1: txt="second"; break;
        case 3: txt="third"; break;
        default: txt=buf2; sprintf(buf2,"%d-th",i+1);
    }
    sprintf(buf1,"the %s part is a function of the others - cannot be independent",txt);
    return buf1;
}
/***********************************************************************
* Variables storing expression style
*
*   style_t X_style  actual style: SIMPLE or ORIGINAL
*   char    X_sep    the separator character, one of .:;
*   int     X_xvar   allow trailing digits in simple style
*/

static syntax_style_t X_style; static char X_sep;
static int X_xvar;
#define SIMPLE		syntax_short
#define ORIGINAL	syntax_full
void set_syntax_style(syntax_style_t style, char sep, int ext_var){ 
    X_style=style; X_sep=sep; X_xvar=ext_var; }

/***********************************************************************
* Macro expansion and storage routines
*   expanded macro texts are stored in the macro_text[] array.
*   A new definition fills out new_macro, which then can be copied
*   into the main macro storage part.
*
* int resize_macro_table(int newsize)
*   resize the table to the maximum of newsize and macro_total. Return
*   the new size.
* void add_new_macro(macro_head_t head)
*   add the new macro to the list of available macros. The macro body
*   is in entropy_expr
* void delete_macro_with_idx(int idx)
*   delete the macro at the given slot.
*/
struct macro_head_t {
    int argno;		/* number of arguments */
    int septype;	/* separator type, bit i for args i and i+1 */
    char name;		/* macro name, 'A'--'Z' */
};

struct macro_text_t {
    struct macro_head_t head;   /* head */
    struct entropy_expr_t *expr;/* the raw macro body */
};

static int max_macros=0;	/* size of macro table */
int macro_total=0; 		/* total number of macros */

static struct macro_text_t *macro_text=NULL; // [minitip_MAX_MACRONO];

int resize_macro_table(int newsize)
{struct macro_text_t *newtable;
    if(newsize<macro_total) newsize=macro_total;
    newtable=realloc(macro_text,newsize*sizeof(struct macro_text_t));
    if(newtable==NULL){ // no change
        return max_macros;
    }
    max_macros=newsize; macro_text=newtable;
    return max_macros;
}
inline static void add_new_macro(struct macro_head_t head)
{int i; struct entropy_expr_t *e;
    if(syntax_error.softerrstr|| syntax_error.harderrstr) return;
    if((e = malloc(sizeof(struct entropy_expr_t)))==NULL){
         softerr(e_NO_MEMORY);
         return;
    }
    macro_text[macro_total].head=head;
    macro_text[macro_total].expr = e;
    e->n = entropy_expr.n;
    e->type = ent_mdef;
    for(i=0;i<entropy_expr.n;i++){
        e->item[i] = entropy_expr.item[i];
    }
    if(macro_total<max_macros) macro_total++;
}

void delete_macro_with_idx(int idx)
{   if(idx<0 || macro_total<=idx) return;
    free(macro_text[idx].expr);
    macro_total--;
    while(idx<macro_total){
        macro_text[idx]=macro_text[idx+1];
        idx++;
    }
}
/***********************************************************************
* int find_macro(macro_head_t head, int partial)
*   search among macros matching the specification in head:
*    head.name:   - macro name, 'A'--'Z'
*    head.argno   - argument number to search for (can be smaller
*                     than the actual argument number)
*    head.septype - separator types up to head.argno
*
*    partial      - 0 find exact match
*                 - 1 find partial match with last separator ;
*                 - 2 find partial match with last separator |
*
*    return value: >=0 the macro's index; -1: no macro was found
*/
static int find_macro(struct macro_head_t head, int partial)
{int idx;
    if(partial){
        int mask=(1<<head.argno)-1;
        int type=head.septype; if(partial==2) type |= 1<<(head.argno-1);
        for(idx=0;idx<macro_total;idx++){
            if(macro_text[idx].head.name == head.name &&
               macro_text[idx].head.argno > head.argno &&
               (macro_text[idx].head.septype&mask)==type ) return idx;
        }
        return -1;
    } /* ask for exact match */
    for(idx=0;idx<macro_total;idx++){
        if(macro_text[idx].head.name==head.name &&
           macro_text[idx].head.argno == head.argno &&
           macro_text[idx].head.septype == head.septype) return idx;
    }
    return -1;
}
/***********************************************************************
* int var_merge(int what, int from[] )
*    in macro invocation, what describes the parameters set which should
*    be merged; the appropriate values are from the from[] array.
*/
static int var_merge(int what, int from[])
{int i; int v;
    v=0;
    for(i=0;what;i++,what>>=1){
        if(what&1) v |= from[i];
    }
    must(v,e_INTERNAL);
    return v;
}
/***********************************************************************
* Identifier handling
*  struct id_table[]        contains all identifiers found so far
*  int id_table_idx         is the next empty slot
*  char *no_new_id_str
*     if not NULL, instead of creating a new variable slot, this 
*     hard error message is issued. Used when defining the macro text.
*
*  int search_id(char *var)
*     returns the index of the identifier if found;
*     adds to the table if not found and the table is not full yet,
*     issues a soft error otherwise
*
*  char *get_idlist_repr(int v, int slot)
*     Using v as a collection of random variables (taken as a bitmap),
*     the textual representation of the random variables list is stored
*     in one of two static buffers (slot==1 and slot==2). Uses ',' as
*     separation for ORIGINAL style. Restricts the length of the lists
*     to MAX_REPR_LENGTH (defined below). Too long list is illegible.
*/

struct { char id[minitip_MAX_ID_LENGTH+1]; }id_table [minitip_MAX_ID_NO+1];
static int id_table_idx=0; /* next empty slot */
static char *no_new_id_str=NULL; /* add no new id */

static int search_id(const char *var)
{int i;
    for(i=0;i<id_table_idx;i++){
        if(strcmp(id_table[i].id,var)==0) return i;
    }
    if(id_table_idx>0 && no_new_id_str){
        harderr(no_new_id_str);
        return id_table_idx-1;
    }
    id_table_idx++;
    if(id_table_idx>=minitip_MAX_ID_NO){
        softerr(e_TOO_MANY_ID);
        id_table_idx--;
    } else {
        strncpy(id_table[id_table_idx-1].id,var,minitip_MAX_ID_LENGTH);
    }
    return id_table_idx-1;
}
inline static void no_new_id(char *str)
{   no_new_id_str=str; }

#define MAX_REPR_LENGTH 201	/* longer list is not understandable; >= 26 */
static char *get_idlist_repr(int v, int slotno)
{static char slot1[MAX_REPR_LENGTH+2], slot2[MAX_REPR_LENGTH+2];
const char *unsorted[minitip_MAX_ID_NO+1];
 char *slot; const char *var; int i,j,n;
    for(i=0,n=0;v!=0;i++,v>>=1)if(v&1){
        unsorted[n]=i<id_table_idx ? id_table[i].id : "?"; 
        n++;
    }
    j=1; // sort the variables
    while(j){
        j=0;
        for(i=0;i<n-1;i++){
            if(strcmp(unsorted[i],unsorted[i+1])>0 ){
                var=unsorted[i]; unsorted[i]=unsorted[i+1]; unsorted[i+1]=var;
                j=1;         
            }
        }
    }
    slot= slotno==1 ? slot1 : slot2;
    for(i=0,j=0;i<n;i++){
        if(j>0 && X_style==ORIGINAL){ 
            slot[j]=','; /* comma separated */
            if(j<MAX_REPR_LENGTH) j++;
        }
        for(var=unsorted[i];*var;var++){
            slot[j]=*var;
            if(j<MAX_REPR_LENGTH)j++;
        }
    }
    slot[j]=0; return slot;
}

/***********************************************************************
* A parsed item is put into the ITEM structure. It is either en entropy
*     expression or a macro invocation. Then it is converted into the
*     final form stored in the struct entropy_expr.
*  item_type   item type, defined in the item_type_t enum
*  multiplier  the multiplier of this item
*  vars[]      when a macro invocation, arguments if that macro
*  var1,var2,var3,var4
*              further arguments depending on the type
*/
typedef enum {
   equal,	/* = */
   greater,	/* >= */
   less,	/* <= */
   diff,	/* == */
   zero,	/* 0.0 */
   Macro,	/* macro invocation */
   H1, 		/* entropy of a single variable */
   H2,		/* (a|b) */
   I2,		/* (a,b) */
   I3,		/* (a,b|c)*/
   Ing,		/* [a,b,c,d], Ingleton */
   Func,	/* a : b */
   Indep,	/* a || b || c ... */
   Markov	/* a -> b -> c -> ... */
} item_type_t;

typedef struct {	/* item structure */
  item_type_t	item_type;
  double	multiplier;
  int           vars[minitip_MAX_ID_NO+1];
  int		var1, var2, var3, var4;
} ITEM;

static ITEM item;

/***********************************************************************
* struct entropy_expr_t entropy_expr
*   the collected entropy expression. Each entry is an entropy of a
*   collection of variables which is stored as a bitmap, plus its
*   coefficient.
*
*   The convenience macros ee_n, ee_item, ee_type are defined to denote
*   fields of the entropy_expr structure in this section.
*/
struct entropy_expr_t entropy_expr;

#define ee_n	entropy_expr.n
#define ee_item	entropy_expr.item
#define ee_type entropy_expr.type

/***********************************************************************
* int ee_varidx(var)
*    find the variable list in entropy_expr, or add a new 
*    entry if not found.
*/
static int ee_varidx(int var)
{int i;
    for(i=0;i<ee_n;i++) if(var==ee_item[i].var) return i;
    ee_n++; // i==ee_n
    if(ee_n>=minitip_MAX_EXPR_LENGTH){
        softerr(e_TOO_LONG_EXPR);
        ee_n--; i--;
    }
    ee_item[i].var=var; ee_item[i].coeff=0.0;
    return i;
}
/***********************************************************************
* increase / decrease the coefficient of a variable or a certain 
*    entropy expression by a given value d
*  ee_add(var,d)           add d to coeff of var
*  ee_subtr(var,d)         subtract d from the coeff of var
*  ee_i2(var1,var2,d)      add d* I(var1;var2)
*  ee_i3(var1,va2,var3,d)  add d*I(var1;var2|var3)
*/
#define ee_add(v,d)	\
    ee_item[ee_varidx(v)].coeff += (d)
#define ee_subtr(v,d)	\
    ee_item[ee_varidx(v)].coeff -= (d)
inline static void ee_i2(int v1,int v2,double d){
    ee_add(v1,d); ee_add(v2,d); ee_subtr(v1|v2,d);
}
inline static void ee_i3(int v1,int v2,int v3, double d){
    ee_add(v1|v3,d);ee_add(v2|v3,d);ee_subtr(v3,d);ee_subtr(v1|v2|v3,d);
}
/***********************************************************************
* Convert an item into a linear combination of entropies
*
*  int ee_negate
*    if set, subtract, rather than add the entropy item to the
*    accumulated result. It should be cleared initially, and
*    is set when the relation symbol =, >=, or == is found.
*
* void clear_entexpr(void)
*    should be called at the start of parsing. Clears entropy_expr
*    as well as the ee_negate flag.
*
*  void convert_item_to_expr(void)
*    given the item stored in ITEM, add it to the entropy expression
*    in entropy_expr. Constraints are handled separately
*/
static int ee_negate;

inline static void clear_entexpr(void)
{ee_negate=0; ee_n=0;}

static void convert_item_to_expr(void)
{double d; int j; struct entropy_expr_t *M;
    if(syntax_error.softerrstr || syntax_error.harderrstr) return;
    d=item.multiplier; if(ee_negate) d=-d;
    switch(item.item_type){
  case zero:	/* skip, do nothing */
        break;
  case equal:	/* negate subsequent terms */
       ee_negate=1; ee_type=ent_eq; break;
  case less:	/* negate existing terms */
       for(j=0;j<ee_n;j++){ ee_item[j].coeff *= -1.0; }
       ee_type=ent_ge; break;
  case greater:	/* negate subsequent terms */
       ee_negate=1; ee_type=ent_ge; break;
  case diff:    /* difference of the two sides */
       ee_negate=1; ee_type=ent_diff; break;
  case H1:	/* entropy of a single variable */
       ee_add(item.var1,d); break;
  case H2:      /* (a|b) */
       ee_subtr(item.var2,d);
       ee_add(item.var1|item.var2,d); break;
  case I2:      /* (a,b) */
       ee_i2(item.var1,item.var2,d); break;
  case I3:      /* (a,b|c) */
       ee_i3(item.var1,item.var2,item.var3,d); break;
  case Ing:
       ee_i2(item.var1,item.var2,-d);
       ee_i3(item.var1,item.var2,item.var3,d);
       ee_i3(item.var1,item.var2,item.var4,d);
       ee_i2(item.var3,item.var4,d);
       break;
  case Macro:   /* M(ab,bc,cd) */
       M = macro_text[item.var1].expr;
       for(j=0;j<M->n;j++){
           ee_add(var_merge(M->item[j].var,item.vars),d*M->item[j].coeff);
       }
       break;
  case Func:	/* v1 : v2 */
      ee_type=ent_eq;
      ee_n=2;
      ee_item[0].var=item.var1;
      ee_item[1].var=item.var2;
      ee_item[0].coeff=+1.0;
      ee_item[1].coeff=-1.0;
      break;
  case Indep:	/* v1 || v2 */
      ee_type=ent_eq;
      ee_n=2;
      ee_item[0].var=item.var1;
      ee_item[1].var=item.var2;
      ee_item[0].coeff=+1.0;
      ee_item[1].coeff=+1.0;
      break;
  case Markov:  /* v1 -> */
      ee_type=ent_Markov;
      ee_item[ee_n].var=item.var1;
      ee_item[ee_n].coeff=+1.0;
      ee_n++;
      break;
  default:
       must(0,e_INTERNAL);
    }
}

/***********************************************************************
* void collapse_expr(void)
*    go over the collected expression and leave out entries with
*    (almost equal to) zero coefficient.
*/
static void collapse_expr(void)
{int i,j;
    for(i=0,j=0;j<ee_n;j++){
        if(ee_item[j].coeff>1.5e-10 || ee_item[j].coeff<-1.5e-10){
            if(i!=j){ee_item[i]=ee_item[j]; }
            i++;
        } else {
           ee_item[j].coeff=0.0;
        }
    }
    ee_n=i;
}

/***********************************************************************
* int not_trivial_expr(void)
*    return 0 (no) if checking >=0 and all coefficients are positive.
*/
static int non_trivial_expr(void)
{int i;
    if(ee_type!=ent_ge) return 1; /* maybe we check for a=0 */
    for(i=0;i<ee_n;i++) if(ee_item[i].coeff < 0.0) return 1;
    return 0;
}

/***********************************************************************
* int first_variable_not_used(void)
*    return the index of the first variable not occurring in 
*    entropy_expr; or -1 if there was an error
*/
static int first_variable_not_used(void)
{int i,v;
    if(syntax_error.softerrstr|| syntax_error.harderrstr) return -1;
    v=0;
    for(i=0;i<ee_n;i++) v |= ee_item[i].var;
    for(i=0;(v&1);i++,v>>=1);
    return i;
}

/***********************************************************************
* void print_expression(void)
*    print out the expression collected in 'entropy_expr' using the
*    actual style. Sort variable lists first by their length, next
*    alphanumerically by their printout representation.
*
* void print_macro_with_idx(int idx)
*    print out the macro at slot idx
*
* int print_macros_with_name(char name, int from)
*    print all macros with the given name above the slot from
*    return the number of lines printed.
* int bitno(int v)
*    returns the number of bits set in v.
*
* void sort_expr_by_variables(void)
*    using bubble sort reshuffle the expression terms.
*/
inline static int bitno(int v)
{int i;
    for(i=0;v;v>>=1){
        if(v&1)i++;
    }
    return i;
}
/* use bubble sort to sort the expression in ee_item by variables */
static void sort_expr_by_variables(void)
{int i,working; int v1,v2; double cf;
    working=1;
    while(working){
        working=0;
        for(i=0;i<ee_n-1;i++){
            v1=ee_item[i].var; v2=ee_item[i+1].var;
            if(bitno(v1)>bitno(v2) ||
               (bitno(v1)==bitno(v2) &&
                  strcmp(get_idlist_repr(v1,1),get_idlist_repr(v2,2))>0) ){
                working=1;
                ee_item[i].var=v2; ee_item[i+1].var=v1;
                cf=ee_item[i].coeff; ee_item[i].coeff=ee_item[i+1].coeff; ee_item[i+1].coeff=cf;
            }
        }
    }
}
/* dump the expression in ee_item */
static void dump_expression(FILE *to)
{int i; double d;
    if(ee_n<=0){ printf("0"); return; }
    sort_expr_by_variables();
    for(i=0;i<ee_n;i++){
        d=ee_item[i].coeff;
        if(d<1.0+1e-9 && d>1.0-1e-9){ fprintf(to,"+"); }
        else if(d<-1.0+1e-9 && d>-1.0-1e-9){ fprintf(to,"-"); }
        else {fprintf(to,"%+lg",d); }
        if(X_style==ORIGINAL){
            fprintf(to,"H(%s)",get_idlist_repr(ee_item[i].var,1));
        } else {
            fprintf(to,"%s",get_idlist_repr(ee_item[i].var,1));
        }
    }
}
/* print out the expression in ee_item */
void print_expression(void)
{int i; double d;
    if(ee_n<=0){ printf("0"); return; }
    sort_expr_by_variables();
    for(i=0;i<ee_n;i++){
        d=ee_item[i].coeff;
        if(d<1.0+1e-9 && d>1.0-1e-9){ printf("+"); }
        else if(d<-1.0+1e-9 && d>-1.0-1e-9){ printf("-"); }
        else {printf("%+lg",d); }
        if(X_style==ORIGINAL){
            printf("H(%s)",get_idlist_repr(ee_item[i].var,1));
        } else {
            printf("%s",get_idlist_repr(ee_item[i].var,1));
        }
    }
}
/* dump a macro */
void dump_macro_with_idx(FILE *to, int idx)
{int i,v; int septype,varno; char varstr[2];
    septype=macro_text[idx].head.septype;
    varno=macro_text[idx].head.argno;
    no_new_id(NULL);
    id_table_idx=0;
    fprintf(to,"macro %c(",macro_text[idx].head.name);
    for(v=0;v<varno;v++){ /* add variables to be printed */
        varstr[0]=v+(X_style==ORIGINAL ? 'A' : 'a');
        varstr[1]=0;
        search_id(varstr);
        fprintf(to,"%s%c",varstr,v<varno-1?((septype&1)?'|':X_sep):')' );
        septype>>=1;
    }
    fprintf(to," = ");
    entropy_expr.n=macro_text[idx].expr->n;
    for(i=0;i<entropy_expr.n;i++){
       entropy_expr.item[i]=macro_text[idx].expr->item[i];
    }
    dump_expression(to);
    fprintf(to,"\n");
}
/* print out a macro */
void print_macro_with_idx(int idx)
{int i,v; int septype,varno; char varstr[2];
    septype=macro_text[idx].head.septype;
    varno=macro_text[idx].head.argno;
    no_new_id(NULL);
    id_table_idx=0;
    printf(" macro %c(",macro_text[idx].head.name);
    for(v=0;v<varno;v++){ /* add variables to be printed */
        varstr[0]=v+(X_style==ORIGINAL ? 'A' : 'a');
        varstr[1]=0;
        search_id(varstr);
        printf("%s%c",varstr,v<varno-1?((septype&1)?'|':X_sep):')' );
        septype>>=1;
    }
    printf(" = ");
    entropy_expr.n=macro_text[idx].expr->n;
    for(i=0;i<entropy_expr.n;i++){
       entropy_expr.item[i]=macro_text[idx].expr->item[i];
    }
    print_expression();
    printf("\n");
}    
/* print out a macro definition */
int print_macros_with_name(char name, int from)
{int idx,printed_no;
    printed_no=0;
    for(idx=from;idx<macro_total;idx++)if(macro_text[idx].head.name==name){
        print_macro_with_idx(idx);
        printed_no++;
    }
    return printed_no;
}

#undef ee_n
#undef ee_item
#undef ee_type

/***********************************************************************
* Lookahead-one parsing routines
*  next_chr()    advances the position to the next visible char
*  next idchr()  advance to the next char; used when parsing identifiers
*  skip_to_visible()
*                skip to the next visible char
*  restore_pos() restore the old (saved) position
*  init_parse(str)
*                sets str to be parsed; erases errors, but keeps stored
*                variables (needed when parsing constraints)
*  R(c)          true, if the next char is c, and advances
*  spy(c)        true if the next char is c, but does not advance
*  is_digit(&v)  true if the next char is digit, store the value
*  is_number(&v) always sets v; true and advances if it is a decimal number
*  is_signed_number(&v) always sets v; reads a signed number
*  is_variable(&v) read the next variable into v
*  is_varlist(&v) reads a list of variables into v
*  is_relation(&v) checks if this is one of '=', '<='. '>=', or '=='
*  is_Ingletion() stores an Ingleton form in item
*  is_simple_expression()
                  stores an entropy expression in item
*/
/* next_chr()  -- advance to the next visible char */
static inline void next_chr(void)
{  X_pos++; while((X_chr=X_str[X_pos])==' '|| X_chr=='\t') X_pos++; }
/* next_idchr() --  advance to the next character */
static inline void next_idchr(void)
{  X_pos++; X_chr=X_str[X_pos]; }
/* skip_to_visible() -- skip to the next visible char */
static inline void skip_to_visible(void)
{ while((X_chr=X_str[X_pos])==' '||X_chr=='\t') X_pos++; }
/* restore_pos(oldpos) -- restore the old position (backtrack) */
static inline void restore_pos(int oldpos)
{  X_pos=oldpos; X_chr=X_str[X_pos]; }
/* init_parse() - initialize parsing */
inline static void init_parse(const char *str){
    X_str=str; X_pos=-1; next_chr();
    syntax_error.softerrstr=NULL; 
    syntax_error.harderrstr=NULL;
    syntax_error.showexpression=0; /* no errors yet */
}
/* R(c) -- check if the next symbol is c; if yes advance */
static inline int R(char c){
    if(X_chr==c){ next_chr(); return 1;}
    return 0;
}
/* spy(c) -- check if the next symbol is c; but don't advance */
// static inline int spy(char c){ return X_chr==c; }
#define spy(c)	X_chr==(c)
/* is_digit(&d) -- if the next symbol is digit, store in d and advance */
static inline int is_digit(int *v){
    if('0'<= X_chr && X_chr <='9'){ *v=X_chr-'0';  next_chr(); return 1;}
    return 0;
}
/*----------------------------------------------------------------------
* int frac_part(double *v)
*    if a fraction part comes, add its value to v; v is assumed to have
*    an initial values set. Advance the sequence of digits.
*    This routine parses the regexp   ([.]\d+)
*/
static int frac_part(double *v)
{int i,oldpos; double scale;
    oldpos=X_pos;
    if(R('.') && is_digit(&i)){
        scale=0.1; *v += scale*i;
        while(is_digit(&i)){
            scale *=0.1; *v += scale*i;
        }
        return 1;
    }
    /* period followed by not a digit */
    restore_pos(oldpos);
    return 0;
}
/*----------------------------------------------------------------------
* int is_number(double *v)
*   return 1 if a decimal number with a possible fractional part comes.
*   The routine parses the regexp  (\d+) | (\d+[.]\d+) | ([.]\d+)
*   Always sets *v.
*/
static int is_number(double *v)
{int i;
    if(is_digit(&i)){
        *v=(double)i;
        while(is_digit(&i)){ *v=(*v)*10.0+(double)i; }
        frac_part(v);
        return 1;
    }
    *v=0.0;
    return frac_part(v);
}
/*----------------------------------------------------------------------
* int is_signed number(double *v)
*    signed number: a sign, a number, or a sign followed by a number
*    The argument is set even if returns false.
*    It parses the regexp: +<number> | -<number> | <number> | +
*    where <number> is parsed by is_number() above.
*    For a single + (without a subsequent number) the value of v is 1.0
*/
static int is_signed_number(double *v)
{    if(R('+')){
         if(!is_number(v)) {*v=1.0; }
         return 1;
     }
     if(R('-')){
         if(!is_number(v)){ *v=1.0; }
         *v = -(*v);
         return 1;
     }
     return is_number(v);
}
/*----------------------------------------------------------------------
* int is_variable(int *v)
*    parses an identifier and returns the bit as found by search(id)
*    For SIMPLE an identifier is  [a-z][']*
*    For ORIGINAL style, an identifier is [a-zA-Z]<letgit>*\'*
*       where letgit contains the underscore as well.
*/
inline static int spy_letgit(void){
    return ('a'<=X_chr && X_chr<='z') ||
           ('A'<=X_chr && X_chr<='Z') ||
           ('0'<=X_chr && X_chr<='9') || X_chr=='_'; }
static int is_variable(int *v)
{char var[minitip_MAX_ID_LENGTH+1]; int i;
    *v=0; i=-1;
    if(X_style==ORIGINAL){
        if( ('a'<= X_chr && X_chr <='z') ||
            ('A'<= X_chr && X_chr <='Z')){
            var[0]=X_chr; i=0; next_idchr();
            while( spy_letgit() ){
                i++; if(i>=minitip_MAX_ID_LENGTH-1){
                    i--; softerr(e_TOO_LONG_ID);
                } else {var[i]=X_chr; }
                next_idchr();
            }
        }
    } else { // X_style == SIMPLE
        if('a'<=X_chr && X_chr<='z'){
            var[0]=X_chr; i=0; next_idchr();
            while(X_xvar==2 && '0'<=X_chr && X_chr<='9'){
                i++; if(i>=minitip_MAX_ID_LENGTH-1){
                    i--; softerr(e_TOO_LONG_ID);
                } else { var[i]=X_chr; }
                next_idchr();
            }
        }
    }
    if(i<0) return 0;
    while(X_chr=='\''){
       i++; if(i>=minitip_MAX_ID_LENGTH){
           i--; softerr(e_TOO_LONG_ID);
       } else {var[i]=X_chr; }
       next_idchr();
    }
    i++; var[i]=0;
    skip_to_visible();
    *v = 1<< search_id(var);
    return 1;
}
/*----------------------------------------------------------------------
* int is_varlist(int *v)
*    a list of variables, the bitmap is stored in v.
*    variables concatenated for SIMPLE style;
*    comma separated for ORIGINAL style.
*/
static int is_varlist(int *v)
{int j;
    if(is_variable(v)){
        if(X_style==SIMPLE){
            while(is_variable(&j)){ *v |= j; }
        } else { // ORIGINAL
            while(R(',')){
                must(is_variable(&j),e_VAR_EXPECTED);
                *v |= j;
            }
        }
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------
* int is_relation(item_type_t *v)
*    check if the next symbol is =, <=, >=, or ==
*/
static int is_relation(item_type_t *v){
    if(R('=')){
        *v = equal;
        if(R('=')) *v = diff;
        return 1; 
    }
    if(R('>')){ must(R('='),e_GREATER); *v=greater; return 1; }
    if(R('<')){ must(R('='),e_LESS);    *v=less; return 1; }
    return 0;
}
/*----------------------------------------------------------------------
* int is_macro_name(char *name)
*    a macro name is a capital letter
*/
static int is_macro_name(char *v){
    if('A'<=X_chr && X_chr <= 'Z'){
        *v=X_chr; next_chr();
        return 1;
    }
    return 0;
}

/***********************************************************************
* Parsing entropy items
*    parse the next entropy item and store it in the ITEM structure
*  int is_Ingleton            [a,b,c,d]
*  int is_par_expression      (a,b) (a|b) (a,b|c)
*  int is_simple_expression   a a,b a|b a,b|c
*  int is_macro_invocation    X(list1,list2|listn)
*
*  int expect_oneof(char c1,c2,c3)
*    arguments are either zero or a character; expect one of them
*    issue meaningful error message
*/
static int is_Ingleton(void)
{   if(R('[')){
        item.item_type=Ing;
        must(is_varlist(&item.var1),e_INGLETONVAR);
        must(R(X_sep),e_INGLETONSEP);
        must(is_varlist(&item.var2),e_INGLETONVAR);
        must(R(X_sep),e_INGLETONSEP);
        must(is_varlist(&item.var3),e_INGLETONVAR);
        must(R(X_sep),e_INGLETONSEP);
        must(is_varlist(&item.var4),e_INGLETONVAR);
        must(R(']'),e_INGLETONCLOSE);
        return 1;
    }
    return 0;
}
static int is_par_expression(void)
{   if(R('(')){ /* (a,b)  (a|b)  (a,b|c) */
        must(is_varlist(&item.var1),e_CONDEXPR);
        if(R('|')){
            item.item_type=H2;
            must(is_varlist(&item.var2),e_IEXPR2);
            must(R(')'),e_CLOSING);
        } else {
            item.item_type=I2;
            must(R(X_sep),e_COMMA_OR_BAR);
            must(is_varlist(&item.var2),e_VARLIST);
            if(R('|')){
                item.item_type=I3;
                must(is_varlist(&item.var3),e_VARLIST);
            }
            must(R(')'),e_CLOSING);
        }
        return 1;
    }
//    if(is_varlist(&item.var1)){
//        item.item_type=H1;
//        return 1;
//    }
    return 0;
}
static int is_simple_expression(void)
{   if(is_varlist(&item.var1)){ /* a a,b a|b a,b|c */
        item.item_type=H1;
        if(R('|')){
            item.item_type=H2;
            must(is_varlist(&item.var2),e_IEXPR2);
        } else if(R(X_sep)){
            item.item_type=I2;
            must(is_varlist(&item.var2),e_VARLIST);
            if(R('|')){
                item.item_type=I3;
                must(is_varlist(&item.var3),e_VARLIST);
            }
        }
        return 1;
    }
    return 0;
}
/* expect one of the three chars/zero, issue the correct error message */
static int expect_oneof(char c1, char c2, char c3)
{int n; static char buff[80];
    if(c1 && R(c1)) return 1;
    if(c2 && R(c2)) return 2;
    if(c3 && R(c3)) return 3;
    if(syntax_error.harderrstr) return 0;
    n=0; if(c1)n++; if(c2)n++; if(c3)n++;
    if(n==1){ sprintf(buff,"symbol %c is expected here",c1|c2|c3); }
    /**     c1==0:  c2   c3
            c2==0   c1   c3
            c3==0   c1   c2  */
    else if(n==2){ sprintf(buff,"either '%c' or '%c' is expected here",c1==0?c2:c1,c3==0?c2:c3); }
    else { sprintf(buff,"one of %c, %c or %c is expected here",c1,c2,c3); }
    must(0,buff);
    return 0;
}
static int is_macro_invocation(void)
{int oldpos; struct macro_head_t head; int macrono; int done;
    oldpos=X_pos;
    if(is_macro_name(&head.name) && R('(')){
        head.argno=0; head.septype=0;
        must(find_macro(head,1)>=0,e_NOMACRO);
        for(done=0; !done; ){
            must(is_varlist(&item.vars[head.argno]),head.argno==0 ? e_CONDEXPR : e_VARLIST);
            if(head.argno<minitip_MAX_ID_NO) head.argno++;
            /* no need to give an error as no such a macro exists */
            /* figure out the next symbol */
            switch(expect_oneof(
                find_macro(head,0) >=0 ? ')' : 0,
                find_macro(head,1) >=0 ? X_sep : 0,
                find_macro(head,2) >=0 ? '|' : 0)){
                case 3:  head.septype |= 1<<(head.argno-1); break;
                case 2:  break; /* sep */
                case 1:  done=1; break; /* ) */
                default: done=1; break; /* error */
            }
        }
        macrono=find_macro(head,0);
        must(macrono>=0,e_NOMACROARG);
        item.item_type=Macro;
        item.var1=macrono;
        return 1;
    }
    restore_pos(oldpos);
    return 0;
}
/***********************************************************************
* void parse_entropyexpr(char *str, int keep, int etype)
*   parse the entropy expression passed in the argument str
*   keep: if !=0, keep variables defined so far; otherwise clear
*   etype: which format the expression should have (see below)
*/
enum {
  expr_check, 	/* check or constraint */
  expr_diff, 	/* diff (zap) */
  expr_macro,	/* macro definition */
};

#define W_start 0	/* we are at the start */
#define W_bexpr 1	/* after entropy expression before relation */
#define W_rel   2	/* just after =, >=, <= */
#define W_aexpr 3	/* after entropy expression after a relation */
static void parse_entropyexpr(const char *str, int keep, int etype)
{int where;             /* where we are */
 double coeff;          /* the coeff for the next item */
 int iscoeff; item_type_t relsym;
    clear_entexpr();    /* clear the result space */
    no_new_id(etype==expr_macro ? e_ID_IN_MACRO : NULL);
    if(!keep) id_table_idx=0; /* don't keep identifiers */
    where=W_start;      /* now we are in an initial position */
    init_parse(str);    /* no errors yet */
    while(X_chr && !syntax_error.harderrstr){ /* go until the first error only */
       if(where != W_start && is_relation(&relsym) ){
           must(where==W_bexpr,e_DOUBLE_REL);
           where=W_rel;
           switch(etype){
             case expr_diff:  must(relsym==diff,e_DIFF_USEEQ); break;
             case expr_macro: harderr(e_NO_REL_MACRO); break;
             case expr_check: must(relsym!=diff,e_DBLEEQ_REL); break;
             default:         /* OK */ break;
           }
           item.item_type=relsym;
           convert_item_to_expr();
           continue;
       }
       if(where!=W_start && where !=W_rel){
           must(spy('+')||spy('-'),e_PLUSORMINUS);
       }
       iscoeff=0; if(is_signed_number(&coeff)){
          iscoeff=1;
          if(R('*')) iscoeff=2;
       } else { coeff=1.0; }
       if((X_style==SIMPLE && is_simple_expression()) ||
          (X_style==SIMPLE && is_par_expression()) ||
          is_Ingleton() || is_macro_invocation()){
          item.multiplier=coeff;
          if(where==W_start) where=W_bexpr;
          if(where==W_rel) where=W_aexpr;
          if(coeff>1.5e-10 || coeff<-1.5e-10) convert_item_to_expr();
          continue;
       }
       must(iscoeff!=2,e_WRONGAST);
       if(where==W_start){
          /* we must see a single zero followed by a relation */
          must(etype!=expr_macro,e_WRONGITEM);
          must(iscoeff,e_WRONGITEM);
          must(is_relation(&relsym),e_WRONGITEM);
          if(etype==expr_diff) must(relsym==diff,e_DIFF_USEEQ);
          if(etype==expr_check)must(relsym!=diff,e_DBLEEQ_REL);
          must(coeff==0.0,e_NOHOMOGEN);
          item.item_type=zero;
          convert_item_to_expr();
          item.item_type=relsym;
          convert_item_to_expr();
          where=W_rel;
          continue;
       }
       if(where==W_rel){ // we must see a single zero, that's all
          must(iscoeff,e_WRONGITEM);
          must(coeff==0.0,e_NOHOMOGEN);
          item.item_type=zero;
          convert_item_to_expr();
          where=W_aexpr; // and this must be the last
          must(spy(0),e_EXTRA_TEXT);
          continue;
       }
       must(iscoeff==0,e_EXTRANUM);
       must(X_chr==0,e_EXTRA_TEXT);
    }
    no_new_id(NULL);     // allow identifiers to be defined
    must(where!=W_start,e_EMPTY);
    must(etype==expr_macro || where!=W_bexpr,e_NORELATION);
    must(etype!=expr_check || where!=W_rel, e_NORHS);
    must(entropy_expr.n>0,e_ALLZERO); // only zero coeffs were used 
    collapse_expr();     // forget zero coeffs */
}
#undef W_start
#undef W_bexpr
#undef W_rel
#undef W_aexpr

/***********************************************************************
* int parse_entropy(char *str, int keep)
*   parse the entropy expression passed in str
*   keep: if !=0, keep variables defined so far; otherwise clear
*   return value: 1 -- there was an error; 0 -- otherwise
*
* int parse_diff(char *str)
*   parse a diff (two expressions separated by == )
*/

int parse_entropy (const char *str, int keep)
{   parse_entropyexpr(str,keep,expr_check);
    if(entropy_expr.n==0){
        harderr(entropy_expr.type==ent_eq? e_SIMPLIFIES_EQ : e_SIMPLIFIES_GE);
    }
    must_show(non_trivial_expr(),e_POSCOMBINATION);
    must_show(entropy_expr.n>1,e_SINGLE_TERM);
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}

int parse_diff(const char *str)
{   parse_entropyexpr(str,0,expr_diff);
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}

/***********************************************************************
* parse a constraint. It can be a RELATION, or one of the following:
*  a)  v1 : v2         the first is a function of the second
*  b)  v1 . v2 . ...   these are completely independent
       v || v2 || ...
*  c)  v1 / v2 / ...   they form a Markov chain (at least three)
       v1 -> v2 -> ...
*
* int is_funcdep(int v1,int v2)
*   parse and store the first type of constraint
*
* int is_indep(int v1,int v2)
*   parse and store the second type of constraint
*
* int is_Markov(int v1,int v2)
*   parse and store the third type of constraint
*
* int parse_constraint(char *str, int keep)
*    parse a constraint and put the result into  entropy_expr
*/
static int is_funcdep(int v1,int v2){
    item.item_type=Func;
    v1|=v2;
    must(v1!=v2,e_FUNC_EQUAL);
    item.var1=v1;
    item.var2=v2;
    convert_item_to_expr();
    must(X_chr==0,e_EXTRA_TEXT);
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}
static int is_indep(char sep, int v1,int v2)
{int v,vall; int oldpos; int i,j;
    item.item_type=Indep; item.var1=v1; item.var2=v2;
    convert_item_to_expr();
    vall=v1|v2;
    if(sep=='.'){
        while(oldpos=X_pos,(R('.')&& is_varlist(&v))){
            item.item_type=H1; item.var1=v; item.multiplier=+1.0;
            vall |= v;
            convert_item_to_expr();
        }
        restore_pos(oldpos);
    } else {
        while(oldpos=X_pos,(R('|')&& R('|')&& is_varlist(&v))){
            item.item_type=H1; item.var1=v; item.multiplier=+1.0;
            vall |= v;
            convert_item_to_expr();
        }
        restore_pos(oldpos);
    }
    must(X_chr==0,e_EXTRA_TEXT);
    // check if none is a subset of the others
    for(i=0;i<entropy_expr.n;i++){
        v=0;
        for(j=0;j<entropy_expr.n;j++)
            if(i!=j) v |= entropy_expr.item[j].var;
        must(v!=vall,e_FUNCTIONOF(i));
    }
    item.item_type=H1; item.var1=vall; item.multiplier=-1.0;
    convert_item_to_expr();
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}
static int is_Markov(char sep, int v1,int v2)
{int v,cnt; int oldpos;
    item.item_type=Markov; item.var1=v1; convert_item_to_expr();
    item.item_type=Markov; item.var1=v2; convert_item_to_expr();
    cnt=2;
    if(sep=='/'){
        while(oldpos=X_pos,(R('/') && is_varlist(&v))){
            item.item_type=Markov; item.var1=v; convert_item_to_expr();
            cnt++;
        }
        restore_pos(oldpos);
    } else {
        while(oldpos=X_pos,(R('-') && R('>') && is_varlist(&v))){
            item.item_type=Markov; item.var1=v; convert_item_to_expr();
            cnt++;
        }
        restore_pos(oldpos);
    }
    must(cnt>=3,e_MARKOV);
    must(X_chr==0,e_EXTRA_TEXT);
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}
int parse_constraint(const char *str, int keep)
{int v1,v2;
    clear_entexpr();   /* clear the result space */
    no_new_id(NULL);   /* add new variables */
    if(!keep) id_table_idx=0; /* don't keep previous identifiers */
    init_parse(str);
    if(strchr(str,'=')==NULL && is_varlist(&v1)){ /* check for special constructs */
        if(R(':')){
              if(is_varlist(&v2)) return is_funcdep(v1,v2);
        } else if(R('.')){
              if(is_varlist(&v2)) return is_indep('.',v1,v2);
        } else if(R('|')){
              if(R('|')&& is_varlist(&v2)) return is_indep('|',v1,v2);
        } else if(R('/')){
              if(is_varlist(&v2)) return is_Markov('/',v1,v2);
        } else if(R('-')){
              if(R('>') && is_varlist(&v2)) return is_Markov('-',v1,v2);
        }
    }
    parse_entropyexpr(str,keep,expr_check);
    if(entropy_expr.n==0){
        harderr(entropy_expr.type==ent_eq? e_SIMPLIFIES_EQ : e_SIMPLIFIES_GE);
    }
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}

/***********************************************************************
* parse a macro definition. It start with the macro name, then comes
*    the argument list, an '=' sign, and then the macro text.
*
* int parse_macro_head(char *str, macro_head *head)
*    pares a macro head, and fill out the *head structure. Return value:
*    0   -- header parsed successfully
*    1   -- some error, syntax_error is filled
*
* int parse_delete_macro(char *str)
*    parse the macro header in str. Return value:
*    -1  -- some error, syntax_error is filled
*    >=0 -- the slot of the macro with this header.
*
* int parse_macro_definition(char *str)
*    parse and set up a new macro definition. Return value:
*    0   -- OK
*    1   -- some error, syntax_error is filled
*/
static int parse_macro_head(const char *str, struct macro_head_t *head)
{char name; int sepmask,argno; int done,var;
    clear_entexpr();       // clear result space 
    no_new_id(NULL);       // add new variables
    id_table_idx=0;        // don't keep old identifiers
    init_parse(str);
    name='\0';
    must(is_macro_name(&name) && R('('),e_MDEF_NAME);
    argno=0; sepmask=0;    // arguments, separator mask
    for(done=0;!done;){
        is_variable(&var); // variable is optional
        if(R(X_sep)){;}
        else if(R('|')){ sepmask |= 1<<argno; }
        else { must(R(')'),e_MDEF_PARSEP); done=1; }
        if(argno<minitip_MAX_ID_NO) argno++;
        else softerr(e_TOO_MANY_ARGS);
    }
    head->name=name; head->argno=argno; head->septype=sepmask;
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}

int parse_delete_macro(const char *str)
{struct macro_head_t head; int ret;
    parse_macro_head(str,&head);
    must(spy(0),e_EXTRA_TEXT);
    if(syntax_error.softerrstr||syntax_error.harderrstr) return -1;
    ret=find_macro(head,0);
    if(ret>=0) return ret;
    must(0,e_MDEL_NONE);
    return -1;
}

int parse_macro_definition(const char *str)
{int done,v,var,defpos; struct macro_head_t head;
    if(parse_macro_head(str,&head)) return 1; /* error in the head */
    v=find_macro(head,0);  // should not be defined
    must(v<0,v<4?e_MDEF_NOSTD:e_MDEF_DEFINED);
                           // we must have space
    if(macro_total >= max_macros-1) softerr(e_TOO_MANY_MACRO);
    if(syntax_error.softerrstr|| syntax_error.harderrstr) return 1;
    clear_entexpr();       // clear the result space
    no_new_id(NULL);       // add new variables
    id_table_idx=0;        // don't keep old identifiers
    init_parse(str);
    head.name='\0';
    must(is_macro_name(&head.name) && R('('),e_MDEF_NAME);
    head.argno=0; head.septype=0;  // arguments, separator mask
    for(done=0;!done;){
        must(is_variable(&var),e_MDEF_NOPAR);
        must(var==1<<head.argno,e_MDEF_SAMEPAR);
        if(R(X_sep)){;}
        else if(R('|')){ head.septype |= 1<<head.argno; }
        else { must(R(')'),e_MDEF_PARSEP); done=1; }
        if(head.argno<minitip_MAX_ID_NO) head.argno++;
    }
    must(R('='),e_MDEF_NOEQ);
    // if there was an error, don't proceed
    if (syntax_error.softerrstr|| syntax_error.harderrstr) return 1;
    defpos=X_pos; parse_entropyexpr(str+defpos,1,expr_macro);
    must(entropy_expr.n>0,e_MDEF_SIMP0);
    v=first_variable_not_used();
    if(0<=v && v<head.argno){ // variable var is not used
        init_parse(str);
        if(is_macro_name(&head.name)) R('(');
        while(v>=0){
           is_variable(&var); v--; while(v>=0 && (R('|')||R(X_sep)) && 0);
        }
        harderr(e_MDEF_UNUSED); /* don't adjust error position */
        return 1;
    }
    add_new_macro(head);
    adjust_error_position(defpos); /* if there was an error */
    return(syntax_error.softerrstr|| syntax_error.harderrstr)?1 : 0;
}


/** EOF **/
