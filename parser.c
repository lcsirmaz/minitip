/* parser.c: minitip parser module */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser.h"

/*------------------------------------------------------------*/
/* string under evaluation */
static char *X_str, X_chr; static int X_pos;
static syntax_style_t X_style;
/*------------------------------------------------------------*/
/* error structure */
struct syntax_error_t syntax_error;
static void softerr(char *err){
    if(!syntax_error.softerrstr){
      syntax_error.softerrstr=err; syntax_error.softerrpos=X_pos; }
}
static void harderr(char *err){
    if(!syntax_error.harderrstr){
      syntax_error.harderrstr=err; syntax_error.harderrpos=X_pos; }
}
#define must(cond,err)	\
    { if(!(cond)) harderr(err); }
/* the inline function evaluates the *err argument even if the 
   condition is false */
//inline static void must (int condition, char *err) 
//{   if(!condition)harderr(err); }

/* soft errors */
/* hack to convert numbers to strings */
#define STRINGIFY(x)		#x
#define N_TO_STRING(x)		STRINGIFY(x)

#define e_TOO_MANY_ID	"too many different random variables (max " N_TO_STRING(minitip_MAX_ID_NO) ")"
#define e_TOO_LONG_ID	"too long identifier (max length=" N_TO_STRING(minitip_MAX_ID_LENGTH) ")"
#define e_TOO_MANY_ITEMS "the expression is too long, cannot handle it (max items " N_TO_STRING(minitip_MAX_ITEMS) ")"
#define e_TOO_LONG_EXPR	"the expanded expression is too long (max " N_TO_STRING(minitip_MAX_EXPR_LENGTH) ")"
/* hard errors */
#define e_VAR_EXPECTED  "variable is expected after a comma (,)"
#define e_GREATER	"> symbol should be followed by ="
#define e_LESS		"< symbol should be followed by ="
#define e_INGLETONVAR	"in 'Ingleton' expression variable list is expected here"
#define e_INGLETONSEP	"in 'Ingleton' expression separator is expected here"
#define e_INGLETONCLOSE	"in 'Ingleton' expression closing ] is missing here"
#define e_DELTAVAR	"in 'Delta' expression variable list is expected here"
#define e_DELTAVARSEP	"in 'Delta' expression separator is expected here"
#define e_DELTACLOSE	"in 'Delta' expression closing ) is expected here"
#define e_CONDEXPR	"( should be followed by a variable list"
#define e_IEXPR2	"variable list is missing after | symbol"
#define e_CLOSING	"closing parenthesis ')' is expected here"
#define e_COMMA_OR_BAR	"either ',' or '|' is expected here"
#define e_VARLIST	"variable list is expected"
#define e_OPEN_AFTERH	"'H' must be followed by '('"
#define e_OPEN_AFTERI	"'I' must be followed by '('"
#define e_SEMICOLON	"a separator ';' is expected"

#define e_PLUSORMINUS	"either '+' or '-' is expected here"
#define e_DOUBLE_REL	"only one relation is allowed in an expression"
#define e_DIFF_USEEQ	"use '=' to separate the expressions (diff)"
#define e_WRONGITEM	"unrecognized character"
#define e_NOHOMOGEN	"constant before or after the relation sign must be zero"
#define e_WRONGAST	"multiplication symbol '*' at a wrong place"
#define e_EXTRA_TEXT	"extra characters at the end"
#define e_EMPTY		"no expression is given"
#define e_NORELATION	"there must be an '=', '<=' or '>=' somewhere"
#define e_NORHS		"no expression after '=', '<=' or '>=' "
#define e_EXTRANUM	"no constants allowed"
#define e_FUNC_EQUAL	"the first variable set is always a function of other"
#define e_ALLZERO	"all coefficients are zero"
#define e_MARKOV	"a Markov chain must contain at least three tags"
#define e_INTERNAL	"internal error, should not occur..."

#define e_SIMPLIFIES_EQ	"the expression simplifies to '0=0', thus it is always TRUE"
#define e_SIMPLIFIES_GE	"the expression simplifies to '0<=0', thus it is always TRUE"
#define e_POSCOMBINATION "the expression is TRUE as a positive combination of entropy values"
static char* e_FUNCTIONOF(int i){
/* sometimes it is called even if should not, and overwrites the static 
   buffer. This is why we check that it is a valid request */
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

/*------------------------------------------------------------------*/
/* set the style */
#define SIMPLE		syntax_short
#define ORIGINAL	syntax_full
void set_syntax_style(syntax_style_t style){ X_style=style; }
/*------------------------------------------------------------------*/
/* the entropy expression is stored here */

typedef enum {
   equal,	/* = */
   greater,	/* >= */
   less,	/* <= */
   zero,	/* 0.0 */
   H1, 		/* entropy of a single variable */
   H2,		/* (a|b) */
   I2,		/* (a,b) */
   I3,		/* (a,b|c)*/
   Ing,		/* [a,b,c,d], Ingleton */
   Delta,	/* D(a,b,c), delta function */
   Func,	/* a:b  a is a function of b: ab=b */
   Indep,	/* a.b or a||b (independent); at least two  */
   Markov,	/* a / b / c  or a->b->c; at least three.. */
} item_type_t;

typedef struct {	/* item structure */
  item_type_t	item_type;
  double	multiplier;
  int		var1, var2, var3, var4;
} ITEM;

static ITEM item_list[minitip_MAX_ITEMS];
static int next_item;	/* index of next empty item in the list */

inline static void advance_item(void){
    next_item++;
    if(next_item>=minitip_MAX_ITEMS){
        softerr(e_TOO_MANY_ITEMS);
        next_item--;
    }
}
/*-----------------------------------------------------------------*/
/* identifier handling */

struct { char id[minitip_MAX_ID_LENGTH+1]; }id_table [minitip_MAX_ID_NO+1];
static int id_table_idx=0; /* next empty slot */

static int search_id(const char *var)
{int i;
    for(i=0;i<id_table_idx;i++){
        if(strcmp(id_table[i].id,var)==0) return i;
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
/* return the character representation of the variable list
   in one of the two static slots */
#define MAX_REPR_LENGTH 201	/* longer list is not understandable; >= 26 */
static char *get_idlist_repr(int v, int slotno)
{static char slot1[MAX_REPR_LENGTH+2], slot2[MAX_REPR_LENGTH+2];
 char *slot; const char *var; int i,j;
    slot= slotno==1 ? slot1 : slot2;
    for(i=0,j=0;v!=0;i++,v>>=1){
        if(v&1){
            if(X_style==SIMPLE){ /* simple case */
                slot[j]='a'+i; j++;
            } else {
                if(j>0){ slot[j]=',';  /* comma separated list */
                         if(j<MAX_REPR_LENGTH) j++;
                }
                for(var=i<id_table_idx ? id_table[i].id : "?";*var;var++){
                    slot[j]=*var;
                    if(j<MAX_REPR_LENGTH)j++;
                }
            }
        }
    }
    slot[j]=0; return slot;
}
/*-----------------------------------------------------------------*/
/* generating matrix columns */
struct entropy_expr_t entropy_expr;

#define ee_n	entropy_expr.n
#define ee_item	entropy_expr.item
#define ee_type entropy_expr.type
/* search for a variable list */
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
/* convert item_list[] into an entropy expression >=0 or ==0 */
static int fill_expr(int diff)
{int i,j,v; int where; double d;
    if(syntax_error.softerrstr || syntax_error.harderrstr) return 1;
    switch(item_list[0].item_type){ /* special cases */
  case Func:	/* v1 - v2 = 0 */
      ee_type=ent_eq;
      ee_n=2;
      ee_item[0].var=item_list[0].var1;
      ee_item[1].var=item_list[0].var2;
      ee_item[0].coeff=+1.0;
      ee_item[1].coeff=-1.0;
      return 0;
  case Indep: /* v1+v2+...+ vn - \cup v_i = 0 */ 
      ee_type=ent_eq;
      ee_n=next_item+1;
      v=0;
      for(i=0;i<next_item;i++){
          ee_item[i].var=item_list[i].var1;
          v |= item_list[i].var1;
          ee_item[i].coeff=+1.0;
      }
      ee_item[i].var=v;
      ee_item[i].coeff=-1.0;
      return 0;
  case Markov:
      ee_type=ent_Markov;
      ee_n=next_item;
      for(i=0;i<next_item;i++){
          ee_item[i].var=item_list[i].var1;
          ee_item[i].coeff=+1.0;
      }
      return 0;
  default:
      break;
    }
    // put all variables together ...
    // where==0: don't; where==1: negate the coeff
    ee_n=0; where=0;
    for(i=0;i<next_item;i++){
       d=item_list[i].multiplier; if(where) d=-d;
       switch(item_list[i].item_type){
  case zero:	/* skip it, do nothing */
       break;
  case equal:	/* negate subsequent terms */
       ee_type=ent_eq; where =1; break;
  case less:	/* negate existing terms */
       for(j=0;j<ee_n;j++){ ee_item[j].coeff *= -1.0; }
       ee_type=ent_ge; break;
  case greater:	/* negate subsequent terms */
       where=1; ee_type=ent_ge; break;
  case H1:	/* entropy of a single variable */
       ee_add(item_list[i].var1,d); break;
  case H2:	/* (a|b) */
       ee_subtr(item_list[i].var2,d);
       ee_add(item_list[i].var1|item_list[i].var2,d); break;
  case I2:	/* (a,b) */
       ee_i2(item_list[i].var1,item_list[i].var2,d); break;
  case I3:	/* (a,b|c)*/
       ee_i3(item_list[i].var1,item_list[i].var2,item_list[i].var3,d);
       break;
  case Ing:	/* [a,b,c,d], Ingleton  -(a,b)+(a,b|c)+(a,b|d)+(c,d) */
       ee_i2(item_list[i].var1,item_list[i].var2,-d);
       ee_i2(item_list[i].var3,item_list[i].var4,d);
       ee_i3(item_list[i].var1,item_list[i].var2,item_list[i].var3,d);
       ee_i3(item_list[i].var1,item_list[i].var2,item_list[i].var4,d);
       break;
   case Delta: 	/* D(a,b,c), Delta: (a,b|c)+(b,c|a)+(c,a|b) */
       ee_i3(item_list[i].var1,item_list[i].var2,item_list[i].var3,d);
       ee_i3(item_list[i].var2,item_list[i].var3,item_list[i].var1,d);
       ee_i3(item_list[i].var3,item_list[i].var1,item_list[i].var2,d);
       break;
   default:
       must(0,e_INTERNAL);
    }}
    // merge zero coeffs
    for(i=0,j=0;j<ee_n;j++){
        if(ee_item[j].coeff>1.5e-10 || ee_item[j].coeff<-1.5e-10){
            if(i!=j){ee_item[i].var=ee_item[j].var;
                     ee_item[i].coeff=ee_item[j].coeff; }
            i++;
        } else {
           ee_item[j].coeff=0.0;
        }
    }
    ee_n=i;
    if(diff==0){
      if(ee_n==0){ /* nothing remained */
        harderr(ee_type==ent_eq ? e_SIMPLIFIES_EQ : e_SIMPLIFIES_GE);
      } else if(ee_type==ent_ge){ /* all coeffs are >=0 */
        where=1; for(i=0;i<ee_n;i++)if(ee_item[i].coeff<0.0) where=0;
        must(where==0,e_POSCOMBINATION);
      }
    }
    return (syntax_error.softerrstr|| syntax_error.harderrstr) ? 1 : 0;
}
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
        if(X_style==SIMPLE){
            printf("%s",get_idlist_repr(ee_item[i].var,1));
        } else {
            printf("H(%s)",get_idlist_repr(ee_item[i].var,1));
        }
    }
}

#undef ee_n
#undef ee_item
#undef ee_type
/*-------------------------------------------------------------*/
/* parsing routines */
/* next_chr()    advances the position to the next non-space char
   restore_pos() restore the old (saved) position
   init_parse()  set up all variables for parsing
   R(<c>)        true, if the next char is <c>, and advances
   spy(<c>)      true if the next char is <c>, but does not advance
   is_digit(&v)  true if the next char is digit, store the value
   is_number(&v) always sets v; true and advances if it is a decimal number
   is_signed_number(&v) always sets v; reads a signed number
   is_variable(v) read the next variable into v
   is_varlist(v) reads a list of variables into v
   is_relation(v) checks if this is a '=', '<=' or '>='
   is_Ingletion() stores an Ingleton form in the next item
   is_Delta()
   is_simple_expression()
   is_orig_expression()
                  stores an entropy expression in the next item

   init_parse(char*str) sets 'str' the string to be parsed. It resets
       errors, but does not reset the variable storage to be handled
       with conditions.
*/
static inline void next_chr(void)
{  X_pos++; while((X_chr=X_str[X_pos])==' '|| X_chr=='\t') X_pos++; }
static inline void next_idchr(void)
{  X_pos++; X_chr=X_str[X_pos]; }
static inline void skip_to_visible(void)
{ while((X_chr=X_str[X_pos])==' '||X_chr=='\t') X_pos++; }
static inline void restore_pos(int oldpos)
{  X_pos=oldpos; X_chr=X_str[X_pos]; }
static void init_parse(char *str){
    X_str=str; X_pos=-1; next_chr();
    syntax_error.softerrstr=NULL; 
    syntax_error.harderrstr=NULL; /* no errors yet */
}
static inline int R(char c){
    if(X_chr==c){ next_chr(); return 1;}
    return 0;
}
// static inline int spy(char c){ return X_chr==c; }
#define spy(c)	X_chr==(c)
static inline int is_digit(int *v){
    if('0'<= X_chr && X_chr <='9'){ *v=X_chr-'0';  next_chr(); return 1;}
    return 0;
}
/* v is assumed to have a value set. It reads a decimal point,
   and a sequence of digits; adjust the value in v */ 
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
/* decimal number: either \d+ or  \d*[.]\d+ */
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
/* signed number: a sign, a number, or a sign followed by a number
   even if returns FALSE, the argument v is set.
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
/* is_variable(v) returns v with a single bit set corresponding to
   the variable. It sets v to zero if no variable was found */
inline static int spy_letgit(void){
    return ('a'<=X_chr && X_chr<='z') ||
           ('A'<=X_chr && X_chr<='Z') ||
           ('0'<=X_chr && X_chr<='9') || X_chr=='_'; }
static int is_variable(int *v)
{char var[minitip_MAX_ID_LENGTH+1]; int i;
    *v=0;
    if(X_style==SIMPLE){
        if('a'<=X_chr && X_chr<='z'){
            *v = 1<<(X_chr-'a'); next_chr(); return 1;
        }
        return 0;
    }
    // no intervening space characters ...
    if( ('a'<= X_chr && X_chr <='z') ||
        ('A'<= X_chr && X_chr <='Z')){
        var[0]=X_chr; i=0; next_idchr();
        while( spy_letgit() ){
            i++; if(i>=minitip_MAX_ID_LENGTH-1){
                i--; softerr(e_TOO_LONG_ID);
            } else {var[i]=X_chr; }
            next_idchr();
        }
        i++; var[i]=0;
        skip_to_visible();
        *v = 1<< search_id(var);
        return 1;
    }
    return 0;
}
/* sequence of variables: comma separated list for ORIGINAL style */
static int is_varlist(int *v)
{int j;
    if(is_variable(v)){
        if(X_style==SIMPLE){
            while(is_variable(&j)){ *v |= j; }
        } else {
            while(R(',')){
                must(is_variable(&j),e_VAR_EXPECTED);
                *v |= j;
            }
        }
        return 1;
    }
    return 0;
}
/* check if the next symbol is =, >=, or <= */
static int is_relation(item_type_t *v){
    if(R('=')){ *v = equal; return 1; }
    if(R('>')){ must(R('='),e_GREATER); *v=greater; return 1; }
    if(R('<')){ must(R('='),e_LESS);    *v=less; return 1; }
    return 0;
}
/* if it is an Ingleton expression, put it into item_list[next_item] */
static int is_Ingleton(void)
{char sep; /* the separator character */
    sep = X_style==SIMPLE ? ',' : ';';
    if(R('[')){
        item_list[next_item].item_type=Ing;
        must(is_varlist(&item_list[next_item].var1),e_INGLETONVAR);
        must(R(sep),e_INGLETONSEP);
        must(is_varlist(&item_list[next_item].var2),e_INGLETONVAR);
        must(R(sep),e_INGLETONSEP);
        must(is_varlist(&item_list[next_item].var3),e_INGLETONVAR);
        must(R(sep),e_INGLETONSEP);
        must(is_varlist(&item_list[next_item].var4),e_INGLETONVAR);
        must(R(']'),e_INGLETONCLOSE);
        return 1;
    }
    return 0;
}
static int is_Delta(void)
{int oldpos; char sep; /* separator character */
    sep = X_style==SIMPLE ? ',':';';
    oldpos=X_pos;
    if(R('D') && R('(')){
        item_list[next_item].item_type=Delta;
        must(is_varlist(&item_list[next_item].var1),e_DELTAVAR);
        must(R(sep),e_DELTAVARSEP);
        must(is_varlist(&item_list[next_item].var2),e_DELTAVAR);
        must(R(sep),e_DELTAVARSEP);
        must(is_varlist(&item_list[next_item].var3),e_DELTAVAR);
        must(R(')'),e_DELTACLOSE);
        return 1;
    }
    restore_pos(oldpos);
    return 0;
}
/* add next expression to item_list[next_item] */
static int is_simple_expression(void)
{   if(R('(')){ /* (a,b)  (a|b)  (a,b|c) */
        must(is_varlist(&item_list[next_item].var1),e_CONDEXPR);
        if(R('|')){
            item_list[next_item].item_type=H2;
            must(is_varlist(&item_list[next_item].var2),e_IEXPR2);
            must(R(')'),e_CLOSING);
        } else {
            item_list[next_item].item_type=I2;
            must(R(','),e_COMMA_OR_BAR);
            must(is_varlist(&item_list[next_item].var2),e_VARLIST);
            if(R('|')){
                item_list[next_item].item_type=I3;
                must(is_varlist(&item_list[next_item].var3),e_VARLIST);
            }
            must(R(')'),e_CLOSING);
        }
        return 1;
    }
    if(is_varlist(&item_list[next_item].var1)){
        item_list[next_item].item_type=H1;
        return 1;
    }
    return 0;
}
static int is_orig_expression(void)
{   if(R('H')){ /* H(a,a), H(a,a|b), I(a,a;b), I(a,a;b|c) */
        item_list[next_item].item_type=H1;
        must(R('('),e_OPEN_AFTERH);
        must(is_varlist(&item_list[next_item].var1),e_VARLIST);
        if(R('|')){
            item_list[next_item].item_type=H2;
            must(is_varlist(&item_list[next_item].var2),e_VARLIST);
        }
        must(R(')'),e_CLOSING);
        return 1;
    }
    if(R('I')){
        item_list[next_item].item_type=I2;
        must(R('('),e_OPEN_AFTERI);
        must(is_varlist(&item_list[next_item].var1),e_VARLIST);
        must(R(';'),e_SEMICOLON);
        must(is_varlist(&item_list[next_item].var2),e_VARLIST);
        if(R('|')){
            item_list[next_item].item_type=I3;
            must(is_varlist(&item_list[next_item].var3),e_VARLIST);
        }
        must(R(')'),e_CLOSING);
        return 1;
    }
    return 0;
}
/*-------------------------------------------------------------*/
/* parse an entropy string passed as an argument.
   Return value: 0: OK;
   1: some error occurred, syntax_error is filled
   It stores the sequence in item_list[] as well.
   When keep==0 clear the id table, otherwise keep it.
*/
#define W_start 0	/* we are at the start */
#define W_bexpr 1	/* after entropy expression before relation */
#define W_rel   2	/* just after =, >=, <= */
#define W_aexpr 3	/* after entropy expression after a relation */
static int parse_entropyexpr(char *str, int keep, int diff)
{int where;     /* where we are */
 double coeff;  /* the coeff for the next symbol */
 int i,iscoeff; item_type_t relsym;
    next_item=0;      /* empty the list initially*/
    where=W_start;    /* now we are in an initial position */
    if(!keep) id_table_idx=0; /* don't keep identifiers */
    init_parse(str); /* no errors yet */
    while(X_chr && !syntax_error.harderrstr){ /* go for the first error only */
       if(where != W_start && is_relation(&relsym) ){
           must(where==W_bexpr,e_DOUBLE_REL);
           where=W_rel;
           item_list[next_item].item_type=relsym;
           advance_item();
           if(diff) must(relsym==equal,e_DIFF_USEEQ);
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
          (X_style==ORIGINAL && is_orig_expression()) ||
          is_Ingleton() || is_Delta() ){
          item_list[next_item].multiplier=coeff;
          if(where==W_start) where=W_bexpr;
          if(where==W_rel) where=W_aexpr;
          if(coeff!=0.0) advance_item();
          continue;
       }
       must(iscoeff!=2,e_WRONGAST);
       if(where==W_start){
          must(iscoeff,e_WRONGITEM);
          must(is_relation(&relsym),e_WRONGITEM);
          must(coeff==0.0,e_NOHOMOGEN);
          item_list[next_item].item_type=zero;
          advance_item();
          item_list[next_item].item_type=relsym;
          advance_item();
          where=W_rel;
          continue;
       }
       if(where==W_rel){
          must(iscoeff,e_WRONGITEM);
          must(coeff==0.0,e_NOHOMOGEN);
          item_list[next_item].item_type=zero;
          advance_item();
          where=W_aexpr; // and this must be the last
          must(X_chr==0,e_EXTRA_TEXT);
          continue;
       }
       must(iscoeff==0,e_EXTRANUM);
       must(X_chr==0,e_EXTRA_TEXT);
    }
    must(where!=W_start,e_EMPTY);
    must(where!=W_bexpr,e_NORELATION);
    must(where!=W_rel, e_NORHS);
    /* check that not all is zero */
    where=0;
    for(i=0;i<next_item;i++)switch(item_list[i].item_type){
        case equal: case greater: case less: case zero: break;
        default: 
           if(item_list[i].multiplier>1.5e-10 ||
              item_list[i].multiplier<-1.5e-10){  where=1; }
           else { item_list[i].multiplier = 0.0; }
    }
    must(where,e_ALLZERO);
    return fill_expr(diff);
}
#undef W_start
#undef W_bexpr
#undef W_rel
#undef W_aexpr

int parse_entropy (char *str, int keep)
{    return parse_entropyexpr(str,keep,0); }

/*-------------------------------------------------------------*/
/* parse a constrain. It can be:
   a) <v1> : <v2>        -- <v1> is a function of <v2>
   b) <v1> . <v2> .      -- they are independent
      <v1> || <v2> ||
   c) <v1> / <v2> /      -- Markov chain
      <v1> -> <v2> -> 
   d) if none of these, then an entropy expression
*/
static int is_func(int v1,int v2){
    item_list[next_item].item_type=Func;
    v1|=v2;
    must(v1!=v2,e_FUNC_EQUAL);
    item_list[next_item].var1=v1;
    item_list[next_item].var2=v2;
    advance_item();
    must(X_chr==0,e_EXTRA_TEXT);
    return fill_expr(0);
}
static int is_indep(char sep, int v1,int v2)
{int v; int oldpos; int i,j;
    item_list[next_item].item_type=Indep;
    item_list[next_item].var1=v1;
    advance_item();
    item_list[next_item].item_type=Indep;
    item_list[next_item].var1=v2;
    advance_item();
    if(sep=='.'){
        while(oldpos=X_pos,(R('.')&& is_varlist(&v))){
            item_list[next_item].item_type=Indep;
            item_list[next_item].var1=v;
            advance_item();
        }
        restore_pos(oldpos);
    } else {
        while(oldpos=X_pos,(R('|')&& R('|')&& is_varlist(&v))){
            item_list[next_item].item_type=Indep;
            item_list[next_item].var1=v;
            advance_item();
        }
        restore_pos(oldpos);
    }
    must(X_chr==0,e_EXTRA_TEXT);
    // check if none is a subset of the others
    for(i=0;i<next_item;i++){
        v=0;
        for(j=0;j<next_item;j++) if(i!=j) v |= item_list[j].var1;
        must(v!=(v|item_list[i].var1),e_FUNCTIONOF(i));

    }
    return fill_expr(0);
}
static int is_Markov(char sep, int v1,int v2)
{int v,cnt; int oldpos;
    item_list[next_item].item_type=Markov;
    item_list[next_item].var1=v1;
    advance_item();
    item_list[next_item].item_type=Markov;
    item_list[next_item].var1=v2;
    advance_item();
    cnt=2;
    if(sep=='/'){
        while(oldpos=X_pos,(R('/')&& is_varlist(&v))){
            item_list[next_item].item_type=Indep;
            item_list[next_item].var1=v;
            cnt++; advance_item();
        }
        restore_pos(oldpos);
    } else {
        while(oldpos=X_pos,(R('-')&& R('>')&& is_varlist(&v))){
            item_list[next_item].item_type=Indep;
            item_list[next_item].var1=v;
            cnt++; advance_item();
        }
        restore_pos(oldpos);
    }
    must(cnt>=3,e_MARKOV);
    must(X_chr==0,e_EXTRA_TEXT);
    return fill_expr(0);
}
int parse_constraint(char *str, int keep)
{int v1,v2;
     next_item=0;      /* empty the list */
     init_parse(str);  /* no errors yet */
     if(!keep) id_table_idx=0;
     if(is_varlist(&v1)){
          if(R(':')){
              if(is_varlist(&v2)) return is_func(v1,v2);
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
     return parse_entropyexpr(str,keep,0);
}
int parse_diff(char *str)
{   return parse_entropyexpr(str,0,1); }


/** EOF **/
