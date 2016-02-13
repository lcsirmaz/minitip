/* mklp.c: minitip module constructing the lp problem */

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

#include <stdlib.h>
#include <stdio.h>
#include "minitip.h"
#include "mklp.h"
#include "parser.h"
#include "xassert.h"

/*----------------------------------------------------------------*/
static inline int mrandom(int v)
{ return v<=1 ? 0 : (random()&0x3fffffff)%v; }
static inline void perm_array(int len, int *arr)
{int i,j,t; /* keep arr[0], i goes from 1 */
   for(i=1;i<len-1;i++){
       j=i+mrandom(len-i);
       t=arr[i];arr[i]=arr[j];arr[j]=t;
   }
}
/*----------------------------------------------------------------*/
/* random variable preparation */
/* MAX_ID_NO should be >=26 (to handle all letters a--z),
                       <=31 (as vars a bits in an integer) */
static int 
/* these variables contain one bit for each variable */
  var_tr[minitip_MAX_ID_NO],	/* variable -> final value */
  var_opt[minitip_MAX_ID_NO],	/* variable optimization */
  var_all;			/* all variables */

static int var_no;		/* final number of variables */
/** LP structure **/
static int shannon;		/* number of shannon inequalities */
static int rows,cols;		/* number of rows and columns */
static int *rowperm;		/* permutation of rows */
static int *colperm;		/* permutation of columns */

/*---------------------------------------------------------*/
#include "glpk.h"
static glp_prob *P=NULL;	/* glpk structure */
static glp_smcp parm;		/* glpk parameters */

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* glp status and return codes */
static char glp_msg_buffer[60];
static char *glp_status_msg(int stat)
{static char *statmsg[] = {
"solution is undefined",        // GLP_UNDEF
"solution is feasible",         // GLP_FEAS
"solution is infeasible",       // GLP_INFEAS
"no feasible solution exists",  // GLP_NOFEAS
"solution is optimal",          // GLP_OPT
"solution is unbounded",        // GLP_UNBND
};
    if(1<=stat && stat<=(int)((sizeof(statmsg)/sizeof(char*))) )
       return statmsg[stat-1];
    sprintf(glp_msg_buffer,"unknown solution state %d", stat);
    return glp_msg_buffer;
}
static char *glp_return_msg(int retval)
{static char *retmsg[] = {
"invalid basis",                        // GLP_EBADB     *
"singular matrix",                      // GLP_ESING     *
"ill-conditioned matrix",               // GLP_ECOND     *
"invalid bounds",                       // GLP_EBOUND
"solver failed",                        // GLP_EFAIL     *
"objective lower limit reached",        // GLP_EOBJLL
"objective upper limit reached",        // GLP_EOBJUL
"iteration limit exceeded",             // GLP_EITLIM    *
"time limit exceeded",                  // GLP_ETMLIM    *
"no primal feasible solution",          // GLP_ENOPFS    *
"no dual feasible solution",            // GLP_ENODFS
"root LP optimum not provided",         // GLP_EROOT
"search terminated by application",     // GLP_ESTOP
"relative mip gap tolerance reached",   // GLP_EMIPGAP
"no primal/dual feasible solution",     // GLP_ENOFEAS
"no convergence",                       // GLP_ENOCVG
"numerical instability",                // GLP_EINSTAB
"invalid data",                         // GLP_EDATA
"result out of range",                  // GLP_ERANGE
};
    if(1<=retval && retval <= (int)((sizeof(retmsg)/sizeof(char*))))
        return retmsg[retval-1];
    sprintf(glp_msg_buffer,"unknown lp code %d", retval);
    return glp_msg_buffer;
}

/** create the initial glp problem, define the rows and columns **/
static void create_glp(void)
{int i;
    P=glp_create_prob();
    glp_add_cols(P,cols); glp_add_rows(P,rows);
    /* set the objective to all zero */
    for(i=0;i<=cols;i++)glp_set_obj_coef(P,i,0.0);
    glp_set_obj_dir(P,GLP_MIN); /* minize */
}
/* clean up all results */
static void release_glp(void){
    if(P){ glp_delete_prob(P); P=NULL; }
}
/* init glp parameters. These should be changeable some way */
static void init_glp_parameters(void)
{
    glp_init_smcp(&parm);
    parm.meth = GLP_DUAL;	// PRIMAL,DUAL,DUALP
    parm.msg_lev = GLP_MSG_ERR; // ORR,ON,ALL,ERR
    parm.pricing = GLP_PT_PSE;	// PSE,STD
    parm.r_test = GLP_RT_HAR;	// HAR,STD
    parm.it_lim = 80000;	// iteration limit
    parm.tm_lim = 10000;	// time limit 10 seconds
    parm.out_frq = 80000;	// output frequency
    parm.presolve = GLP_ON;	// presolve, helps on numerical instability
    glp_term_out(GLP_OFF);	// no terminal output
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* add a column to */
/** arrays to pass a column to glpk */
static int    row_idx[minitip_MAX_EXPR_LENGTH+1];
static double row_val[minitip_MAX_EXPR_LENGTH+1];

/* sort by idx; indices go 1<=i<=n */
static void sort_rowidx(int n) /* bubble sort */
{int OK;int i,t; double vt;
    OK=0; while(!OK){
        OK=1;
        for(i=1;i<n;i++) if(row_idx[i]>row_idx[i+1]){
           OK=0;
           t=row_idx[i];row_idx[i]=row_idx[i+1]; row_idx[i+1]=t;
           vt=row_val[i];row_val[i]=row_val[i+1];row_val[i+1]=vt;
        }
    }
}
/* add the next column. 
   col  = index of the column (1<=col<=cols)
   n    = number of entries in row_idx[],row_val[] (1<=i<=n)
   type = GLP_FR: free; GLP_LO: >=0 */
static void add_column(int col,int n,int type)
{   sort_rowidx(n);
    glp_set_col_bnds(P,col,type,0.0,0.0);
    glp_set_mat_col(P,col,n,row_idx,row_val);
}
/* add the goal, it is in row_idx,row_val[1..n] */
static void add_goal(int n)
{int i,in; double v;
    sort_rowidx(n);
    for(i=1,in=1;i<=rows;i++){
        if(in<=n && row_idx[in]==i){ v=row_val[in]; in++; }
        else { v=0.0; }
//        glp_set_row_bnds(P,i,GLP_UP,0.0,v);
        glp_set_row_bnds(P,i,GLP_FX,v,0.0);
    }
}

/*---------------------------------------------------------*/
/* variable optimization: merge variables which are always together */
static void init_var_assignment(void)
{int i; int v;
    v=0; for(i=0;i<minitip_MAX_ID_NO;i++) v|=(1<<i);
    for(i=0;i<minitip_MAX_ID_NO;i++) var_opt[i]=v;
    var_all=0; /* no final variables yet */
}
/* add_var(v) should be called for all variable sets involved.
   At the end, var_opt[i] has bits set which occur together in all
   variable sets. Bit 'i' is always set in var_opt[i]. */
inline static void add_var(int vv)
{int compvv = ~vv; int i,v;
    var_all |= vv;
    for(i=0,v=1;i<minitip_MAX_ID_NO;i++,v<<=1)
        var_opt[i] &= (v&vv) ? vv : compvv;
}
/* add variables in the entropy_expr structure */
static void add_expr_variables(void)
{int i;
    for(i=0;i<entropy_expr.n;i++){
       add_var(entropy_expr.item[i].var);
    }
}
/* this procedure computes the minimal set of variables and
   assigns them into var_tr. Computes the final number of rows and
   columns. Returns 1 if the number of variables is less than 2. */
static int do_variable_assignment(void)
{int i,v,j,vj,nextv;
    nextv=1; var_no=0;
    for(i=0;i<minitip_MAX_ID_NO;i++){
        var_tr[i]=0; /* nothing assigned yet */
        var_opt[i] &= var_all; /* keep only used variables */
    }
    for(i=0,v=1;i<minitip_MAX_ID_NO;i++,v<<=1){
        if(!(v&var_all)) continue; /* not used */
        if(var_tr[i]) continue; /* a value was assigned */
        /* assign a new value to this and all in the same group */
        for(j=i,vj=var_opt[i]>>i; vj; j++,vj>>=1) if(vj&1){
            var_tr[j]=nextv;
        }
        nextv<<=1; var_no++;
    }
    /* var_no:  0   1   2    3     4    n
       rows:    0   1   3    7    15    2^n-1
       shannon: 0   0   1    6    24    n(n-1)2^(n-3)
    */
    xassert(nextv == (1<<var_no) );
    if(var_no<2) return 1;
    rows = nextv-1;
    shannon = var_no<2 ? 0 : var_no < 3 ? 1 : var_no*(var_no-1)*(1<<(var_no-3));
    cols = shannon+var_no; /* plus the N-{i}<=N inequalities */
    return 0;
}
/* get the translated variable */
static int varidx(int v)
{int i,w;
    for(w=0,i=0;v;i++,v>>=1)if(v&1){ w|=var_tr[i]; }
    return rowperm[w];
}

/* create the idx-th shannon inequality
    the last var_no-2 bits give a subset, before it is the two extra vars
    don't do variable translation, use only the rowperm[] table
*/
static int add_shannon(int i,int idx)
{int v1,v2,v3; int mask;
    if(idx>=shannon+var_no) return 1;
    if(idx>=shannon){         // abcdef - abcd
        v1=(1<<var_no)-1;     // all variables
        v2= 1<<(idx-shannon); // single variables
        row_idx[1]=rowperm[v1];     row_val[1]=1.0;
        row_idx[2]=rowperm[v1&~v2]; row_val[2]=-1.0;
        add_column(i,var_no<2?1:2,GLP_LO);
        return 0;
    }
    xassert(var_no>=2);
    v2=idx>>(var_no-2); // determine v1 and v2
    v1=0; while(v2>v1){ v1++; v2-=v1; } v1++; // v2 < v1 
    // v1>v2; now create the subset from the rest of idx
    v1=1<<v1; v2=1<<v2;
    v3 = idx & (-1+(1<<(var_no-2)));
    // make zero at places v1 and v2 shifting the rest of v3
    mask = -1+v2; v3= (v3&mask) | ((v3&~mask)<<1);
    mask = -1+v1; v3= (v3&mask) | ((v3&~mask)<<1);
    row_idx[1]=rowperm[v1|v3];     row_val[1]=+1.0;
    row_idx[2]=rowperm[v2|v3];     row_val[2]=+1.0;
    row_idx[3]=rowperm[v1|v2|v3];  row_val[3]=-1.0;
    row_idx[4]=rowperm[v3];        row_val[4]=-1.0;
    add_column(i, v3==0?3:4, GLP_LO);
    return 0;
}

static void add_constraint(int col,int idx,int next_expr(int))
{int i,nextexpr;
    for(i=0;idx>=0;i++){
        nextexpr=next_expr(i);
        xassert(nextexpr==0);
        if(entropy_expr.type == ent_Markov){
            if(idx>entropy_expr.n-3){ idx -= entropy_expr.n-2; continue; }
            int v1,v2,v; int j;
            // add idx-th Markov equality
            v=v1=v2=0; for(j=0;j<entropy_expr.n;j++){
                if(j<idx+1) v1 |= entropy_expr.item[j].var;
                else if(j>idx+1) v2 |= entropy_expr.item[j].var;
                else v=entropy_expr.item[j].var;
            } // (v1,v2|v)=0
            row_idx[1]=varidx(v1|v);    row_val[1]=1.0;
            row_idx[2]=varidx(v2|v);    row_val[2]=1.0;
            row_idx[3]=varidx(v1|v2|v); row_val[3]=-1.0;
            row_idx[4]=varidx(v);       row_val[4]=-1.0;
            add_column(col,4,GLP_FR);
            return;
        } else if(idx>0) { idx--; continue; }
        else { // add this constraint
            int j;
            for(j=0;j<entropy_expr.n;j++){
                row_idx[j+1]=varidx(entropy_expr.item[j].var);
                row_val[j+1]=entropy_expr.item[j].coeff;
            }
            add_column(col,entropy_expr.n, entropy_expr.type==ent_eq?GLP_FR : GLP_LO);
            return;
        }
    }
    xassert(idx!=idx);
}

/* call the LP solver; mult is either +1.0 or -1.0 */
static char *invoke_lp(double mult, int next_expr(int))
{char *retval; int i,glp_res;
    create_glp(); // create a new glp instance
    for(i=0;i<entropy_expr.n;i++){
        row_idx[i+1]=varidx(entropy_expr.item[i].var);
        row_val[i+1]=mult*entropy_expr.item[i].coeff;
    }
    add_goal(entropy_expr.n); // right hand side value
    // go over the columns add them to the lp instance
    for(i=1;i<=cols;i++){
        int colct=colperm[i];
        if(add_shannon(i,colct)){ // this is aconstraint
            add_constraint(i,colct-(shannon+var_no),next_expr);
        }
    }
    /* call the lp */
    init_glp_parameters();
    if(parm.presolve!=GLP_ON) // generate the first basis
        glp_adv_basis(P,0);
    glp_res=glp_simplex(P,&parm);
    retval=NULL;
    switch(glp_res){
  case 0:           glp_res=glp_get_status(P); break;
  case GLP_ENOPFS:  // no primal feasible solution
                    glp_res=GLP_NOFEAS; break;
  default:          retval=glp_return_msg(glp_res);
    }
    if(!retval){
        retval = glp_res==GLP_OPT ? EXPR_TRUE :
                 glp_res==GLP_NOFEAS ? EXPR_FALSE :
                 glp_status_msg(glp_res);
    }
    release_glp();
    return retval;
}

char *call_lp(int next_expr(int))
{int i,constraints; expr_type_t goal_type; 
 char *retval, *retval2;
    /* initially the expression to be checked is in entropy_expr.
       determine first the variables */
    init_var_assignment(); /* start collecting variables */
    add_expr_variables();  /* variables in the expression to be checked */
    constraints=0;
    for(i=0;next_expr(i)==0;i++){ /* go over all constraints */
        constraints++;
         // Markov constraints give several cols
        if(entropy_expr.type==ent_Markov){
            constraints+=entropy_expr.n-3;
        }
        add_expr_variables();
    }
    /* figure out final variables, rows, cols, number of shannon */
    if(do_variable_assignment()){ // number of variables is less than 2
        return "number of final random variables is less than 2";
    }
    /* get memory for row and column permutation */
    cols += constraints;
    rowperm=malloc((rows+1)*sizeof(int));
    colperm=malloc((cols+1)*sizeof(int));
    if(!rowperm || !colperm){
        if(rowperm){ free(rowperm); rowperm=NULL; }
        if(colperm){ free(colperm); colperm=NULL; }
        return "the problem is too large, not enough memory";
    }
    for(i=0;i<=rows;i++) rowperm[i]=i;    perm_array(rows+1,rowperm);
    for(i=0;i<=cols;i++) colperm[i]= i-1; perm_array(cols+1,colperm);
    /* the expression to be checked, this will be the goal */
    if(constraints) next_expr(-1);
    goal_type=entropy_expr.type; // ent_eq, ent_ge
    retval=invoke_lp(1.0,next_expr);
    // call again with -1.0 when checking for ent_eq
    if(goal_type==ent_eq && (retval==EXPR_TRUE || retval==EXPR_FALSE)){
        next_expr(-1); // reload the problem
        retval2=invoke_lp(-1.0,next_expr);
        if(retval2==EXPR_TRUE){
            if(retval==EXPR_FALSE) retval=EQ_LE_ONLY;
        } else if(retval2==EXPR_FALSE){
            if(retval==EXPR_TRUE) retval=EQ_GE_ONLY;
        } else {
            retval=retval2;
        }
    }
    /* release allocated memory */
    if(rowperm){ free(rowperm); rowperm=NULL; }
    if(colperm){ free(colperm); colperm=NULL; }
    return retval;
}

/* EOF */

